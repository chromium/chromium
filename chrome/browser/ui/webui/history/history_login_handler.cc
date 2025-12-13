// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_login_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/history/history_identity_state_watcher.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

HistoryLoginHandler::HistoryLoginHandler(
    base::RepeatingClosure identity_state_changed_callback)
    : identity_state_changed_callback_(
          std::move(identity_state_changed_callback)) {}

HistoryLoginHandler::~HistoryLoginHandler() = default;

void HistoryLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "otherDevicesInitialized",
      base::BindRepeating(&HistoryLoginHandler::HandleOtherDevicesInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startTurnOnSyncFlow",
      base::BindRepeating(&HistoryLoginHandler::HandleTurnOnSyncFlow,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "recordSigninPendingOffered",
      base::BindRepeating(
          &HistoryLoginHandler::HandleRecordSigninPendingOffered,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getInitialIdentityState",
      base::BindRepeating(&HistoryLoginHandler::HandleGetInitialIdentityState,
                          base::Unretained(this)));
}

void HistoryLoginHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  // base::Unretained(this) is safe here because `this` owns the watcher.'
  history_identity_state_watcher_ =
      std::make_unique<HistoryIdentityStateWatcher>(
          identity_manager, sync_service,
          base::BindRepeating(&HistoryLoginHandler::IdentityStateChanged,
                              base::Unretained(this)));
  IdentityStateChanged();
}

void HistoryLoginHandler::OnJavascriptDisallowed() {
  history_identity_state_watcher_ = nullptr;
}

void HistoryLoginHandler::HandleOtherDevicesInitialized(
    const base::Value::List& /*args*/) {
  AllowJavascript();
}

void HistoryLoginHandler::IdentityStateChanged() {
  if (!identity_state_changed_callback_.is_null()) {
    identity_state_changed_callback_.Run();
  }

  FireWebUIListener("history-identity-state-changed",
                    GetHistoryIdentityStateDict());
}

void HistoryLoginHandler::HandleTurnOnSyncFlow(
    const base::Value::List& /*args*/) {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
#if !BUILDFLAG(IS_CHROMEOS)
  if (account_info.IsEmpty()) {
    account_info = signin_ui_util::GetSingleAccountForPromos(identity_manager);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile, account_info, signin_metrics::AccessPoint::kRecentTabs);
}

void HistoryLoginHandler::HandleRecordSigninPendingOffered(
    const base::Value::List& /*args*/) {
  signin_metrics::LogSigninPendingOffered(
      signin_metrics::AccessPoint::kRecentTabs);
}

void HistoryLoginHandler::HandleGetInitialIdentityState(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetHistoryIdentityStateDict());
}

// LINT.IfChange(GetHistoryIdentityStateDict)
base::Value::Dict HistoryLoginHandler::GetHistoryIdentityStateDict() {
  base::Value::Dict dict;
  HistoryIdentityState history_identity_state =
      history_identity_state_watcher_->GetHistoryIdentityState();
  dict.Set("signIn", static_cast<int>(history_identity_state.sign_in));
  dict.Set("tabsSync", static_cast<int>(history_identity_state.tab_sync));
  dict.Set("historySync",
           static_cast<int>(history_identity_state.history_sync));
  return dict;
}
// LINT.ThenChange(/chrome/browser/resources/history/externs.ts:HistoryIdentityState)
