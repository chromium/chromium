// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distillability_agent.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_distillability.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace dom_distiller {

namespace {

const char* const kFilterlist[] = {"www.reddit.com", "tools.usps.com",
                                   "old.reddit.com"};

// Returns whether it is necessary to send updates back to the browser.
// The number of updates can be from 0 to 2. See the tests in
// "distillable_page_utils_browsertest.cc".
// Most heuristics types only require one update after parsing.
// Adaboost-based heuristics are the only ones doing the second update,
// which is after loading.
bool NeedToUpdate(bool is_loaded) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return !is_loaded;
    case DistillerHeuristicsType::OG_ARTICLE:
      return !is_loaded;
    case DistillerHeuristicsType::ADABOOST_MODEL:
    case DistillerHeuristicsType::ALL_ARTICLES:
      return true;
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

// Returns whether this update is the last one for the page.
bool IsLast(bool is_loaded) {
  if (GetDistillerHeuristicsType() == DistillerHeuristicsType::ADABOOST_MODEL ||
      GetDistillerHeuristicsType() == DistillerHeuristicsType::ALL_ARTICLES)
    return is_loaded;

  return true;
}

bool IsFiltered(const GURL& url) {
  for (auto* filter : kFilterlist) {
    if (base::EqualsCaseInsensitiveASCII(url.GetHost(), filter)) {
      return true;
    }
  }
  return false;
}

void DumpDistillability(content::RenderFrame* render_frame,
                        const blink::WebDistillabilityFeatures& features,
                        const std::vector<double>& derived,
                        double score,
                        bool distillable,
                        double long_score,
                        bool long_page,
                        bool filtered) {
  base::Value::Dict dict;
  std::string msg;

  base::Value::Dict raw_features;
  raw_features.Set("is_mobile_friendly", features.is_mobile_friendly);
  raw_features.Set("open_graph", features.open_graph);
  raw_features.Set("element_count", static_cast<int>(features.element_count));
  raw_features.Set("anchor_count", static_cast<int>(features.anchor_count));
  raw_features.Set("form_count", static_cast<int>(features.form_count));
  raw_features.Set("text_input_count",
                   static_cast<int>(features.text_input_count));
  raw_features.Set("password_input_count",
                   static_cast<int>(features.password_input_count));
  raw_features.Set("p_count", static_cast<int>(features.p_count));
  raw_features.Set("pre_count", static_cast<int>(features.pre_count));
  raw_features.Set("moz_score", features.moz_score);
  raw_features.Set("moz_score_all_sqrt", features.moz_score_all_sqrt);
  raw_features.Set("moz_score_all_linear", features.moz_score_all_linear);
  dict.Set("features", std::move(raw_features));

  base::Value::List derived_features;
  for (double value : derived) {
    derived_features.Append(value);
  }
  dict.Set("derived_features", std::move(derived_features));

  dict.Set("score", score);
  dict.Set("distillable", distillable);
  dict.Set("long_score", long_score);
  dict.Set("long_page", long_page);
  dict.Set("filtered", filtered);
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &msg);
  msg = "adaboost_classification = " + msg;

  render_frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kVerbose,
                                    msg);
}

void RecordDistillabilityMetrics(double score,
                                 double long_score,
                                 bool is_disitillable,
                                 ukm::UkmRecorder* ukm_recorder,
                                 ukm::SourceId source_id) {
  int adj_score = std::abs(score * 100);
  if (score > 0) {
    base::UmaHistogramCounts100("DomDistiller.AdaBoostModel.PositiveScore",
                                adj_score);
  } else {
    base::UmaHistogramCounts100("DomDistiller.AdaBoostModel.NegativeScore",
                                adj_score);
  }

  int adj_long_score = std::abs(long_score * 100);
  if (long_score > 0) {
    base::UmaHistogramCounts100("DomDistiller.LongModel.PositiveScore",
                                adj_long_score);
  } else {
    base::UmaHistogramCounts100("DomDistiller.LongModel.NegativeScore",
                                adj_long_score);
  }

  base::UmaHistogramBoolean("DomDistiller.IsDistillable", is_disitillable);
  ukm::builders::DomDistiller_ModelResult_Distillable(source_id)
      .SetDistillable(is_disitillable)
      .Record(ukm_recorder);
}

