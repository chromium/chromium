// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_REVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_REVIEW_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class PrivacyReviewHandler : public SettingsPageUIHandler {
 public:
  PrivacyReviewHandler() = default;
  ~PrivacyReviewHandler() override = default;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

 private:
  friend class PrivacyReviewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacyReviewHandlerTest, IsPrivacyReviewAvailable);

  void HandleIsPrivacyReviewAvailable(base::Value::ConstListView args);

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_REVIEW_HANDLER_H_
