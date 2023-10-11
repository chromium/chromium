// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_search/visual_search_classifier_agent.h"

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/companion/visual_search.mojom.h"
#include "chrome/common/companion/visual_search/features.h"
#include "chrome/renderer/companion/visual_search/visual_search_classification_and_eligibility.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace companion::visual_search {

namespace {

using optimization_guide::proto::EligibilitySpec;
using optimization_guide::proto::FeatureLibrary;
using optimization_guide::proto::OrOfThresholdingRules;
using optimization_guide::proto::ThresholdingRule;

using DOMImageList = base::flat_map<ImageId, SingleImageFeaturesAndBytes>;

EligibilitySpec CreateEligibilitySpec(std::string config_proto) {
  EligibilitySpec eligibility_spec;

  if (!config_proto.empty()) {
    eligibility_spec.ParseFromString(config_proto);
    if (!eligibility_spec.has_additional_cheap_pruning_options()) {
      eligibility_spec.mutable_additional_cheap_pruning_options()
          ->set_z_index_overlap_fraction(0.85);
    }
  } else {
    // This is the default configuration if a config is not provided.
    auto* new_rule = eligibility_spec.add_cheap_pruning_rules()->add_rules();
    new_rule->set_feature_name(FeatureLibrary::IMAGE_VISIBLE_AREA);
    new_rule->set_normalizing_op(FeatureLibrary::BY_VIEWPORT_AREA);
    new_rule->set_thresholding_op(FeatureLibrary::GT);
    new_rule->set_threshold(0.01);
    new_rule = eligibility_spec.add_cheap_pruning_rules()->add_rules();
    new_rule->set_feature_name(FeatureLibrary::IMAGE_FRACTION_VISIBLE);
    new_rule->set_thresholding_op(FeatureLibrary::GT);
    new_rule->set_threshold(0.45);
    new_rule = eligibility_spec.add_cheap_pruning_rules()->add_rules();
    new_rule->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_WIDTH);
    new_rule->set_thresholding_op(FeatureLibrary::GT);
    new_rule->set_threshold(100);
    new_rule = eligibility_spec.add_cheap_pruning_rules()->add_rules();
    new_rule->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_HEIGHT);
    new_rule->set_thresholding_op(FeatureLibrary::GT);
    new_rule->set_threshold(100);
    new_rule = eligibility_spec.add_post_renormalization_rules()->add_rules();
    new_rule->set_feature_name(FeatureLibrary::IMAGE_VISIBLE_AREA);
    new_rule->set_normalizing_op(FeatureLibrary::BY_MAX_VALUE);
    new_rule->set_thresholding_op(FeatureLibrary::GT);
    new_rule->set_threshold(0.5);
    auto* shopping_rule =
        eligibility_spec.add_classifier_score_rules()->add_rules();
    shopping_rule->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
    shopping_rule->set_thresholding_op(FeatureLibrary::GT);
    shopping_rule->set_threshold(0.5);
    auto* sensitivity_rule =
        eligibility_spec.add_classifier_score_rules()->add_rules();
    sensitivity_rule->set_feature_name(FeatureLibrary::SENS_CLASSIFIER_SCORE);
    sensitivity_rule->set_thresholding_op(FeatureLibrary::LT);
    sensitivity_rule->set_threshold(0.5);
    eligibility_spec.mutable_additional_cheap_pruning_options()
        ->set_z_index_overlap_fraction(0.85);
  }

  return eligibility_spec;
}

// Depth-first search for recursively traversing DOM elements and pulling out
// references for images (SkBitmap).
void FindImageElements(blink::WebElement element,
                       std::vector<blink::WebElement>& images) {
  if (element.ImageContents().isNull()) {
    for (blink::WebNode child = element.FirstChild(); !child.IsNull();
         child = child.NextSibling()) {
      if (child.IsElementNode()) {
        FindImageElements(child.To<blink::WebElement>(), images);
      }
    }
  } else {
    if (element.HasAttribute("src")) {
      images.emplace_back(element);
    }
  }
}