bool IsDistillablePageAdaboost(blink::WebDocument& doc,
                               const DistillablePageDetector* detector,
                               const DistillablePageDetector* long_page,
                               bool is_last,
                               bool& is_long_article,
                               bool& is_mobile_friendly,
                               content::RenderFrame* render_frame,
                               bool dump_info,
                               ukm::UkmRecorder* ukm_recorder) {
  GURL parsed_url(doc.Url());
  if (!parsed_url.is_valid()) {
    return false;
  }
  blink::WebDistillabilityFeatures features = doc.DistillabilityFeatures();
  is_mobile_friendly = features.is_mobile_friendly;
  std::vector<double> derived = CalculateDerivedFeatures(
      features.open_graph, parsed_url, features.element_count,
      features.anchor_count, features.form_count, features.moz_score,
      features.moz_score_all_sqrt, features.moz_score_all_linear);
  double score = detector->Score(derived) - detector->GetThreshold();
  double long_score = long_page->Score(derived) - long_page->GetThreshold();
  bool distillable = score > 0;
  is_long_article = long_score > 0;
  bool filtered = IsFiltered(parsed_url);

  bool is_distillable = distillable && is_long_article;
  RecordDistillabilityMetrics(score, long_score, is_distillable, ukm_recorder,
                              doc.GetUkmSourceId());

  if (dump_info) {
    DumpDistillability(render_frame, features, derived, score, distillable,
                       long_score, is_long_article, filtered);
  }

  if (filtered) {
    return false;
  }

  return is_distillable;
}

bool IsDistillablePage(blink::WebDocument& doc,
                       bool is_last,
                       bool& is_long_article,
                       bool& is_mobile_friendly,
                       content::RenderFrame* render_frame,
                       bool dump_info,
                       ukm::UkmRecorder* ukm_recorder) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return true;
    case DistillerHeuristicsType::OG_ARTICLE:
      return doc.DistillabilityFeatures().open_graph;
    case DistillerHeuristicsType::ADABOOST_MODEL:
    case DistillerHeuristicsType::ALL_ARTICLES:
      return IsDistillablePageAdaboost(
          doc, DistillablePageDetector::GetNewModel(),
          DistillablePageDetector::GetLongPageModel(), is_last, is_long_article,
          is_mobile_friendly, render_frame, dump_info, ukm_recorder);
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

}  // namespace

DistillabilityAgent::DistillabilityAgent(content::RenderFrame* render_frame,
                                         bool dump_info)
    : RenderFrameObserver(render_frame), dump_info_(dump_info) {
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

void DistillabilityAgent::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  if (layout_type != blink::WebMeaningfulLayout::kFinishedParsing &&
      layout_type != blink::WebMeaningfulLayout::kFinishedLoading) {
    return;
  }

  DCHECK(render_frame());
  DCHECK(render_frame()->GetWebFrame());
  if (!render_frame()->GetWebFrame()->IsOutermostMainFrame())
    return;
  blink::WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  if (doc.IsNull() || doc.Body().IsNull())
    return;
  if (!url_utils::IsUrlDistillable(doc.Url()))
    return;

  bool is_loaded = layout_type == blink::WebMeaningfulLayout::kFinishedLoading;
  if (!NeedToUpdate(is_loaded))
    return;

  bool is_last = IsLast(is_loaded);
  // Connect to Mojo service on browser to notify page distillability.
  mojo::Remote<mojom::DistillabilityService> distillability_service;
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      distillability_service.BindNewPipeAndPassReceiver());
  if (!distillability_service.is_bound())
    return;
  bool is_long_article = false;
  bool is_mobile_friendly = false;
  bool is_distillable =
      IsDistillablePage(doc, is_last, is_long_article, is_mobile_friendly,
                        render_frame(), dump_info_, ukm_recorder_.get());
  distillability_service->NotifyIsDistillable(
      is_distillable, is_last, is_long_article, is_mobile_friendly);
}

DistillabilityAgent::~DistillabilityAgent() = default;

void DistillabilityAgent::OnDestruct() {
  delete this;
}

}  // namespace dom_distiller
