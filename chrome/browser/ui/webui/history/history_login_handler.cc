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
#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

HistoryLoginHandler::HistoryLoginHandler(
    base::RepeatingClosure signin_state_changed_callback)
    : signin_state_changed_callback_(std::move(signin_state_changed_callback)) {
}

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
}

void HistoryLoginHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  history_sign_in_state_watcher_ = std::make_unique<HistorySignInStateWatcher>(
      identity_manager, sync_service,
      base::BindRepeating(&HistoryLoginHandler::SigninStateChanged,
                          base::Unretained(this)));
  SigninStateChanged();
}

void HistoryLoginHandler::OnJavascriptDisallowed() {
  history_sign_in_state_watcher_ = nullptr;
}

void HistoryLoginHandler::HandleOtherDevicesInitialized(
    const base::Value::List& /*args*/) {
  AllowJavascript();
}

void HistoryLoginHandler::SigninStateChanged() {
  if (!signin_state_changed_callback_.is_null()) {
    signin_state_changed_callback_.Run();
  }

  HistorySignInState sign_in_state =
      history_sign_in_state_watcher_->GetSignInState();
  FireWebUIListener("sign-in-state-changed", static_cast<int>(sign_in_state));
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
