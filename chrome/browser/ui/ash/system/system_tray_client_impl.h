// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_SYSTEM_TRAY_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_SYSTEM_TRAY_CLIENT_IMPL_H_

#include <optional>
#include <string_view>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/cpp/update_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system/system_clock_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace ash {
struct DeviceEnterpriseInfo;
struct LocaleInfo;
class SystemTray;
enum class LoginStatus;
enum class NotificationStyle;
}  // namespace ash

class Profile;

// Handles method calls delegated back to chrome from ash. Also notifies ash of
// relevant state changes in chrome.
class SystemTrayClientImpl : public ash::SystemTrayClient,
                             public ash::system::SystemClockObserver,
                             public policy::CloudPolicyStore::Observer,
                             public UpgradeObserver {
 public:
  SystemTrayClientImpl();

  SystemTrayClientImpl(const SystemTrayClientImpl&) = delete;
  SystemTrayClientImpl& operator=(const SystemTrayClientImpl&) = delete;

  ~SystemTrayClientImpl() override;

  static SystemTrayClientImpl* Get();

  // Specifies if notification is recommended or required by administrator and
  // triggers the notification to be shown with the given body and title.
  // Only applies to OS updates.
  virtual void SetRelaunchNotificationState(
      const ash::RelaunchNotificationState& relaunch_notification_state);

  // Resets update state to hide notification.
  void ResetUpdateState();

  // Wrappers around ash::mojom::SystemTray interface:
  void SetPrimaryTrayEnabled(bool enabled);
  void SetPrimaryTrayVisible(bool visible);
  void SetPerformanceTracingIconVisible(bool visible);
  void SetLocaleList(std::vector<ash::LocaleInfo> locale_list,
                     const std::string& current_locale_iso_code);
  void SetShowEolNotice(bool show, bool eol_passed_recently);

  // ash::SystemTrayClient:
  void ShowSettings(int64_t display_id) override;
  void ShowAccountSettings() override;
  void ShowBluetoothSettings() override;
  void ShowBluetoothSettings(const std::string& device_id) override;
  void ShowBluetoothPairingDialog(
      std::optional<std::string_view> device_address) override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowDarkModeSettings() override;
  void ShowStorageSettings() override;
  void ShowPowerSettings() override;
  void ShowPrivacyAndSecuritySettings() override;
  void ShowPrivacyHubSettings() override;
  void ShowSpeakOnMuteDetectionSettings() override;
  void ShowSmartPrivacySettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowConnectedDevicesSettings() override;
  void ShowTetherNetworkSettings() override;
  void ShowWifiSyncSettings() override;
  void ShowAboutChromeOS() override;
  void ShowAboutChromeOSDetails() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowColorCorrectionSettings() override;
  void ShowGestureEducationHelp() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowEnterpriseInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowSettingsCellularSetup(bool show_psim_flow) override;
  void ShowMobileDataSubpage() override;
  void ShowSettingsSimUnlock() override;
  void ShowApnSubpage(const std::string& network_id) override;
  void ShowThirdPartyVpnCreate(const std::string& extension_id) override;
  void ShowArcVpnCreate(const std::string& app_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowHotspotSubpage() override;
  void ShowMultiDeviceSetup() override;
  void ShowFirmwareUpdate() override;
  void SetLocaleAndExit(const std::string& locale_iso_code) override;
  void ShowAccessCodeCastingDialog(
      AccessCodeCastDialogOpenLocation open_location) override;
  void ShowCalendarEvent(const std::optional<GURL>& event_url,
                         const base::Time& date,
                         bool& opened_pwa,
                         GURL& finalized_event_url) override;
  void ShowVideoConference(const GURL& video_conference_url) override;
  void ShowChannelInfoAdditionalDetails() override;
  void ShowChannelInfoGiveFeedback() override;
  void ShowAudioSettings() override;
  void ShowEolInfoPage() override;
  void RecordEolNoticeShown() override;
  bool IsUserFeedbackEnabled() override;
  void ShowGraphicsTabletSettings() override;
  void ShowMouseSettings() override;
  void ShowTouchpadSettings() override;
  void ShowRemapKeysSubpage(int device_id) override;
  void ShowYouTubeMusicPremiumPage() override;
  void ShowChromebookPerksYouTubePage() override;
  void ShowKeyboardSettings() override;
  void ShowPointingStickSettings() override;
  void ShowNearbyShareSettings() override;

 protected:
  // Used by mocks in tests.
  explicit SystemTrayClientImpl(SystemTrayClientImpl* mock_instance);

 private:
  // Observes profile changed and profile's policy changed.
  class EnterpriseAccountObserver;

  // Helper function shared by ShowNetworkSettings() and ShowNetworkConfigure().
  void ShowNetworkSettingsHelper(const std::string& network_id,
                                 bool show_configure);

  // Requests that ash show the update available icon.
  void HandleUpdateAvailable();

  // ash::system::SystemClockObserver:
  void OnSystemClockChanged(ash::system::SystemClock* clock) override;

  // UpgradeObserver implementation.
  void OnUpdateDeferred(bool use_notification) override;
  void OnUpdateOverCellularAvailable() override;
  void OnUpdateOverCellularOneTimePermissionGranted() override;
  void OnUpgradeRecommended() override;

  // policy::CloudPolicyStore::Observer
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  void UpdateDeviceEnterpriseInfo();
  void UpdateEnterpriseAccountDomainInfo(Profile* profile);

  // The system tray model in ash.
  const raw_ptr<ash::SystemTray> system_tray_;

  // Information on whether the update is recommended or required.
  ash::RelaunchNotificationState relaunch_notification_state_;

  std::unique_ptr<ash::DeviceEnterpriseInfo> last_device_enterprise_info_;
  std::string last_enterprise_account_domain_manager_;

  std::unique_ptr<EnterpriseAccountObserver> enterprise_account_observer_;

  // Whether the eol incentive is due to a recently pasesed end of life date.
  bool eol_incentive_recently_passed_ = false;
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_SYSTEM_TRAY_CLIENT_IMPL_H_
