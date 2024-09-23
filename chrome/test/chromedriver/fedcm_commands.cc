// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/fedcm_commands.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/fedcm_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/session.h"

Status ExecuteCancelDialog(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }

  base::Value::Dict command_params;
  command_params.Set("dialogId", tracker->GetLastDialogId());

  std::unique_ptr<base::Value> result;
  status = web_view->SendCommandAndGetResult("FedCm.dismissDialog",
                                             command_params, &result);
  tracker->DialogClosed();
  return status;
}

Status ExecuteSelectAccount(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }
  if (!params.FindInt("accountIndex")) {
    return Status(kInvalidArgument, "accountIndex must be specified");
  }

  base::Value::Dict command_params;
  command_params.Set("dialogId", tracker->GetLastDialogId());
  command_params.Set("accountIndex", *params.FindInt("accountIndex"));

  std::unique_ptr<base::Value> result;
  status = web_view->SendCommandAndGetResult("FedCm.selectAccount",
                                             command_params, &result);
  // Only mark the dialog as closed if the command succeeded. For example,
  // if there is a dialog up but it is not an account chooser, selectAccount
  // will fail but the dialog is still up and a later canceldialog command
  // should succeed.
  if (status.IsOk()) {
    tracker->DialogClosed();
  }
  return status;
}

Status ExecuteClickDialogButton(Session* session,
                                WebView* web_view,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }
  if (!params.FindString("dialogButton")) {
    return Status(kInvalidArgument, "dialogButton must be specified");
  }

  base::Value::Dict command_params;
  command_params.Set("dialogId", tracker->GetLastDialogId());

  std::string button = *params.FindString("dialogButton");
  if (button == "TermsOfService" || button == "PrivacyPolicy") {
    std::optional<int> index = params.FindInt("index");
    if (!index) {
      return Status(kInvalidArgument, "index must be specified");
    }
    command_params.Set("accountIndex", *index);
    command_params.Set("accountUrlType", button);
    std::unique_ptr<base::Value> result;
    return web_view->SendCommandAndGetResult("FedCm.openUrl", command_params,
                                             &result);
  }

  command_params.Set("dialogButton", button);

  std::unique_ptr<base::Value> result;
  status = web_view->SendCommandAndGetResult("FedCm.clickDialogButton",
                                             command_params, &result);
  tracker->DialogClosed();
  return status;
}

Status ExecuteGetAccounts(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }
  *value = std::make_unique<base::Value>(tracker->GetLastAccounts().Clone());
  return Status(kOk);
}

Status ExecuteGetDialogType(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }
  *value = std::make_unique<base::Value>(tracker->GetLastDialogType());
  return Status(kOk);
}

Status ExecuteGetFedCmTitle(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }
  if (!tracker->HasDialog()) {
    return Status(kNoSuchAlert);
  }
  base::Value::Dict dict;
  dict.Set("title", tracker->GetLastTitle());
  std::optional<std::string> subtitle = tracker->GetLastSubtitle();
  if (subtitle) {
    dict.Set("subtitle", *subtitle);
  }
  *value = std::make_unique<base::Value>(std::move(dict));
  return Status(kOk);
}

Status ExecuteSetDelayEnabled(Session* session,
                              WebView* web_view,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  // We don't technically need the tracker to implement this command. However,
  // the tracker calls enable during initialization, so if it gets initialized
  // after this command, it would overwrite the delay enabled flag.
  // To avoid that, ensure it is created here.
  FedCmTracker* tracker = nullptr;
  Status status = web_view->GetFedCmTracker(&tracker);
  if (!status.IsOk()) {
    return status;
  }

  if (!params.FindBool("enabled")) {
    return Status(kInvalidArgument, "enabled must be specified");
  }

  base::Value::Dict command_params;
  command_params.Set("disableRejectionDelay", !*params.FindBool("enabled"));

  std::unique_ptr<base::Value> result;
  status = web_view->SendCommandAndGetResult("FedCm.enable", command_params,
                                             &result);
  return status;
}

Status ExecuteResetCooldown(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  std::unique_ptr<base::Value> result;
  Status status = web_view->SendCommandAndGetResult(
      "FedCm.resetCooldown", base::Value::Dict(), &result);
  return status;
}
