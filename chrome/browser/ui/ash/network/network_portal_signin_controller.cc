// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
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
  GURL url;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (default_network)
    url = default_network->probe_url();
  if (url.is_empty())
    url = GURL(captive_portal::CaptivePortalDetector::kDefaultURL);

  SigninMode mode = GetSigninMode();
  NET_LOG(EVENT) << "Show signin mode: " << mode;
  base::UmaHistogramEnumeration("Network.NetworkPortalSigninMode", mode);
  switch (mode) {
    case SigninMode::kSigninDialog:
      ShowDialog(ProfileHelper::GetSigninProfile(), url);
      break;
    case SigninMode::kSingletonTab:
      ShowSingletonTab(ProfileManager::GetActiveUserProfile(), url);
      break;
  }
}

NetworkPortalSigninController::SigninMode
NetworkPortalSigninController::GetSigninMode() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    // Login screen. Always show an incognito dialog.
    return SigninMode::kSigninDialog;
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kCaptivePortalAuthenticationIgnoresProxy)) {
    // If allowed, use an incognito dialog to ignore any proxies.
    return SigninMode::kSigninDialog;
  }
  return SigninMode::kSingletonTab;
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

void NetworkPortalSigninController::ShowDialog(Profile* profile,
                                               const GURL& url) {
  if (dialog_)
    return;

  dialog_ =
      new NetworkPortalWebDialog(url, web_dialog_weak_factory_.GetWeakPtr());
  dialog_->SetWidget(views::Widget::GetWidgetForNativeWindow(
      chrome::ShowWebDialog(nullptr, profile, dialog_)));
}

void NetworkPortalSigninController::ShowSingletonTab(Profile* profile,
                                                     const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  if (!displayer.browser())
    return;

  ::ShowSingletonTab(displayer.browser(), url);
}

std::ostream& operator<<(
    std::ostream& stream,
    const NetworkPortalSigninController::SigninMode& signin_mode) {
  switch (signin_mode) {
    case NetworkPortalSigninController::SigninMode::kSigninDialog:
      stream << "Signin Dialog";
      break;
    case NetworkPortalSigninController::SigninMode::kSingletonTab:
      stream << "Singleton Tab";
      break;
  }
  return stream;
}

}  // namespace ash
