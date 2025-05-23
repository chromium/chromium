// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PrivacySandboxDialogCallbackState)
enum class PrivacySandboxDialogCallbackState {
  kSingleActionCallbackDNE = 0,
  kMultiActionCallbackDNE = 1,
  kCallbackUnknownBeforeShown = 2,
  kMaxValue = kCallbackUnknownBeforeShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxDialogCallbackState)
class PrivacySandboxDialogHandler : public content::WebUIMessageHandler {
 public:
  PrivacySandboxDialogHandler(
      base::RepeatingCallback<void(
          PrivacySandboxService::AdsDialogCallbackNoArgsEvents)> dialog_callback,
      base::OnceCallback<void(int)> resize_callback,
      PrivacySandboxService::PromptType prompt_type);
  ~PrivacySandboxDialogHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class PrivacySandboxDialogHandlerTest;
  friend class PrivacySandboxNoticeDialogHandlerCallbackDNETest;
  void SetDialogCallbackForTesting(
      const base::RepeatingCallback<void(
          PrivacySandboxService::AdsDialogCallbackNoArgsEvents)>& callback);
  void HandlePromptActionOccurred(const base::Value::List& args);
  void HandleResizeDialog(const base::Value::List& args);
  void HandleShowDialog(const base::Value::List& args);
  void HandleRecordPrivacyPolicyLoadTime(const base::Value::List& args);
  void HandleShouldShowAdTopicsContentParity(const base::Value::List& args);
  void CloseDialog();

  base::RepeatingCallback<void(
      PrivacySandboxService::AdsDialogCallbackNoArgsEvents)>
      dialog_callback_;
  base::OnceCallback<void(int)> resize_callback_;
  PrivacySandboxService::PromptType prompt_type_;

  raw_ptr<PrivacySandboxService> privacy_sandbox_service_;

  // The number of times a final decision was taken on a notice.
  int final_decision_count_ = 0;
  // Whether the dialog was shown.
  bool did_dialog_show_ = false;
  // Whether the user has clicked on one of the buttons: accept consent, decline
  // consent, acknowledge notice or open settings.
  bool did_user_make_decision_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
