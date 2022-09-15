// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

NetworkPortalSigninController::NetworkPortalSigninController() = default;

NetworkPortalSigninController::~NetworkPortalSigninController() = default;

base::WeakPtr<NetworkPortalSigninController>
NetworkPortalSigninController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void NetworkPortalSigninController::ShowSignin() {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  bool use_incognito_profile =
      profile && profile->GetPrefs()->GetBoolean(
                     prefs::kCaptivePortalAuthenticationIgnoresProxy);

  if (use_incognito_profile) {
    ShowDialog();
  } else {
    if (!profile)
      return;
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    if (!displayer.browser())
      return;
    GURL url(captive_portal::CaptivePortalDetector::kDefaultURL);
    ShowSingletonTab(displayer.browser(), url);
  }
}

void NetworkPortalSigninController::CloseSignin() {
  if (dialog_)
    dialog_->Close();
}

bool NetworkPortalSigninController::DialogIsShown() {
  return !!dialog_;
}

void NetworkPortalSigninController::OnDialogDestroyed(
    const NetworkPortalWebDialog* dialog) {
  if (dialog != dialog_)
    return;
  dialog_ = nullptr;
  SigninProfileHandler::Get()->ClearSigninProfile(base::NullCallback());
}

void NetworkPortalSigninController::ShowDialog() {
  if (dialog_)
    return;

  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  dialog_ = new NetworkPortalWebDialog(web_dialog_weak_factory_.GetWeakPtr());
  dialog_->SetWidget(views::Widget::GetWidgetForNativeWindow(
      chrome::ShowWebDialog(nullptr, signin_profile, dialog_)));
}

}  // namespace ash
