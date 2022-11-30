// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_login_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/profile_info_watcher.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

HistoryLoginHandler::HistoryLoginHandler(base::RepeatingClosure signin_callback)
    : signin_callback_(std::move(signin_callback)) {}

HistoryLoginHandler::~HistoryLoginHandler() {}

void HistoryLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "otherDevicesInitialized",
      base::BindRepeating(&HistoryLoginHandler::HandleOtherDevicesInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startTurnOnSyncFlow",
      base::BindRepeating(&HistoryLoginHandler::HandleTurnOnSyncFlow,
                          base::Unretained(this)));
}

void HistoryLoginHandler::OnJavascriptAllowed() {
  profile_info_watcher_ = std::make_unique<ProfileInfoWatcher>(
      Profile::FromWebUI(web_ui()),
      base::BindRepeating(&HistoryLoginHandler::ProfileInfoChanged,
                          base::Unretained(this)));
  ProfileInfoChanged();
}

void HistoryLoginHandler::OnJavascriptDisallowed() {
  profile_info_watcher_ = nullptr;
}

void HistoryLoginHandler::HandleOtherDevicesInitialized(
    const base::Value::List& /*args*/) {
  AllowJavascript();
}

void HistoryLoginHandler::ProfileInfoChanged() {
  bool signed_in = !profile_info_watcher_->GetAuthenticatedUsername().empty();
  if (!signin_callback_.is_null())
    signin_callback_.Run();

  FireWebUIListener("sign-in-state-changed", base::Value(signed_in));
}

void HistoryLoginHandler::HandleTurnOnSyncFlow(
    const base::Value::List& /*args*/) {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile,
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
}
