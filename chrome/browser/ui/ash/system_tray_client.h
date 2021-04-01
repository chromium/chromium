// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_

#include "ash/public/cpp/system_tray_client.h"
#include "base/macros.h"
#include "chrome/browser/ash/system/system_clock_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace ash {
struct LocaleInfo;
class SystemTray;
enum class LoginStatus;
enum class NotificationStyle;
enum class UpdateType;
}  // namespace ash

class Profile;

// Handles method calls delegated back to chrome from ash. Also notifies ash of
// relevant state changes in chrome.
// TODO: Consider renaming this to SystemTrayClientImpl.
class SystemTrayClient : public ash::SystemTrayClient,
                         public ash::system::SystemClockObserver,
                         public policy::CloudPolicyStore::Observer,
                         public UpgradeObserver {
 public:
  SystemTrayClient();
  ~SystemTrayClient() override;

  static SystemTrayClient* Get();

  // Specifies if notification is recommended or required by administrator and
  // triggers the notification to be shown with the given body and title.
  // Only applies to OS updates.
  void SetUpdateNotificationState(ash::NotificationStyle style,
                                  const std::u16string& notification_title,
                                  const std::u16string& notification_body);

  // Shows a notification that a Lacros browser update is available.
  void SetLacrosUpdateAvailable();

  // Wrappers around ash::mojom::SystemTray interface:
  void SetPrimaryTrayEnabled(bool enabled);
  void SetPrimaryTrayVisible(bool visible);
  void SetPerformanceTracingIconVisible(bool visible);
  void SetLocaleList(std::vector<ash::LocaleInfo> locale_list,
                     const std::string& current_locale_iso_code);

  // ash::SystemTrayClient:
  void ShowSettings(int64_t display_id) override;
  void ShowBluetoothSettings() override;
  void ShowBluetoothPairingDialog(const std::string& address,
                                  const std::u16string& name_for_display,
                                  bool paired,
                                  bool connected) override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowPowerSettings() override;
  void ShowPrivacyAndSecuritySettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowConnectedDevicesSettings() override;
  void ShowTetherNetworkSettings() override;
  void ShowWifiSyncSettings() override;
  void ShowAboutChromeOS() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowGestureEducationHelp() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowPublicAccountInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowSettingsCellularSetup(bool show_psim_flow) override;
  void ShowSettingsSimUnlock() override;
  void ShowThirdPartyVpnCreate(const std::string& extension_id) override;
  void ShowArcVpnCreate(const std::string& app_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowMultiDeviceSetup() override;
  void RequestRestartForUpdate() override;
  void SetLocaleAndExit(const std::string& locale_iso_code) override;

 private:
  // Observes profile changed and profile's policy changed.
  class EnterpriseAccountObserver;

  // Helper function shared by ShowNetworkSettings() and ShowNetworkConfigure().
  void ShowNetworkSettingsHelper(const std::string& network_id,
                                 bool show_configure);

  // Requests that ash show the update available icon.
  void HandleUpdateAvailable(ash::UpdateType update_type);

  // ash::system::SystemClockObserver:
  void OnSystemClockChanged(ash::system::SystemClock* clock) override;

  // UpgradeObserver implementation.
  void OnUpdateOverCellularAvailable() override;
  void OnUpdateOverCellularOneTimePermissionGranted() override;
  void OnUpgradeRecommended() override;

  // policy::CloudPolicyStore::Observer
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  void UpdateEnterpriseDomainInfo();
  void UpdateEnterpriseAccountDomainInfo(Profile* profile);

  // The system tray model in ash.
  ash::SystemTray* const system_tray_;

  // Tells update notification style, for example required by administrator.
  ash::NotificationStyle update_notification_style_;

  // Update notification title to be overwritten.
  std::u16string update_notification_title_;

  // Update notification body to be overwritten.
  std::u16string update_notification_body_;

  // Avoid sending ash an empty enterprise domain manager at startup and
  // suppress duplicate IPCs during the session.
  std::string last_enterprise_domain_manager_;
  bool last_active_directory_managed_ = false;
  std::string last_enterprise_account_domain_manager_;

  std::unique_ptr<EnterpriseAccountObserver> enterprise_account_observer_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayClient);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
