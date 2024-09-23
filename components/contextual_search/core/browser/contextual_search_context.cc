// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/contextual_search_context.h"

#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/language_detection/language_detection_util.h"

ContextualSearchContext::ContextualSearchContext() = default;
ContextualSearchContext::~ContextualSearchContext() = default;

void ContextualSearchContext::SetResolveProperties(
    const std::string& home_country,
    bool may_send_base_page_url) {
  can_resolve_ = true;
  home_country_ = home_country;
  can_send_base_page_url_ = may_send_base_page_url;
}

void ContextualSearchContext::AdjustSelection(int start_adjust,
                                              int end_adjust) {
  // TODO(crbug.com/40060256): These values should be sanitized and should be
  // sanitized closer to where they are received from the renderer process.
  DCHECK(start_offset_ + start_adjust >= 0);
  DCHECK(start_offset_ + start_adjust <=
         static_cast<int>(surrounding_text_.length()));
  DCHECK(end_offset_ + end_adjust >= 0);
  DCHECK(end_offset_ + end_adjust <=
         static_cast<int>(surrounding_text_.length()));
  start_offset_ += start_adjust;
  end_offset_ += end_adjust;
}

void ContextualSearchContext::PrepareToResolve(
    bool is_exact_resolve,
    const std::string& related_searches_stamp) {
  is_exact_resolve_ = is_exact_resolve;
  related_searches_stamp_ = related_searches_stamp;
  if (!related_searches_stamp_.empty()) {
    // Only RELATED_SEARCHES queries pass a stamp.
    request_type_ = RequestType::RELATED_SEARCHES;
  }
}

std::string ContextualSearchContext::DetectLanguage() const {
  std::string language = GetReliableLanguage(GetSelection());
  if (language.empty())
    language = GetReliableLanguage(surrounding_text_);
  return language;
}

base::WeakPtr<ContextualSearchContext> ContextualSearchContext::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string ContextualSearchContext::GetReliableLanguage(
    const std::u16string& contents) const {
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score;
  std::string language = translate::DeterminePageLanguage(
      /*code=*/std::string(),
      /*html_lang=*/std::string(), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  // Make sure we return an empty string when unreliable or an unknown result.
  if (!is_model_reliable || language == translate::kUnknownLanguageCode)
    language = "";
  return language;
}

std::u16string ContextualSearchContext::GetSelection() const {
  int start = start_offset_;
  int end = end_offset_;
  DCHECK(start >= 0);
  DCHECK(end >= 0);
  DCHECK(end <= static_cast<int>(surrounding_text_.length()));
  DCHECK(start <= end);
  return surrounding_text_.substr(start, end - start);
}
