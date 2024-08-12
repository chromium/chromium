// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_DRIVER_H_

#include <memory>
#include <string>

#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

namespace testing {

class MockTranslateDriver : public TranslateDriver {
 public:
  MockTranslateDriver();
  ~MockTranslateDriver() override;

  void Reset();

  // TranslateDriver:
  void OnIsPageTranslatedChanged() override;
  void OnTranslateEnabledChanged() override;
  bool IsLinkNavigation() override;
  void TranslatePage(int page_seq_no,
                     const std::string& translate_script,
                     const std::string& source_lang,
                     const std::string& target_lang) override;
  void RevertTranslation(int page_seq_no) override {}
  bool IsIncognito() const override;
  const std::string& GetContentsMimeType() override;
  const GURL& GetLastCommittedURL() const override;
  const GURL& GetVisibleURL() override;
  ukm::SourceId GetUkmSourceId() override;
  LanguageState& GetLanguageState();
  bool HasCurrentPage() const override;

  void set_incognito() { is_incognito_ = true; }

  bool on_is_page_translated_changed_called() const {
    return on_is_page_translated_changed_called_;
  }

  bool on_translate_enabled_changed_called() const {
    return on_translate_enabled_changed_called_;
  }

  bool TranslatePage_is_called() const { return translate_page_is_called_; }

  void SetLastCommittedURL(const GURL& url);
  void SetVisibleURL(const GURL& url);

  void SetPageMimeType(const std::string& mime_type);

 private:
  bool is_incognito_;
  bool on_is_page_translated_changed_called_;
  bool on_translate_enabled_changed_called_;
  bool translate_page_is_called_;
  LanguageState language_state_;
  GURL last_committed_url_;
  std::string page_mime_type_ = "text/html";
  GURL visible_url_;
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_DRIVER_H_
