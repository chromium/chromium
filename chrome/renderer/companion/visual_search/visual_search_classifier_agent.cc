// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_search/visual_search_classifier_agent.h"

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace companion::visual_search {

namespace {

// Representation of list of images found in DOM.
// The string type will eventually be replaced with a struct containing
// image features (i.e. SingleImageGeometryFeature).
typedef std::vector<std::pair<std::string, SkBitmap>> DOMImageList;

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

  // TODO (b/277771722): Convert list of images to DOMImageList structure.
  // This requires calling ClassificationAndEligibility module to generate
  // SingleImageGeometryFeatures.

  return images;
}

std::vector<SkBitmap> ClassifyImagesOnBackground(DOMImageList images,
                                                 std::string model_data,
                                                 std::string config_proto) {
  std::vector<SkBitmap> results;

  // TODO(b/277771722) - call classifier with the following steps:
  // 1) init classifier with model_data and config_proto.
  // 2) run classifier and eligibility on imagelist.
  // 3) Return list of bitmaps once complete or empty list.
  // 4) Limit the number of images that we send to the top N.

  return results;
}

}  // namespace

VisualSearchClassifierAgent::VisualSearchClassifierAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  if (render_frame) {
    render_frame_ = render_frame;
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
    const std::string config_proto,
    ClassifierResultCallback callback) {
  if (is_classifying_) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "Companion.VisualSearch.Agent.OngoingClassificationFailure",
        is_classifying_);
    return;
  }

  if (!visual_model.IsValid()) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.InvalidModelFailure",
                            visual_model.IsValid());
    return;
  }

  if (!visual_model_.Initialize(std::move(visual_model))) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.InitModelFailure",
                            true);
    return;
  }

  if (callback.is_null()) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.NoCallbackFailure",
                            callback.is_null());
    return;
  }

  is_classifying_ = true;
  result_callback_ = std::move(callback);
  std::string model_data =
      std::string(reinterpret_cast<const char*>(visual_model_.data()));
  std::vector<std::pair<std::string, SkBitmap>> dom_images =
      FindImagesOnPage(render_frame_);
  LOCAL_HISTOGRAM_COUNTS_100("Companion.VisualSearch.Agent.DomImageCount",
                             dom_images.size());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ClassifyImagesOnBackground, std::move(dom_images),
                     std::move(model_data), std::move(config_proto)),
      base::BindOnce(&VisualSearchClassifierAgent::OnClassificationDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VisualSearchClassifierAgent::OnClassificationDone(
    const std::vector<SkBitmap> results) {
  is_classifying_ = false;
  if (result_callback_.is_null()) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.Agent.NoCallbackFailure",
                            result_callback_.is_null());
    return;
  }
  std::move(result_callback_).Run(results);
  // We only use a callback once and require caller to allow provide it per
  // call.
  result_callback_.Reset();
}

void VisualSearchClassifierAgent::OnDestruct() {
  delete this;
}

}  // namespace companion::visual_search
