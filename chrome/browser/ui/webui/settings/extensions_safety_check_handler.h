// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_EXTENSIONS_SAFETY_CHECK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_EXTENSIONS_SAFETY_CHECK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// Settings page UI handler that checks for any extensions that trigger
// a review by the safety check.
class ExtensionsSafetyCheckHandler : public settings::SettingsPageUIHandler {
 public:
  ExtensionsSafetyCheckHandler();
  ~ExtensionsSafetyCheckHandler() override;

 protected:
  // Return the display string that represents how many extensions
  // need to be reviewed by the user.
  std::u16string GetExtensionsThatNeedReview();

 private:
  // Calculate the number of extensions that need to be reviewed by the
  // user.
  void HandleGetExtensionsThatNeedReview(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  base::WeakPtrFactory<ExtensionsSafetyCheckHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_EXTENSIONS_SAFETY_CHECK_HANDLER_H_
