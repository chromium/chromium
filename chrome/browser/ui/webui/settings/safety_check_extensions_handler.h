// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace extensions {
class CWSInfoService;
class SafetyCheckExtensionsHandlerTest;
}  // namespace extensions

namespace settings {

// Settings page UI handler that checks for any extensions that trigger
// a review by the safety check.
class SafetyCheckExtensionsHandler : public settings::SettingsPageUIHandler {
 public:
  explicit SafetyCheckExtensionsHandler(Profile* profile);
  ~SafetyCheckExtensionsHandler() override;

  void SetCWSInfoServiceForTest(extensions::CWSInfoService* cws_info_service);

 private:
  friend class extensions::SafetyCheckExtensionsHandlerTest;

  // Calculate the number of extensions that need to be reviewed by the
  // user.
  void HandleGetNumberOfExtensionsThatNeedReview(const base::Value::List& args);

  // Return the number of extensions that should be reviewed by the user.
  // There are currently three triggers the `SafetyCheckExtensionsHandler`
  // tracks:
  // -- Extension Malware Violation
  // -- Extension Policy Violation
  // -- Extension Unpublished by the developer
  int GetNumberOfExtensionsThatNeedReview();

  // SettingsPageUIHandler implementation.
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<extensions::CWSInfoService> cws_info_service_ = nullptr;
  base::WeakPtrFactory<SafetyCheckExtensionsHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_
