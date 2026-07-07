// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/metrics/debug/field_trials_handler_base.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// UI Handler for the Field Trials tab of chrome://metrics-internals.
class FieldTrialsHandler : public content::WebUIMessageHandler,
                           public metrics::FieldTrialsHandlerBase::Delegate {
 public:
  explicit FieldTrialsHandler(Profile* profile);
  ~FieldTrialsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // metrics::FieldTrialsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchState(const base::ListValue& args);
  void HandleSetEnrollState(const base::ListValue& args);
  void HandleRestart(const base::ListValue& args);
  void HandleLookupTrialOrGroupName(const base::ListValue& args);

  bool GetShowNames();

  raw_ptr<Profile> profile_;
  std::unique_ptr<metrics::FieldTrialsHandlerBase> base_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
