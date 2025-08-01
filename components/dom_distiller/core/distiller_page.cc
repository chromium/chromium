// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/extraction_utils.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

// Counts the number of words in the text_content portion, used to record how
// many words are present for a readability distillation. Note this won't work
// as well on languages like Chinese where the space separation isn't the
// same as in english.
int CountWords(const std::string& text_content) {
  int result = 0;
  bool prev_char_whitespace = false;
  for (const char& it : text_content) {
    bool cur_char_whitespace = it == ' ';
    if (prev_char_whitespace && !cur_char_whitespace) {
      result++;
    }
    prev_char_whitespace = cur_char_whitespace;
  }

  return result + 1;
}

// Converts the js object returned by the readability distiller into the
// DomDistillerResult expected by the distillation infra.
bool ReadabilityDistillerResultToDomDistillerResult(
    const base::Value& value,
    proto::DomDistillerResult* result) {
  if (!value.is_dict()) {
    return false;
  }

  const base::DictValue* dict_value = value.GetIfDict();

  if (auto* title = dict_value->FindString("title")) {
    result->set_title(*title);
  }
  if (auto* content = dict_value->FindString("content")) {
    auto* distilled_content = new proto::DistilledContent();
    distilled_content->set_html(*content);
    result->set_allocated_distilled_content(std::move(distilled_content));
  }

  if (auto* dir = dict_value->FindString("dir")) {
    result->set_text_direction(*dir);
  } else {
    result->set_text_direction("auto");
  }

  if (auto* text_content = dict_value->FindString("textContent")) {
    auto* statistics_info = new proto::StatisticsInfo();
    statistics_info->set_word_count(CountWords(*text_content));
    result->set_allocated_statistics_info(statistics_info);
  }

  return true;
}

// This enum is used to record histograms for OnDistillationDone results.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(DistillationParseResult)
enum class DistillationParseResult {
  kSuccess = 0,
  kParseFailure = 1,
  kNoData = 2,
  kMaxValue = kNoData,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:DistillationParseResult)

}  // namespace

DistillerPageFactory::~DistillerPageFactory() = default;

DistillerPage::DistillerPage() : ready_(true) {}

DistillerPage::~DistillerPage() = default;

void DistillerPage::DistillPage(
    const GURL& gurl,
    const dom_distiller::proto::DomDistillerOptions options,
    DistillerPageCallback callback) {
  CHECK(ready_);
  CHECK(callback);
  CHECK(!distiller_page_callback_);
  // It is only possible to distill one page at a time. |ready_| is reset when
  // the callback to OnDistillationDone happens.
  ready_ = false;
  distiller_page_callback_ = std::move(callback);

  DistillPageImpl(gurl, ShouldUseReadabilityDistiller()
                            ? GetReadabilityDistillerScript()
                            : GetDistillerScriptWithOptions(options));
}

void DistillerPage::OnDistillationDone(const GURL& page_url,
                                       const base::Value* value) {
  DCHECK(!ready_);
  ready_ = true;

  std::unique_ptr<dom_distiller::proto::DomDistillerResult> distiller_result(
      new dom_distiller::proto::DomDistillerResult());
  bool found_content;
  DistillationParseResult result;

  if (value->is_none()) {
    found_content = false;
    result = DistillationParseResult::kNoData;
  } else {
    found_content =
        ShouldUseReadabilityDistiller()
            ? ReadabilityDistillerResultToDomDistillerResult(
                  *value, distiller_result.get())
            : dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
                  *value, distiller_result.get());
    if (found_content) {
      result = DistillationParseResult::kSuccess;
    } else {
      DVLOG(1) << "Unable to parse DomDistillerResult.";
      result = DistillationParseResult::kParseFailure;
    }
  }

  // Record result for page distillation
  base::UmaHistogramEnumeration("DomDistiller.Distillation.Result", result);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(distiller_page_callback_),
                                std::move(distiller_result), found_content));
}

}  // namespace dom_distiller
