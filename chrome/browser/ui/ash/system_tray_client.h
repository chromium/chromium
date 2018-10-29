// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_

#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/system/system_clock_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
enum class LoginStatus;
}

namespace views {
class Widget;
class WidgetDelegate;
}  // namespace views

// Handles method calls delegated back to chrome from ash. Also notifies ash of
// relevant state changes in chrome.
// TODO: Consider renaming this to SystemTrayClientChromeOS.
class SystemTrayClient : public ash::mojom::SystemTrayClient,
                         public chromeos::system::SystemClockObserver,
                         public policy::CloudPolicyStore::Observer,
                         public UpgradeObserver {
 public:
  SystemTrayClient();
  ~SystemTrayClient() override;

  static SystemTrayClient* Get();

  // Returns the container id for the parent window for new dialogs. The parent
  // varies based on the current login and lock screen state.
  static int GetDialogParentContainerId();

  // Creates a modal dialog in the parent window for new dialogs on the primary
  // display. See GetDialogParentContainerId() and views::CreateDialogWidget().
  // The returned widget is owned by its native widget.
  static views::Widget* CreateUnownedDialogWidget(
      views::WidgetDelegate* widget_delegate);

  // Shows an update icon for an Adobe Flash update and forces a device reboot
  // when the update is applied.
  void SetFlashUpdateAvailable();

  // Specifies if notification is recommended or required by administrator and
  // triggers the notification to be shown with the given body and title.
  void SetUpdateNotificationState(ash::mojom::NotificationStyle style,
                                  const base::string16& notification_title,
                                  const base::string16& notification_body);

  // Wrappers around ash::mojom::SystemTray interface:
  void SetPrimaryTrayEnabled(bool enabled);
  void SetPrimaryTrayVisible(bool visible);
  void SetPerformanceTracingIconVisible(bool visible);

  // ash::mojom::SystemTrayClient:
  void ShowSettings() override;
  void ShowBluetoothSettings() override;
  void ShowBluetoothPairingDialog(const std::string& address,
                                  const base::string16& name_for_display,
                                  bool paired,
                                  bool connected) override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowPowerSettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowConnectedDevicesSettings() override;
  void ShowAboutChromeOS() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowPublicAccountInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowThirdPartyVpnCreate(const std::string& extension_id) override;
  void ShowArcVpnCreate(const std::string& app_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowMultiDeviceSetup() override;
  void RequestRestartForUpdate() override;

 private:
  // Helper function shared by ShowNetworkSettings() and ShowNetworkConfigure().
  void ShowNetworkSettingsHelper(const std::string& network_id,
                                 bool show_configure);

  // Requests that ash show the update available icon.
  void HandleUpdateAvailable();

  // chromeos::system::SystemClockObserver:
  void OnSystemClockChanged(chromeos::system::SystemClock* clock) override;

  // UpgradeObserver implementation.
  void OnUpdateOverCellularAvailable() override;
  void OnUpdateOverCellularOneTimePermissionGranted() override;
  void OnUpgradeRecommended() override;

  // policy::CloudPolicyStore::Observer
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  void UpdateEnterpriseDisplayDomain();

  // System tray mojo service in ash.
  ash::mojom::SystemTrayPtr system_tray_;

  // Binds this object to the client interface.
  mojo::Binding<ash::mojom::SystemTrayClient> binding_;

  // Whether an Adobe Flash component update is available.
  bool flash_update_available_ = false;

  // Tells update notification style, for example required by administrator.
  ash::mojom::NotificationStyle update_notification_style_ =
      ash::mojom::NotificationStyle::DEFAULT;

  // Update notification title to be overwritten.
  base::string16 update_notification_title_;

  // Update notification body to be overwritten.
  base::string16 update_notification_body_;

  // Avoid sending ash an empty enterprise display domain at startup and
  // suppress duplicate IPCs during the session.
  std::string last_enterprise_display_domain_;
  bool last_active_directory_managed_ = false;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayClient);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