// Top-level wrapper call to trigger DOM traversal to find images.
DOMImageList FindImagesOnPage(content::RenderFrame* render_frame) {
  DOMImageList images;
  std::vector<blink::WebElement> image_elements;
  const blink::WebDocument doc = render_frame->GetWebFrame()->GetDocument();
  if (doc.IsNull() || doc.Body().IsNull()) {
    return images;
  }
  FindImageElements(doc.Body(), image_elements);

  int image_counter = 0;
  for (auto& element : image_elements) {
    std::string alt_text;
    if (element.HasAttribute("alt")) {
      alt_text = element.GetAttribute("alt").Utf8();
    }
    ImageId id = base::NumberToString(image_counter++);
    images[id] = {
        VisualClassificationAndEligibility::ExtractFeaturesForEligibility(
            id, element),
        element.ImageContents(), alt_text};
  }

  return images;
}

ClassificationResultsAndStats ClassifyImagesOnBackground(
    DOMImageList images,
    std::string model_data,
    std::string config_proto,
    gfx::SizeF viewport_size) {
  ClassificationResultsAndStats results;
  const auto classifier = VisualClassificationAndEligibility::Create(
      model_data, CreateEligibilitySpec(config_proto));

  if (classifier == nullptr) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "Companion.VisualQuery.Agent.ClassifierCreationFailure", true);
    return results;
  }

  auto classifier_results =
      classifier->RunClassificationAndEligibility(images, viewport_size);

  const auto& metrics = classifier->classification_metrics();
  results.second =
      mojom::ClassificationStats::New(mojom::ClassificationStats());
  results.second->eligible_count = metrics.eligible_count;
  results.second->shoppy_count = metrics.shoppy_count;
  results.second->sensitive_count = metrics.sensitive_count;
  results.second->shoppy_nonsensitive_count = metrics.shoppy_nonsensitive_count;
  results.second->results_count = metrics.result_count;

  int result_counter = 0;
  int maxNumberResults = features::MaxVisualSuggestions();
  for (const auto& image_id : classifier_results) {
    results.first.emplace_back(images[image_id]);
    if (++result_counter >= maxNumberResults) {
      break;
    }
  }
  return results;
}

}  // namespace

VisualSearchClassifierAgent::VisualSearchClassifierAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  if (render_frame) {
    render_frame_ = render_frame;
    render_frame->GetAssociatedInterfaceRegistry()
        ->AddInterface<mojom::VisualSuggestionsRequestHandler>(
            base::BindRepeating(
                &VisualSearchClassifierAgent::OnRendererAssociatedRequest,
                base::Unretained(this)));
  }
}

VisualSearchClassifierAgent::~VisualSearchClassifierAgent() = default;

// static
VisualSearchClassifierAgent* VisualSearchClassifierAgent::Create(
    content::RenderFrame* render_frame) {
  return new VisualSearchClassifierAgent(render_frame);
}

