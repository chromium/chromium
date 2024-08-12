// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

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
  if (profile->IsOffTheRecord()) {
    return profile;
  }

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

class SigninWebDialogDelegate : public ui::WebDialogDelegate {
 public:
  explicit SigninWebDialogDelegate(GURL url) {
    set_can_close(true);
    set_can_resize(false);
    set_dialog_content_url(url);
    set_dialog_modal_type(ui::mojom::ModalType::kSystem);
    set_dialog_title(l10n_util::GetStringUTF16(
        IDS_CAPTIVE_PORTAL_AUTHORIZATION_DIALOG_NAME));
    set_show_dialog_title(true);

    const float kScale = 0.8;
    set_dialog_size(gfx::ScaleToRoundedSize(
        display::Screen::GetScreen()->GetPrimaryDisplay().size(), kScale));
  }

  ~SigninWebDialogDelegate() override = default;

  void OnLoadingStateChanged(content::WebContents* source) override {
    NetworkHandler::Get()->network_state_handler()->RequestPortalDetection();
  }
};

}  // namespace

// static
NetworkPortalSigninController* NetworkPortalSigninController::Get() {
  static base::NoDestructor<NetworkPortalSigninController> instance;
  return instance.get();
}

NetworkPortalSigninController::NetworkPortalSigninController() = default;

NetworkPortalSigninController::~NetworkPortalSigninController() = default;

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
      portal_state != NetworkState::PortalState::kPortalSuspected) {
    // If no portal signin is required, do not attempt to show the signin page.
    NET_LOG(EVENT) << "Show signin mode from: " << source << ": Network '"
                   << NetworkId(default_network)
                   << "' is in a non portal state: " << portal_state;
    return;
  }

  url = default_network->probe_url();
  if (url.is_empty()) {
    url = GURL(captive_portal::CaptivePortalDetector::kDefaultURL);
  }

  SigninMode mode = GetSigninMode(portal_state);
  NET_LOG(EVENT) << "Show signin mode: " << mode << " from: " << source;
  base::UmaHistogramEnumeration("Network.NetworkPortalSigninMode", mode);
  base::UmaHistogramEnumeration("Network.NetworkPortalSigninSource", source);

  signin_network_guid_ = default_network->guid();
  signin_start_time_ = base::TimeTicks::Now();
  if (!network_state_handler_observation_.IsObserving()) {
    network_state_handler_observation_.Observe(
        NetworkHandler::Get()->network_state_handler());
  }

  switch (mode) {
    case SigninMode::kSigninDialog:
      // OOBE/Login needs to show the portal signin UI in a dialog.
      ShowSigninDialog(url);
      break;
    case SigninMode::kNormalTab:
      if (chromeos::features::IsCaptivePortalPopupWindowEnabled()) {
        ShowActiveProfileTab(url);
      } else {
        ShowTab(ProfileManager::GetActiveUserProfile(), url);
      }
      break;
    case SigninMode::kSigninDefault: {
      if (chromeos::features::IsCaptivePortalPopupWindowEnabled()) {
        // An OTR profile will be used with extensions enabled and all proxies
        // disabled by the proxy service.
        ShowSigninWindow(url);
      } else {
        ShowTab(GetOTROrActiveProfile(), url);
      }
      break;
    }
    case SigninMode::kIncognitoDisabledByPolicy:
      ShowTab(ProfileManager::GetActiveUserProfile(), url);
      break;
    case SigninMode::kIncognitoDisabledByParentalControls: {
      // Supervised users require SupervisedUserNavigationThrottle which is
      // only available to non OTR profiles.
      ShowTab(ProfileManager::GetActiveUserProfile(), url);
      break;
    }
  }
}

NetworkPortalSigninController::SigninMode
NetworkPortalSigninController::GetSigninMode(
    NetworkState::PortalState portal_state) const {
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    NET_LOG(DEBUG) << "GetSigninMode: Not logged in";
    return SigninMode::kSigninDialog;
  }

  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    NET_LOG(DEBUG) << "GetSigninMode: Kiosk app";
    return SigninMode::kSigninDialog;
  }

  if (user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    NET_LOG(DEBUG) << "GetSigninMode: Child User";
    return SigninMode::kIncognitoDisabledByParentalControls;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    NET_LOG(DEBUG) << "GetSigninMode: No profile";
    return SigninMode::kSigninDialog;
  }

  // This pref defaults to true, but if a policy is active the policy value
  // defaults to false ("any captive portal authentication pages are shown in a
  // regular tab [if a proxy is active]").
  // Note: Generally we always want to show the portal signin UI in an OTR
  // tab to avoid providing cookies, see b/245578628 for details.
  const bool ignore_proxy = profile->GetPrefs()->GetBoolean(
      chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy);
  if (!ignore_proxy && ProxyActive(profile)) {
    return SigninMode::kNormalTab;
  }

  policy::IncognitoModeAvailability availability;
  IncognitoModePrefs::IntToAvailability(
      profile->GetPrefs()->GetInteger(
          policy::policy_prefs::kIncognitoModeAvailability),
      &availability);
  if (availability == policy::IncognitoModeAvailability::kDisabled) {
    return SigninMode::kIncognitoDisabledByPolicy;
  }

  return SigninMode::kSigninDefault;
}

