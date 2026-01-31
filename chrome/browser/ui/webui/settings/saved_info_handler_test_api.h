// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_TEST_API_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/settings/saved_info_handler.h"
#include "content/public/browser/web_ui.h"

namespace settings {

class SavedInfoHandlerTestApi {
 public:
  explicit SavedInfoHandlerTestApi(SavedInfoHandler* handler)
      : handler_(*handler) {}

  void HandleGetPasswordCount(const base::ListValue& args) {
    handler_->HandleGetPasswordCount(args);
  }

  void HandleGetLoyaltyCardsCount(const base::ListValue& args) {
    handler_->HandleGetLoyaltyCardsCount(args);
  }

  void HandleRequestDataManagementSurvey(const base::ListValue& args) {
    handler_->HandleRequestDataManagementSurvey(args);
  }

  void set_web_ui(content::WebUI* web_ui) { handler_->set_web_ui(web_ui); }

 private:
  const raw_ref<SavedInfoHandler> handler_;
};

inline SavedInfoHandlerTestApi test_api(SavedInfoHandler& handler) {
  return SavedInfoHandlerTestApi(&handler);
}

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_TEST_API_H_
