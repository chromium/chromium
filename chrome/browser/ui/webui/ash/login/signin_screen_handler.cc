// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/signin_screen_handler.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace {

// Timeout to delay first notification about offline state for a
// current network.
constexpr base::TimeDelta kOfflineTimeout = base::Seconds(1);

// Timeout used to prevent infinite connecting to a flaky network.
constexpr base::TimeDelta kConnectingTimeout = base::Seconds(60);

// Max number of Gaia Reload to Show Proxy Auth Dialog.
const int kMaxGaiaReloadForProxyAuthDialog = 3;

}  // namespace

namespace ash {

namespace {

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason,
                  net::Error frame_error) {
  return NetworkStateInformer::IsProxyError(state, reason) ||
         (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
          (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
           frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

}  // namespace

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen,
    GaiaScreenHandler* gaia_screen_handler)
    : network_state_informer_(network_state_informer),
      error_screen_(error_screen),
      proxy_auth_dialog_reload_times_(kMaxGaiaReloadForProxyAuthDialog),
      gaia_screen_handler_(gaia_screen_handler),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kSignin)) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_);
  gaia_screen_handler_->set_signin_screen_handler(this);
}

SigninScreenHandler::~SigninScreenHandler() {
  StopNetworkObservation();
}

void SigninScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void SigninScreenHandler::Show() {
  CHECK(ExistingUserController::current_controller());
  histogram_helper_->OnScreenShow();
}

void SigninScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  // ERROR_REASON_FRAME_ERROR is an explicit signal from GAIA frame so it shoud
  // force network error UI update.
  bool force_update = reason == NetworkError::ERROR_REASON_FRAME_ERROR;
  UpdateStateInternal(reason, force_update);
}

void SigninScreenHandler::SetOfflineTimeoutForTesting(
    base::TimeDelta offline_timeout) {
  is_offline_timeout_for_test_set_ = true;
  offline_timeout_for_test_ = offline_timeout;
}

// SigninScreenHandler, private: -----------------------------------------------

// TODO(antrim@): split this method into small parts.
// TODO(antrim@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(NetworkError::ErrorReason reason,
                                              bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(antrim): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so ExistingUserController
  // will be nullptr. Network state processing logic does not belong here.
  auto* existing_user_controller = ExistingUserController::current_controller();
  if (existing_user_controller &&
      (existing_user_controller->IsUserSigninCompleted() ||
       existing_user_controller->IsSigninInProgress())) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);

  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if ((state == NetworkStateInformer::OFFLINE ||
       network_state_ignored_until_proxy_auth_) &&
      !force_update && !update_state_callback_.IsCancelled()) {
    return;
  }

  update_state_callback_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      network_state_ignored_until_proxy_auth_) {
    update_state_callback_.Reset(
        base::BindOnce(&SigninScreenHandler::UpdateStateInternal,
                       weak_factory_.GetWeakPtr(), reason, true));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, update_state_callback_.callback(),
        is_offline_timeout_for_test_set_ ? offline_timeout_for_test_
                                         : kOfflineTimeout);
    return;
  }

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_callback_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_callback_.Reset(
          base::BindOnce(&SigninScreenHandler::UpdateStateInternal,
                         weak_factory_.GetWeakPtr(), reason, true));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, connecting_callback_.callback(), kConnectingTimeout);
    }
    return;
  }
  connecting_callback_.Cancel();

  const bool is_online = NetworkStateInformer::IsOnline(state, reason);
  const bool is_behind_captive_portal =
      NetworkStateInformer::IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool error_screen_should_overlay = IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;
  proxy_auth_dialog_need_reload_ =
      (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED) &&
      (state == NetworkStateInformer::PROXY_AUTH_REQUIRED) &&
      (proxy_auth_dialog_reload_times_ > 0);

  bool reload_gaia = false;

  if (is_online || !is_behind_captive_portal)
    error_screen_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    HideOfflineMessage(state, reason);
    return;
  }

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frame load since network has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia = true;
  }

  if (reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frameload since proxy settings has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia = true;
  }

  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      reason != gaia_reload_reason_ &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry frame load due to reason: "
                 << NetworkError::ErrorReasonString(reason);
    gaia_reload_reason_ = reason;
    reload_gaia = true;
  }

  if (is_gaia_loading_timeout) {
    LOG(WARNING) << "Retry frame load due to loading timeout.";
    reload_gaia = true;
  }

  if (proxy_auth_dialog_need_reload_) {
    --proxy_auth_dialog_reload_times_;
    LOG(WARNING) << "Retry frame load to show proxy auth dialog";
    reload_gaia = true;
  }

  if (!is_online || is_gaia_loading_timeout || is_gaia_error) {
    if (GetCurrentScreen() != ErrorScreenView::kScreenId) {
      error_screen_->SetParentScreen(GaiaView::kScreenId);
      error_screen_->SetHideCallback(base::BindOnce(
          &SigninScreenHandler::OnErrorScreenHide, weak_factory_.GetWeakPtr()));
      histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
    }
    error_screen_->ShowNetworkErrorMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);
  }

  if (reload_gaia)
    ReloadGaia(/*force_reload=*/true);
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             NetworkError::ErrorReason reason) {
  if (!IsGaiaHiddenByError())
    return;

  gaia_reload_reason_ = NetworkError::ERROR_REASON_NONE;

  error_screen_->Hide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaia(reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
}

void SigninScreenHandler::ReloadGaia(bool force_reload) {
  gaia_screen_handler_->ReloadGaia(force_reload);
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      network_state_ignored_until_proxy_auth_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED: {
      if (IsGaiaHiddenByError()) {
        // Start listening to network state notifications immediately, hoping
        // that the network will switch to ONLINE soon.
        update_state_callback_.Cancel();
        ReenableNetworkStateUpdatesAfterProxyAuth();
      } else {
        // Gaia is not hidden behind an error yet. Discard last cached network
        // state notification and wait for `kProxyAuthTimeout` before
        // considering network update notifications again (hoping the network
        // will become ONLINE by then).
        update_state_callback_.Cancel();
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth,
                weak_factory_.GetWeakPtr()),
            kProxyAuthTimeout);
      }
      break;
    }
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      update_state_callback_.Cancel();
      ReenableNetworkStateUpdatesAfterProxyAuth();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth() {
  network_state_ignored_until_proxy_auth_ = false;
}

void SigninScreenHandler::OnErrorScreenHide() {
  histogram_helper_->OnErrorHide();
  error_screen_->SetParentScreen(ash::OOBE_SCREEN_UNKNOWN);
  ReloadGaia(/*force_reload=*/true);
  ShowScreenDeprecated(GaiaView::kScreenId);
}

bool SigninScreenHandler::IsGaiaVisible() {
  return GetCurrentScreen() == GaiaView::kScreenId;
}

bool SigninScreenHandler::IsGaiaHiddenByError() {
  return (GetCurrentScreen() == ErrorScreenView::kScreenId) &&
         (error_screen_->GetParentScreen() == GaiaView::kScreenId);
}

net::Error SigninScreenHandler::FrameError() const {
  return gaia_screen_handler_->frame_error();
}

NetworkStateInformer::State
SigninScreenHandler::GetNetworkStateInformerStateForMigration() {
  return network_state_informer_->state();
}

void SigninScreenHandler::StartNetworkObservation() {
  network_state_informer_->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());
}

void SigninScreenHandler::StopNetworkObservation() {
  network_state_informer_->RemoveObserver(this);
  registrar_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace ash
