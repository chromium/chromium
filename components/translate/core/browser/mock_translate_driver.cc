// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_driver.h"

#include <string>

namespace translate {

namespace testing {

MockTranslateDriver::MockTranslateDriver()
    : is_incognito_(false),
      on_is_page_translated_changed_called_(false),
      on_translate_enabled_changed_called_(false),
      translate_page_is_called_(false),
      language_state_(this) {}

MockTranslateDriver::~MockTranslateDriver() = default;

void MockTranslateDriver::TranslatePage(int page_seq_no,
                                        const std::string& translate_script,
                                        const std::string& source_lang,
                                        const std::string& target_lang) {
  translate_page_is_called_ = true;
}

void MockTranslateDriver::Reset() {
  on_is_page_translated_changed_called_ = false;
  on_translate_enabled_changed_called_ = false;
}

void MockTranslateDriver::OnIsPageTranslatedChanged() {
  on_is_page_translated_changed_called_ = true;
}

void  MockTranslateDriver::OnTranslateEnabledChanged() {
  on_translate_enabled_changed_called_ = true;
}

bool MockTranslateDriver::IsLinkNavigation() {
  return false;
}

bool MockTranslateDriver::IsIncognito() const {
  return is_incognito_;
}

const std::string& MockTranslateDriver::GetContentsMimeType() {
  return page_mime_type_;
}

const GURL& MockTranslateDriver::GetLastCommittedURL() const {
  return last_committed_url_;
}

const GURL& MockTranslateDriver::GetVisibleURL() {
  return visible_url_;
}

ukm::SourceId MockTranslateDriver::GetUkmSourceId() {
  return ukm::kInvalidSourceId;
}

LanguageState& MockTranslateDriver::GetLanguageState() {
  return language_state_;
}

bool MockTranslateDriver::HasCurrentPage() const {
  return true;
}

void MockTranslateDriver::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

void MockTranslateDriver::SetPageMimeType(
    const std::string& mime_type) {
  page_mime_type_ = mime_type;
}

void MockTranslateDriver::SetVisibleURL(const GURL& url) {
  visible_url_ = url;
}

}  // namespace testing

}  // namespace translate