void NetworkPortalSigninController::CloseSignin() {
  if (dialog_widget_) {
    dialog_widget_->Close();
  }
}

bool NetworkPortalSigninController::DialogIsShown() {
  return !!dialog_widget_;
}

void NetworkPortalSigninController::OnWidgetDestroying(views::Widget* widget) {
  if (widget != dialog_widget_) {
    return;
  }
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
  SigninProfileHandler::Get()->ClearSigninProfile(base::NullCallback());
}

void NetworkPortalSigninController::PortalStateChanged(
    const NetworkState* default_network,
    NetworkState::PortalState portal_state) {
  bool is_signin_network =
      default_network && default_network->guid() == signin_network_guid_;
  if (is_signin_network && !default_network->IsOnline()) {
    // Signin network is still not online, nothing to do.
    return;
  }

  if (!signin_network_guid_.empty()) {
    // If the signin network is online, record the time since the signin UI was
    // shown. Otherwise record 0 to indicate that signin did not occur.
    base::TimeDelta elapsed;
    if (is_signin_network) {
      elapsed = base::TimeTicks::Now() - signin_start_time_;
    }
    base::UmaHistogramMediumTimes("Network.NetworkPortalSigninTime", elapsed);
    signin_network_guid_ = "";
    network_state_handler_observation_.Reset();
  }

  // If signin is using a dialog in the OOBE/login screen, close it if the
  // default network changed or became online.
  if (dialog_widget_) {
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }

  // If signin is using a browser window, the user may still be using the window
  // so we don't try to close it.
}

void NetworkPortalSigninController::OnShuttingDown() {
  network_state_handler_observation_.Reset();
}

void NetworkPortalSigninController::ShowSigninDialog(const GURL& url) {
  if (dialog_widget_) {
    dialog_widget_->Show();
    return;
  }

  auto web_dialog_delegate = std::make_unique<SigninWebDialogDelegate>(url);

  dialog_widget_ = views::Widget::GetWidgetForNativeWindow(
      // ui::WebDialogDelegate is self-deleting, so pass ownership of it (as a
      // raw pointer) in here.
      chrome::ShowWebDialog(nullptr, ProfileHelper::GetSigninProfile(),
                            web_dialog_delegate.release()));
  dialog_widget_observation_.Observe(dialog_widget_.get());
}

void NetworkPortalSigninController::ShowSigninWindow(const GURL& url) {
  // Calls NetworkPortalSigninWindow::Show in the appropriate browser (Ash or
  // Lacros).
  ash::NewWindowDelegate::GetPrimary()->OpenCaptivePortalSignin(url);
}

void NetworkPortalSigninController::ShowTab(Profile* profile, const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  if (!displayer.browser()) {
    return;
  }

  NavigateParams params(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ::Navigate(&params);
}

void NetworkPortalSigninController::ShowActiveProfileTab(const GURL& url) {
  // Opens a new tab the appropriate browser (Ash or Lacros).
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

std::ostream& operator<<(
    std::ostream& stream,
    const NetworkPortalSigninController::SigninMode& signin_mode) {
  switch (signin_mode) {
    case NetworkPortalSigninController::SigninMode::kSigninDialog:
      stream << "Signin Dialog";
      break;
    case NetworkPortalSigninController::SigninMode::kNormalTab:
      stream << "Normal Tab (proxies enabled)";
      break;
    case NetworkPortalSigninController::SigninMode::kSigninDefault:
      stream << "Signin Window";
      break;
    case NetworkPortalSigninController::SigninMode::kIncognitoDisabledByPolicy:
      stream << "Signin Window (Incognito mode disabled by policy)";
      break;
    case NetworkPortalSigninController::SigninMode::
        kIncognitoDisabledByParentalControls:
      stream << "Signin Window (Incognito mode disabled by parental controls)";
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
