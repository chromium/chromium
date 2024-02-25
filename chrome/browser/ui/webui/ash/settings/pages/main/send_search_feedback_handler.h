// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_SEND_SEARCH_FEEDBACK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_SEND_SEARCH_FEEDBACK_HANDLER_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash::settings {

// WebUI message handler for os settings send search feedback.
class SendSearchFeedbackHandler : public ::settings::SettingsPageUIHandler {
 public:
  SendSearchFeedbackHandler();

  SendSearchFeedbackHandler(const SendSearchFeedbackHandler&) = delete;
  SendSearchFeedbackHandler& operator=(const SendSearchFeedbackHandler&) =
      delete;

  ~SendSearchFeedbackHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Wrapper for HandleOpenFeedbackDialog, needed for testing the correct value
  // of description_template gets passed through from JS to C++ via
  // chrome.send(). |description_template| contains a description template for
  // the feedback dialog.
  virtual void OpenFeedbackDialogWrapper(
      const std::string& description_template);

  // Opens the feedback dialog.
  // |args| contains a description template.
  virtual void HandleOpenFeedbackDialog(const base::Value::List& args);
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_SEND_SEARCH_FEEDBACK_HANDLER_H_