void VisualSearchClassifierAgent::StartVisualClassification(
    base::File visual_model,
    const std::string& config_proto,
    mojo::PendingRemote<mojom::VisualSuggestionsResultHandler> result_handler) {
  DOMImageList dom_images = FindImagesOnPage(render_frame_);

  // We check to see if we have found any images in the DOM, if there are no
  // images, we use that as a strong signal that we traversed the DOM
  // prematurely, so we try again after 2 seconds. We use the |is_retrying_|
  // boolean to ensure that we only do this once.
  // TODO(b/294900101) - Remove this first attempt for more robust heuristic.
  if (dom_images.size() == 0 && !is_retrying_) {
    base::UmaHistogramBoolean("Companion.VisualQuery.Agent.StartClassification",
                              false);
    is_retrying_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&VisualSearchClassifierAgent::StartVisualClassification,
                       weak_ptr_factory_.GetWeakPtr(), std::move(visual_model),
                       std::move(config_proto), std::move(result_handler)),
        features::StartClassificationRetryDuration());
    return;
  }
  base::UmaHistogramBoolean("Companion.VisualQuery.Agent.StartClassification",
                            true);
  if (result_handler.is_valid()) {
    result_handler_.reset();
    result_handler_.Bind(std::move(result_handler));
  }

  ClassificationResultsAndStats empty_results;

  if (is_classifying_) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "Companion.VisualSearch.Agent.OngoingClassificationFailure",
        is_classifying_);
    OnClassificationDone(std::move(empty_results));
    return;
  }

  if (!visual_model.IsValid()) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.InvalidModelFailure",
                            !visual_model.IsValid());
    OnClassificationDone(std::move(empty_results));
    return;
  }

  if (!visual_model_.IsValid() &&
      !visual_model_.Initialize(std::move(visual_model))) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.InitModelFailure",
                            true);
    OnClassificationDone(std::move(empty_results));
    return;
  }

  is_classifying_ = true;
  std::string model_data =
      std::string(reinterpret_cast<const char*>(visual_model_.data()),
                  visual_model_.length());
  base::UmaHistogramCounts100("Companion.VisualQuery.Agent.DomImageCount",
                              dom_images.size());

  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  gfx::SizeF viewport_size = frame->View()->VisualViewportSize();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ClassifyImagesOnBackground, std::move(dom_images),
                     std::move(model_data), std::move(config_proto),
                     viewport_size),
      base::BindOnce(&VisualSearchClassifierAgent::OnClassificationDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VisualSearchClassifierAgent::OnClassificationDone(
    ClassificationResultsAndStats results) {
  is_classifying_ = false;
  is_retrying_ = false;
  std::vector<mojom::VisualSearchSuggestionPtr> final_results;
  for (auto& result : results.first) {
    final_results.emplace_back(mojom::VisualSearchSuggestion::New(
        result.image_contents, result.alt_text));
  }

  mojom::ClassificationStatsPtr stats;
  if (results.second.is_null()) {
    stats = mojom::ClassificationStats::New(mojom::ClassificationStats());
  } else {
    stats = std::move(results.second);
  }

  if (result_handler_.is_bound()) {
    result_handler_->HandleClassification(std::move(final_results),
                                          std::move(stats));
  }
  LOCAL_HISTOGRAM_COUNTS_100("Companion.VisualQuery.Agent.ClassificationDone",
                             results.first.size());
}

void VisualSearchClassifierAgent::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::VisualSuggestionsRequestHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void VisualSearchClassifierAgent::DidFinishLoad() {
  if (!features::IsVisualSearchSuggestionsAgentEnabled()) {
    return;
  }
  if (!render_frame_ || !render_frame_->IsMainFrame() ||
      model_provider_.is_bound()) {
    return;
  }

  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      model_provider_.BindNewPipeAndPassReceiver());

  if (model_provider_.is_bound()) {
    model_provider_->GetModelWithMetadata(
        base::BindOnce(&VisualSearchClassifierAgent::HandleGetModelCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    LOCAL_HISTOGRAM_BOOLEAN(
        "Companion.VisualQuery.Agent.ModelRequestSentSuccess", true);
  }
}

void VisualSearchClassifierAgent::HandleGetModelCallback(
    base::File file,
    const std::string& config) {
  // Now that we have the result, we can unbind and reset the receiver pipe.
  model_provider_.reset();

  mojo::PendingRemote<mojom::VisualSuggestionsResultHandler> result_handler;
  StartVisualClassification(std::move(file), config, std::move(result_handler));
}

void VisualSearchClassifierAgent::OnDestruct() {
  if (render_frame_) {
    render_frame_->GetAssociatedInterfaceRegistry()->RemoveInterface(
        mojom::VisualSuggestionsRequestHandler::Name_);
  }
  delete this;
}

}  // namespace companion::visual_search
