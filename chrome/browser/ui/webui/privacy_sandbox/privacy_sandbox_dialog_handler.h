// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrivacySandboxDialogHandler : public content::WebUIMessageHandler {
 public:
  PrivacySandboxDialogHandler(
      base::OnceClosure close_callback,
      base::OnceCallback<void(int)> resize_callback,
      base::OnceClosure show_dialog_callback,
      base::OnceClosure open_settings_callback,
      base::OnceClosure open_measurement_settings_callback,
      PrivacySandboxService::PromptType prompt_type);
  ~PrivacySandboxDialogHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class PrivacySandboxDialogHandlerTest;

  void HandlePromptActionOccurred(const base::Value::List& args);
  void HandleResizeDialog(const base::Value::List& args);
  void HandleShowDialog(const base::Value::List& args);
  void HandleRecordPrivacyPolicyLoadTime(const base::Value::List& args);
  // Determines if the Privacy Policy page should be shown.
  void HandleShouldShowPrivacySandboxPrivacyPolicy(
      const base::Value::List& args);
  void CloseDialog();

  base::OnceClosure close_callback_;
  base::OnceCallback<void(int)> resize_callback_;
  base::OnceClosure show_dialog_callback_;
  base::OnceClosure open_settings_callback_;
  base::OnceClosure open_measurement_settings_callback_;
  PrivacySandboxService::PromptType prompt_type_;

  raw_ptr<PrivacySandboxService> privacy_sandbox_service_;

  // Whether the user has clicked on one of the buttons: accept consent, decline
  // consent, acknowledge notice or open settings.
  bool did_user_make_decision_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
