// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_login_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/profile_info_watcher.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

HistoryLoginHandler::HistoryLoginHandler(const base::Closure& signin_callback)
    : signin_callback_(signin_callback) {}

HistoryLoginHandler::~HistoryLoginHandler() {}

void HistoryLoginHandler::RegisterMessages() {
  profile_info_watcher_ = std::make_unique<ProfileInfoWatcher>(
      Profile::FromWebUI(web_ui()),
      base::Bind(&HistoryLoginHandler::ProfileInfoChanged,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "otherDevicesInitialized",
      base::BindRepeating(&HistoryLoginHandler::HandleOtherDevicesInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startSignInFlow",
      base::BindRepeating(&HistoryLoginHandler::HandleStartSignInFlow,
                          base::Unretained(this)));
}

void HistoryLoginHandler::HandleOtherDevicesInitialized(
    const base::ListValue* /*args*/) {
  AllowJavascript();
  ProfileInfoChanged();
}

void HistoryLoginHandler::ProfileInfoChanged() {
  bool signed_in = !profile_info_watcher_->GetAuthenticatedUsername().empty();
  if (!signin_callback_.is_null())
    signin_callback_.Run();

  if (IsJavascriptAllowed())
    CallJavascriptFunction("updateSignInState", base::Value(signed_in));
}

void HistoryLoginHandler::HandleStartSignInFlow(
    const base::ListValue* /*args*/) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
      signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS, false);
}
