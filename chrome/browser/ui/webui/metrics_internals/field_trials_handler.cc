// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/field_trials_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

// LINT.IfChange(field_trials_handler)

FieldTrialsHandler::FieldTrialsHandler(Profile* profile)
    : profile_(profile),
      base_handler_(std::make_unique<metrics::FieldTrialsHandlerBase>(
          this,
          g_browser_process->variations_service(),
          g_browser_process->local_state())) {}

FieldTrialsHandler::~FieldTrialsHandler() = default;

void FieldTrialsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchTrialState",
      base::BindRepeating(&FieldTrialsHandler::HandleFetchState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTrialEnrollState",
      base::BindRepeating(&FieldTrialsHandler::HandleSetEnrollState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&FieldTrialsHandler::HandleRestart,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "lookupTrialOrGroupName",
      base::BindRepeating(&FieldTrialsHandler::HandleLookupTrialOrGroupName,
                          base::Unretained(this)));
}

void FieldTrialsHandler::ResolvePageCallback(const base::ValueView callback_id,
                                             const base::ValueView response) {
  ResolveJavascriptCallback(callback_id, response);
}

void FieldTrialsHandler::HandleFetchState(const base::ListValue& args) {
  AllowJavascript();
  // args[0]: Callback ID.
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleFetchState(args[0], GetShowNames());
}

void FieldTrialsHandler::HandleSetEnrollState(const base::ListValue& args) {
  AllowJavascript();
  // args[0]: Callback ID.
  // args[1]: Trial name hash (string).
  // args[2]: Group name hash (string).
  // args[3]: Whether the override is enabled (bool).
  CHECK_EQ(args.size(), 4U);
  base_handler_->HandleSetEnrollState(args[0], args[1].GetString(),
                                      args[2].GetString(), args[3].GetBool());
}

void FieldTrialsHandler::HandleRestart(const base::ListValue& args) {
  chrome::AttemptRestart();
}

void FieldTrialsHandler::HandleLookupTrialOrGroupName(
    const base::ListValue& args) {
  AllowJavascript();
  // args[0]: Callback ID.
  // args[1]: Trial or group name (string).
  CHECK_EQ(args.size(), 2U);
  base_handler_->HandleLookupTrialOrGroupName(args[0], args[1].GetString());
}

bool FieldTrialsHandler::GetShowNames() {
  bool always_show_names =
#if defined(OFFICIAL_BUILD)
      false;
#else
      true;
#endif

  return always_show_names ||
         gaia::IsGoogleInternalAccountEmail(
             IdentityManagerFactory::GetForProfile(profile_)
                 ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                 .email);
}

// LINT.ThenChange(//ios/chrome/browser/webui/ui_bundled/metrics_internals/field_trials_handler.mm)
