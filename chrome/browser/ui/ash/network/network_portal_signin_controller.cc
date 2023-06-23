// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/user_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool ProxyActive(Profile* profile) {
  std::unique_ptr<ProxyConfigDictionary> proxy_config =
      ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          profile->GetPrefs(), g_browser_process->local_state());
  if (!proxy_config) {
    return false;
  }
  ProxyPrefs::ProxyMode mode;
  proxy_config->GetMode(&mode);
  if (mode == ProxyPrefs::MODE_DIRECT) {
    return false;
  }
  NET_LOG(DEBUG) << "GetSigninMode: Proxy config mode: " << mode;
  return true;
}

Profile* GetOTROrActiveProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  // In Guest mode, the active profile is OTR. Since we do not support creating
  // an OTR profile from another OTR profile we use the active profile for
  // captive portal signin.
  if (profile->IsOffTheRecord())
    return profile;

  // When not in Guest mode we use a separate signin OTR profile to avoid
  // passing existing OTR cookies to the captive portal signin page, see
  // b/245578628 for details.
  static base::NoDestructor<Profile::OTRProfileID> otr_profile_id(
      Profile::OTRProfileID::CreateUniqueForCaptivePortal());
  Profile* otr_profile =
      profile->GetOffTheRecordProfile(*otr_profile_id,
                                      /*create_if_needed=*/true);
  DCHECK(otr_profile);
  return otr_profile;
}

}  // namespace

NetworkPortalSigninController::NetworkPortalSigninController() = default;

NetworkPortalSigninController::~NetworkPortalSigninController() = default;

base::WeakPtr<NetworkPortalSigninController>
NetworkPortalSigninController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void NetworkPortalSigninController::ShowSignin(SigninSource source) {
  GURL url;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network) {
    // If no network is connected, do not attempt to show the signin page.
    NET_LOG(EVENT) << "Show signin mode from: " << source << ": No network.";
    return;
  }
  auto portal_state = default_network->GetPortalState();
  if (portal_state != NetworkState::PortalState::kPortal &&
      portal_state != NetworkState::PortalState::kPortalSuspected &&
      portal_state != NetworkState::PortalState::kProxyAuthRequired) {
    // If no portal or proxy signin is required, do not attempt to show the
    // signin page.
    NET_LOG(EVENT) << "Show signin mode from: " << source << ": Network '"
                   << default_network->guid()
                   << "' is in a non portal state: " << portal_state;
    return;
  }

  url = default_network->probe_url();
  if (url.is_empty())
    url = GURL(captive_portal::CaptivePortalDetector::kDefaultURL);

  SigninMode mode = GetSigninMode();
  NET_LOG(EVENT) << "Show signin mode: " << mode << " from: " << source;
  base::UmaHistogramEnumeration("Network.NetworkPortalSigninMode", mode);
  base::UmaHistogramEnumeration("Network.NetworkPortalSigninSource", source);
  switch (mode) {
    case SigninMode::kSigninDialog:
      ShowDialog(ProfileHelper::GetSigninProfile(), url);
      break;
    case SigninMode::kNormalTab:
      ShowTab(ProfileManager::GetActiveUserProfile(), url);
      break;
    case SigninMode::kIncognitoTab: {
      ShowTab(GetOTROrActiveProfile(), url);
      break;
    }
    case SigninMode::kIncognitoDialogDisabled:
    case SigninMode::kIncognitoDialogParental: {
      // TODO(b/271942666): Remove these modes entirely.
      ShowTab(ProfileManager::GetActiveUserProfile(), url);
      break;
    }
  }
}

NetworkPortalSigninController::SigninMode
NetworkPortalSigninController::GetSigninMode() const {
  if (!user_manager::UserManager::IsInitialized()
      || !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    NET_LOG(DEBUG) << "GetSigninMode: Not logged in";
    return SigninMode::kSigninDialog;
  }

  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    NET_LOG(DEBUG) << "GetSigninMode: Kiosk app";
    return SigninMode::kSigninDialog;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    // Login screen. Always show a dialog using the signin profile.
    NET_LOG(DEBUG) << "GetSigninMode: No profile";
    return SigninMode::kSigninDialog;
  }

  // This pref defaults to true, but if a policy is active the policy value
  // defaults to false ("any captive portal authentication pages are shown in a
  // regular tab [if a proxy is active]").
  // Note: Generally we always want to show the portal signin UI in an incognito
  // tab to avoid providing cookies, see b/245578628 for details.
  const bool ignore_proxy = profile->GetPrefs()->GetBoolean(
      prefs::kCaptivePortalAuthenticationIgnoresProxy);
  if (!ignore_proxy && ProxyActive(profile)) {
    return SigninMode::kNormalTab;
  }

  policy::IncognitoModeAvailability availability;
  IncognitoModePrefs::IntToAvailability(
      profile->GetPrefs()->GetInteger(
          policy::policy_prefs::kIncognitoModeAvailability),
      &availability);
  if (availability == policy::IncognitoModeAvailability::kDisabled) {
    // Use a dialog to prevent navigation and use an OTR profile due to
    // Incognito browsing disabled by policy preference.
    return SigninMode::kIncognitoDialogDisabled;
  }

  if (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
      policy::IncognitoModeAvailability::kDisabled) {
    // Use a dialog to prevent navigation and use an OTR profile due to
    // Incognito browsing disabled by parental controls.
    return SigninMode::kIncognitoDialogParental;
  }

  // Show an incognito tab to ignore any proxies.
  return SigninMode::kIncognitoTab;
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

void NetworkPortalSigninController::ShowTab(Profile* profile, const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  if (!displayer.browser())
    return;

  NavigateParams params(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ::Navigate(&params);
}

std::ostream& operator<<(
    std::ostream& stream,
    const NetworkPortalSigninController::SigninMode& signin_mode) {
  switch (signin_mode) {
    case NetworkPortalSigninController::SigninMode::kSigninDialog:
      stream << "Signin Dialog";
      break;
    case NetworkPortalSigninController::SigninMode::kNormalTab:
      stream << "Normal Tab";
      break;
    case NetworkPortalSigninController::SigninMode::kIncognitoTab:
      stream << "OTR Tab";
      break;
    case NetworkPortalSigninController::SigninMode::kIncognitoDialogDisabled:
      stream << "Incognito mode disabled, showing in normal tab";
      break;
    case NetworkPortalSigninController::SigninMode::kIncognitoDialogParental:
      stream << "Parental mode disables Incognito, showing in normal tab";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const NetworkPortalSigninController::SigninSource& signin_source) {
  switch (signin_source) {
    case NetworkPortalSigninController::SigninSource::kNotification:
      stream << "Notification";
      break;
    case NetworkPortalSigninController::SigninSource::kSettings:
      stream << "Settings";
      break;
    case NetworkPortalSigninController::SigninSource::kQuickSettings:
      stream << "Quick Settings";
      break;
    case NetworkPortalSigninController::SigninSource::kErrorPage:
      stream << "Error page";
      break;
  }
  return stream;
}

}  // namespace ash
