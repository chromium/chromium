// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_driver.h"

#include <string>

namespace translate {

namespace testing {

const std::string kHtmlMimeType = "text/html";

MockTranslateDriver::MockTranslateDriver()
    : is_incognito_(false),
      on_is_page_translated_changed_called_(false),
      on_translate_enabled_changed_called_(false),
      translate_page_is_called_(false),
      language_state_(this),
      last_committed_url_(GURL::EmptyGURL()) {}

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

bool MockTranslateDriver::IsIncognito() {
  return is_incognito_;
}

const std::string& MockTranslateDriver::GetContentsMimeType() {
  return kHtmlMimeType;
}

const GURL&  MockTranslateDriver::GetLastCommittedURL() {
  return last_committed_url_;
}

const GURL& MockTranslateDriver::GetVisibleURL() {
  return GURL::EmptyGURL();
}

ukm::SourceId MockTranslateDriver::GetUkmSourceId() {
  return ukm::kInvalidSourceId;
}

bool MockTranslateDriver::HasCurrentPage() {
  return true;
}

void MockTranslateDriver::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

}  // namespace testing

}  // namespace translate

