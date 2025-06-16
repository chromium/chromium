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

  if (dict_value->contains("title")) {
    result->set_title(dict_value->Find("title")->GetString());
  }
  if (dict_value->contains("content")) {
    auto* distilled_content = new proto::DistilledContent();
    distilled_content->set_html(dict_value->Find("content")->GetString());
    result->set_allocated_distilled_content(std::move(distilled_content));
  }

  if (dict_value->contains("dir")) {
    result->set_text_direction(dict_value->Find("content")->GetString());
  } else {
    result->set_text_direction("auto");
  }

  if (dict_value->contains("textContent")) {
    auto* statistics_info = new proto::StatisticsInfo();
    std::string text_content = dict_value->Find("textContent")->GetString();
    statistics_info->set_word_count(CountWords(text_content));
    result->set_allocated_statistics_info(statistics_info);
  }

  return true;
}

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
  if (value->is_none()) {
    found_content = false;
  } else {
    found_content =
        ShouldUseReadabilityDistiller()
            ? ReadabilityDistillerResultToDomDistillerResult(
                  *value, distiller_result.get())
            : dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
                  *value, distiller_result.get());
    if (!found_content) {
      DVLOG(1) << "Unable to parse DomDistillerResult.";
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(distiller_page_callback_),
                                std::move(distiller_result), found_content));
}

}  // namespace dom_distiller
