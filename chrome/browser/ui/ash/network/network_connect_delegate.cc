// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_connect_delegate.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ui/ash/network/enrollment_dialog_view.h"
#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"
#include "chrome/browser/ui/ash/network/network_state_notifier.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/mobile_setup_dialog.h"

namespace {

bool IsUIAvailable() {
  // UI is available when screen is unlocked.
  return !ash::ScreenLocker::default_screen_locker() ||
         !ash::ScreenLocker::default_screen_locker()->locked();
}

}  // namespace

NetworkConnectDelegate::NetworkConnectDelegate()
    : network_state_notifier_(std::make_unique<ash::NetworkStateNotifier>()) {}

NetworkConnectDelegate::~NetworkConnectDelegate() = default;

void NetworkConnectDelegate::ShowNetworkConfigure(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowNetworkConfigure(network_id);
}

void NetworkConnectDelegate::ShowNetworkSettings(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowNetworkSettings(network_id);
}

bool NetworkConnectDelegate::ShowEnrollNetwork(const std::string& network_id) {
  if (!IsUIAvailable())
    return false;
  return ash::enrollment::CreateEnrollmentDialog(network_id);
}

void NetworkConnectDelegate::ShowMobileSetupDialog(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowSettingsCellularSetup(
      /*show_psim_flow=*/true);
}

void NetworkConnectDelegate::ShowCarrierAccountDetail(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  ash::cellular_setup::MobileSetupDialog::ShowByNetworkId(network_id);
}

void NetworkConnectDelegate::ShowPortalSignin(
    const std::string& network_id,
    ash::NetworkConnect::Source source) {
  if (!IsUIAvailable())
    return;
  ash::NetworkPortalSigninController::SigninSource signin_source;
  switch (source) {
    case ash::NetworkConnect::Source::kSettings:
      signin_source =
          ash::NetworkPortalSigninController::SigninSource::kSettings;
      break;
    case ash::NetworkConnect::Source::kQuickSettings:
      signin_source =
          ash::NetworkPortalSigninController::SigninSource::kQuickSettings;
      break;
  }
  ash::NetworkPortalSigninController::Get()->ShowSignin(signin_source);
}

void NetworkConnectDelegate::ShowNetworkConnectError(
    const std::string& error_name,
    const std::string& network_id) {
  network_state_notifier_->ShowNetworkConnectErrorForGuid(error_name,
                                                          network_id);
}

void NetworkConnectDelegate::ShowMobileActivationError(
    const std::string& network_id) {
  network_state_notifier_->ShowMobileActivationErrorForGuid(network_id);
}

void NetworkConnectDelegate::ShowCarrierUnlockNotification() {
  if (!IsUIAvailable()) {
    return;
  }
  network_state_notifier_->ShowCarrierUnlockNotification();
}

void NetworkConnectDelegate::SetSystemTrayClient(
    ash::SystemTrayClient* system_tray_client) {
  network_state_notifier_->set_system_tray_client(system_tray_client);
}
