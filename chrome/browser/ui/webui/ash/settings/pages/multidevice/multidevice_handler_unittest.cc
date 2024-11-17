// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/multidevice/multidevice_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/webui/eche_app_ui/fake_apps_access_manager.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/fake_camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

constexpr char kDialogIntroActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents.IntroScreen";
constexpr char kDialogFinishOnPhoneActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents."
    "FinishSetupOnYourPhoneScreen";
constexpr char kDialogConnectingActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents.ConnectingToPhoneScreen";
constexpr char kDialogConnectionErrorActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents."
    "CouldNotEstablishConnectionScreen";
constexpr char kDialogConnectionTimeOutActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents.ConnectionLostScreen";
constexpr char kDialogSetAPinOrPasswordHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents.SetAPinOrPasswordScreen";
constexpr char kDialogSetupFinishedActionHistogram[] =
    "PhoneHub.PermissionsOnboarding.DialogScreenEvents.SetUpFinishedScreen";
constexpr char kSetupButtonInSettingsClikedHistogram[] =
    "PhoneHub.PermissionsOnboarding.SetUpMode.OnSettingsClicked";
constexpr char kDialogIntroScreenSetupModeHistogram[] =
    "PhoneHub.PermissionsOnboarding.SetUpMode.IntroScreenShown";
constexpr char kDialogSetUpFinishedScreenSetupModeHistogram[] =
    "PhoneHub.PermissionsOnboarding.SetUpMode.SetUpFinishedScreenShown";

using ::testing::Optional;

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class TestMultideviceHandler : public MultideviceHandler {
 public:
  TestMultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::MultideviceFeatureAccessManager*
          multidevice_feature_access_manager,
      eche_app::AppsAccessManager* apps_access_manager,
      phonehub::CameraRollManager* camera_roll_manager,
      phonehub::BrowserTabsModelProvider* browser_tabs_model_provider)
      : MultideviceHandler(prefs,
                           multidevice_setup_client,
                           multidevice_feature_access_manager,
                           apps_access_manager,
                           camera_roll_manager,
                           browser_tabs_model_provider) {}
  ~TestMultideviceHandler() override = default;

  // Make public for testing.
  using MultideviceHandler::AllowJavascript;
  using MultideviceHandler::RegisterMessages;
  using MultideviceHandler::set_web_ui;
};

multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
GenerateDefaultFeatureStatesMap() {
  return multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap{
      {multidevice_setup::mojom::Feature::kBetterTogetherSuite,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kInstantTethering,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kSmartLock,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kPhoneHub,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kPhoneHubNotifications,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kWifiSync,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts},
      {multidevice_setup::mojom::Feature::kEche,
       multidevice_setup::mojom::FeatureState::
           kUnavailableNoVerifiedHost_NoEligibleHosts}};
}

void VerifyPageContentDict(
    const base::Value* value,
    multidevice_setup::mojom::HostStatus expected_host_status,
    const std::optional<multidevice::RemoteDeviceRef>& expected_host_device,
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map,
    bool expected_is_nearby_share_disallowed_by_policy_,
    bool expected_is_phone_hub_apps_access_granted_,
    bool expected_is_camera_roll_file_permission_granted_,
    bool expected_is_camera_roll_access_status_granted_,
    bool expected_is_feature_setup_request_supported_) {
  ASSERT_TRUE(value->is_dict());
  const base::Value::Dict& page_content_dict = value->GetDict();

  std::optional<int> mode = page_content_dict.FindInt("mode");
  ASSERT_TRUE(mode);
  EXPECT_EQ(static_cast<int>(expected_host_status), *mode);

  std::optional<int> better_together_state =
      page_content_dict.FindInt("betterTogetherState");
  ASSERT_TRUE(better_together_state);
  auto it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite);
  EXPECT_EQ(static_cast<int>(it->second), *better_together_state);

  std::optional<int> instant_tethering_state =
      page_content_dict.FindInt("instantTetheringState");
  ASSERT_TRUE(instant_tethering_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kInstantTethering);
  EXPECT_EQ(static_cast<int>(it->second), *instant_tethering_state);

  std::optional<int> smart_lock_state =
      page_content_dict.FindInt("smartLockState");
  ASSERT_TRUE(smart_lock_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kSmartLock);
  EXPECT_EQ(static_cast<int>(it->second), *smart_lock_state);

  std::optional<int> phone_hub_state =
      page_content_dict.FindInt("phoneHubState");
  ASSERT_TRUE(phone_hub_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kPhoneHub);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_state);

  std::optional<int> phone_hub_notifications_state =
      page_content_dict.FindInt("phoneHubNotificationsState");
  ASSERT_TRUE(phone_hub_notifications_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubNotifications);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_notifications_state);

  std::optional<int> phone_hub_camera_roll_state =
      page_content_dict.FindInt("phoneHubCameraRollState");
  ASSERT_TRUE(phone_hub_camera_roll_state);
  if (base::FeatureList::IsEnabled(ash::features::kPhoneHubCameraRoll)) {
    it = feature_states_map.find(
        multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
    EXPECT_EQ(static_cast<int>(it->second), *phone_hub_camera_roll_state);
  } else {
    EXPECT_EQ(
        static_cast<int>(
            multidevice_setup::mojom::FeatureState::kNotSupportedByChromebook),
        *phone_hub_camera_roll_state);
  }

  std::optional<int> phone_hub_task_continuation_state =
      page_content_dict.FindInt("phoneHubTaskContinuationState");
  ASSERT_TRUE(phone_hub_task_continuation_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_task_continuation_state);

  std::optional<int> phone_hub_apps_state =
      page_content_dict.FindInt("phoneHubAppsState");
  ASSERT_TRUE(phone_hub_apps_state);
  if (base::FeatureList::IsEnabled(ash::features::kEcheSWA)) {
    it = feature_states_map.find(multidevice_setup::mojom::Feature::kEche);
    EXPECT_EQ(static_cast<int>(it->second), *phone_hub_apps_state);
  } else {
    EXPECT_EQ(
        static_cast<int>(
            multidevice_setup::mojom::FeatureState::kNotSupportedByChromebook),
        *phone_hub_apps_state);
  }

  std::optional<int> wifi_sync_state =
      page_content_dict.FindInt("wifiSyncState");
  ASSERT_TRUE(wifi_sync_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kWifiSync);
  EXPECT_EQ(static_cast<int>(it->second), *wifi_sync_state);

  const std::string* host_device_name =
      page_content_dict.FindString("hostDeviceName");
  if (expected_host_device) {
    ASSERT_TRUE(host_device_name);
    EXPECT_EQ(expected_host_device->name(), *host_device_name);
  } else {
    EXPECT_FALSE(host_device_name);
  }

  EXPECT_THAT(page_content_dict.FindBool("isNearbyShareDisallowedByPolicy"),
              Optional(expected_is_nearby_share_disallowed_by_policy_));

  EXPECT_THAT(page_content_dict.FindInt("appsAccessStatus"),
              Optional(expected_is_phone_hub_apps_access_granted_ ? 2 : 1));

  EXPECT_THAT(page_content_dict.FindBool("isCameraRollFilePermissionGranted"),
              Optional(expected_is_camera_roll_file_permission_granted_));

  EXPECT_THAT(
      page_content_dict.FindBool("isPhoneHubPermissionsDialogSupported"),
      Optional(features::IsEcheSWAEnabled() ||
               features::IsPhoneHubCameraRollEnabled()));

  EXPECT_THAT(page_content_dict.FindInt("cameraRollAccessStatus"),
              Optional(expected_is_camera_roll_access_status_granted_ ? 2 : 1));

  EXPECT_THAT(
      page_content_dict.FindBool("isPhoneHubFeatureCombinedSetupSupported"),
      Optional(expected_is_feature_setup_request_supported_));
}

}  // namespace

class MultideviceHandlerTest : public testing::Test {
 public:
  MultideviceHandlerTest(const MultideviceHandlerTest&) = delete;
  MultideviceHandlerTest& operator=(const MultideviceHandlerTest&) = delete;

 protected:
  MultideviceHandlerTest()
      : test_device_(multidevice::CreateRemoteDeviceRefForTest()) {}
  ~MultideviceHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_feature_access_manager_ =
        std::make_unique<phonehub::FakeMultideviceFeatureAccessManager>(
            phonehub::MultideviceFeatureAccessManager::AccessStatus::
                kAvailableButNotGranted);
    fake_apps_access_manager_ =
        std::make_unique<eche_app::FakeAppsAccessManager>(
            phonehub::MultideviceFeatureAccessManager::AccessStatus::
                kAvailableButNotGranted);
    fake_camera_roll_manager_ =
        std::make_unique<phonehub::FakeCameraRollManager>();

    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterNearbySharingPrefs(prefs_->registry());
    prefs_->SetBoolean(::prefs::kNearbySharingEnabledPrefName, true);
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(true);

    prefs_->registry()->RegisterBooleanPref(
        multidevice_setup::kInstantTetheringAllowedPrefName,
        /*default_value=*/true);
    prefs_->registry()->RegisterBooleanPref(ash::prefs::kEnableAutoScreenLock,
                                            false);
    prefs_->registry()->RegisterIntegerPref(
        phonehub::prefs::kScreenLockStatus,
        static_cast<int>(phonehub::ScreenLockManager::LockStatus::kLockedOff));

    CreateHandler();
  }

  void CreateHandler() {
    handler_ = std::make_unique<TestMultideviceHandler>(
        prefs_.get(), fake_multidevice_setup_client_.get(),
        fake_multidevice_feature_access_manager_.get(),
        fake_apps_access_manager_.get(), fake_camera_roll_manager_.get(),
        &fake_browser_tabs_model_provider_);

    test_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&test_profile_));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(test_web_contents_.get());
    handler_->set_web_ui(test_web_ui_.get());

    handler_->RegisterMessages();
    handler_->AllowJavascript();
  }

  void InitWithFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpHandlerWithEmptyManagers() {
    handler_.reset();
    test_web_ui_.reset();
    handler_ = std::make_unique<TestMultideviceHandler>(
        prefs_.get(), fake_multidevice_setup_client_.get(), nullptr, nullptr,
        nullptr, nullptr);

    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(test_web_contents_.get());
    handler_->set_web_ui(test_web_ui_.get());

    handler_->RegisterMessages();
    handler_->AllowJavascript();
  }

  void CallGetPageContentData() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::Value::List args;
    args.Append("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getPageContentData", args);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    VerifyPageContent(call_data.arg3());
  }

  void CallRemoveHostDevice() {
    size_t num_remote_host_device_calls_before_call =
        fake_multidevice_setup_client()->num_remove_host_device_called();
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("removeHostDevice", empty_args);
    EXPECT_EQ(num_remote_host_device_calls_before_call + 1u,
              fake_multidevice_setup_client()->num_remove_host_device_called());
  }

  void CallAttemptNotificationSetup(bool has_notification_access_been_granted) {
    fake_multidevice_feature_access_manager()
        ->SetNotificationAccessStatusInternal(
            has_notification_access_been_granted
                ? phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAccessGranted
                : phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted,
            phonehub::MultideviceFeatureAccessManager::AccessProhibitedReason::
                kUnknown);
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("attemptNotificationSetup",
                                         empty_args);
  }

  void CallCancelNotificationSetup() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("cancelNotificationSetup", empty_args);
  }

  void CallAttemptAppsSetup(bool has_access_been_granted) {
    fake_apps_access_manager()->SetAccessStatusInternal(
        has_access_been_granted ? phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAccessGranted
                                : phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAvailableButNotGranted);
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("attemptAppsSetup", empty_args);
  }

  void CallCancelAppsSetup() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("cancelAppsSetup", empty_args);
  }

  void CallAttemptCameraRollSetup(bool has_camera_roll_access_been_granted) {
    fake_multidevice_feature_access_manager()
        ->SetCameraRollAccessStatusInternal(
            has_camera_roll_access_been_granted
                ? phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAccessGranted
                : phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted);
    base::Value::List args;
    args.Append(/*camera_roll=*/true);
    args.Append(/*notifications=*/false);
    test_web_ui()->HandleReceivedMessage("attemptCombinedFeatureSetup", args);
  }

  void CallCancelCameraRollSetup() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("cancelCombinedFeatureSetup",
                                         empty_args);
  }

  void CalAttempFeatureSetupConnection() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("attemptFeatureSetupConnection",
                                         empty_args);
  }

  void CallCancelFeatureSetupConnection() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("cancelFeatureSetupConnection",
                                         empty_args);
  }

  void CallHandleShowBrowserSyncSettings() {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("showBrowserSyncSettings", empty_args);
  }

  void SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        std::make_pair(host_status, host_device));
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateFeatureStatesUpdate(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->SetFeatureStates(feature_states_map);
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateNearbyShareEnabledPrefChange(bool is_enabled, bool is_managed) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();
    size_t expected_call_count = call_data_count_before_call;
    bool did_managed_change =
        is_managed !=
        prefs_->IsManagedPreference(::prefs::kNearbySharingEnabledPrefName);
    bool did_enabled_change =
        is_enabled !=
        prefs_->GetBoolean(::prefs::kNearbySharingEnabledPrefName);

    if (is_managed) {
      prefs_->SetManagedPref(::prefs::kNearbySharingEnabledPrefName,
                             std::make_unique<base::Value>(is_enabled));
      EXPECT_TRUE(
          prefs_->IsManagedPreference(::prefs::kNearbySharingEnabledPrefName));
      if (did_managed_change) {
        ++expected_call_count;
      }
    } else {
      prefs_->RemoveManagedPref(::prefs::kNearbySharingEnabledPrefName);
      EXPECT_FALSE(
          prefs_->IsManagedPreference(::prefs::kNearbySharingEnabledPrefName));
      if (did_managed_change) {
        ++expected_call_count;
      }

      prefs_->SetBoolean(::prefs::kNearbySharingEnabledPrefName, is_enabled);
      if (did_enabled_change) {
        ++expected_call_count;
      }
    }
    EXPECT_EQ(is_enabled,
              prefs_->GetBoolean(::prefs::kNearbySharingEnabledPrefName));

    EXPECT_EQ(expected_call_count, test_web_ui()->call_data().size());

    if (expected_call_count == call_data_count_before_call) {
      return;
    }

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(expected_call_count - 1);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());

    expected_is_nearby_share_disallowed_by_policy_ = !is_enabled && is_managed;
    VerifyPageContent(call_data.arg2());
  }

  void SimulateAppsAccessStatusChanged(bool has_access_been_granted) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    phonehub::MultideviceFeatureAccessManager::AccessStatus apps_access_status =
        has_access_been_granted ? phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAccessGranted
                                : phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAvailableButNotGranted;
    fake_apps_access_manager()->SetAccessStatusInternal(apps_access_status);
    expected_is_phone_hub_apps_access_granted_ = has_access_been_granted;

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateCameraRollFilePermissionChanged(bool file_permission_granted) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_camera_roll_manager()->SetIsAndroidStorageGranted(
        file_permission_granted);
    expected_is_camera_roll_file_permission_granted_ = file_permission_granted;

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateCameraRollAccessstatusChanged(
      bool has_camera_roll_access_status_granted) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_feature_access_manager()
        ->SetCameraRollAccessStatusInternal(
            has_camera_roll_access_status_granted
                ? phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAccessGranted
                : phonehub::MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted);
    expected_is_camera_roll_access_status_granted_ =
        has_camera_roll_access_status_granted;
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void CallRetryPendingHostSetup(bool success) {
    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("retryPendingHostSetup", empty_args);
    fake_multidevice_setup_client()->InvokePendingRetrySetHostNowCallback(
        success);
  }

  void CallSetFeatureEnabledState(multidevice_setup::mojom::Feature feature,
                                  bool enabled,
                                  const std::optional<std::string>& auth_token,
                                  bool success) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::Value::List args;
    args.Append("handlerFunctionName");
    args.Append(static_cast<int>(feature));
    args.Append(enabled);
    if (auth_token) {
      args.Append(*auth_token);
    }

    base::Value::List empty_args;
    test_web_ui()->HandleReceivedMessage("setFeatureEnabledState", args);
    fake_multidevice_setup_client()
        ->InvokePendingSetFeatureEnabledStateCallback(
            feature /* expected_feature */, enabled /* expected_enabled */,
            auth_token /* expected_auth_token */, success);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(success, call_data.arg3()->GetBool());
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *test_web_ui_->call_data()[index];
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return fake_multidevice_setup_client_.get();
  }

  phonehub::FakeMultideviceFeatureAccessManager*
  fake_multidevice_feature_access_manager() {
    return fake_multidevice_feature_access_manager_.get();
  }

  eche_app::FakeAppsAccessManager* fake_apps_access_manager() {
    return fake_apps_access_manager_.get();
  }

  phonehub::FakeCameraRollManager* fake_camera_roll_manager() {
    return fake_camera_roll_manager_.get();
  }

  void SimulateNotificationOptInStatusChange(
      phonehub::NotificationAccessSetupOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_feature_access_manager()
        ->SetNotificationSetupOperationStatus(status);

    bool completed_successfully = status ==
                                  phonehub::NotificationAccessSetupOperation::
                                      Status::kCompletedSuccessfully;
    if (completed_successfully) {
      call_data_count_before_call++;
    }

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.onNotificationAccessSetupStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_EQ(call_data.arg2()->GetInt(), static_cast<int32_t>(status));
  }

  bool IsNotificationAccessSetupOperationInProgress() {
    return fake_multidevice_feature_access_manager()
        ->IsNotificationSetupOperationInProgress();
  }

  void SimulateAppsOptInStatusChange(
      eche_app::AppsAccessSetupOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_apps_access_manager()->SetAppsSetupOperationStatus(status);

    bool completed_successfully =
        status ==
        eche_app::AppsAccessSetupOperation::Status::kCompletedSuccessfully;
    if (completed_successfully) {
      call_data_count_before_call++;
    }

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.onAppsAccessSetupStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_EQ(call_data.arg2()->GetInt(), static_cast<int32_t>(status));
  }

  bool IsAppsAccessSetupOperationInProgress() {
    return fake_apps_access_manager()->IsSetupOperationInProgress();
  }

  void SimulateCameraRollOptInStatusChange(
      phonehub::CombinedAccessSetupOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_feature_access_manager()->SetCombinedSetupOperationStatus(
        status);

    bool completed_successfully =
        status ==
        phonehub::CombinedAccessSetupOperation::Status::kCompletedSuccessfully;
    if (completed_successfully) {
      call_data_count_before_call++;
    }

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.onCombinedAccessSetupStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_EQ(call_data.arg2()->GetInt(), static_cast<int32_t>(status));
  }

  void SimulateFeatureSetupConnectionStatusChange(
      phonehub::FeatureSetupConnectionOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();
    fake_multidevice_feature_access_manager()
        ->SetFeatureSetupConnectionOperationStatus(status);

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.onFeatureSetupConnectionStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_EQ(call_data.arg2()->GetInt(), static_cast<int32_t>(status));
  }

  bool IsFeatureSetupConnectionOperationInProgress() {
    return fake_multidevice_feature_access_manager()
        ->IsFeatureSetupConnectionOperationInProgress();
  }

  void SimulateEnableScreenLockChanged() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    prefs_->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.OnEnableScreenLockChanged",
              call_data.arg1()->GetString());
  }

  void SimulateScreenLockStatusChanged() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    prefs_->SetInteger(
        phonehub::prefs::kScreenLockStatus,
        static_cast<int>(phonehub::ScreenLockManager::LockStatus::kLockedOn));

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.OnScreenLockStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
  }

  bool IsCameraRollAccessSetupOperationInProgress() {
    return fake_multidevice_feature_access_manager()
        ->IsCombinedSetupOperationInProgress();
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  const multidevice::RemoteDeviceRef test_device_;

  bool expected_is_nearby_share_disallowed_by_policy_ = false;
  bool expected_is_phone_hub_apps_access_granted_ = false;
  bool expected_is_camera_roll_file_permission_granted_ = true;
  bool expected_is_camera_roll_access_status_granted_ = false;
  bool expected_is_feature_setup_request_supported_ = false;

 private:
  void VerifyPageContent(const base::Value* value) {
    VerifyPageContentDict(
        value, fake_multidevice_setup_client_->GetHostStatus().first,
        fake_multidevice_setup_client_->GetHostStatus().second,
        fake_multidevice_setup_client_->GetFeatureStates(),
        expected_is_nearby_share_disallowed_by_policy_,
        expected_is_phone_hub_apps_access_granted_,
        expected_is_camera_roll_file_permission_granted_,
        expected_is_camera_roll_access_status_granted_,
        expected_is_feature_setup_request_supported_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile test_profile_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<phonehub::FakeMultideviceFeatureAccessManager>
      fake_multidevice_feature_access_manager_;
  std::unique_ptr<eche_app::FakeAppsAccessManager> fake_apps_access_manager_;
  std::unique_ptr<phonehub::FakeCameraRollManager> fake_camera_roll_manager_;
  phonehub::FakeBrowserTabsModelProvider fake_browser_tabs_model_provider_;
  MockNewWindowDelegate new_window_delegate_;

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device_;
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map_;

  std::unique_ptr<TestMultideviceHandler> handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MultideviceHandlerTest, PageContentDataRequestedWithNullManagers) {
  SetUpHandlerWithEmptyManagers();

  base::Value::List args;
  args.Append("handlerFunctionName");
  test_web_ui()->HandleReceivedMessage("getPageContentData", args);
}

TEST_F(MultideviceHandlerTest, FeatureSetupConnectionFlow) {
  using Status = phonehub::FeatureSetupConnectionOperation::Status;

  CalAttempFeatureSetupConnection();
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnected);
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kCompletedSuccessfully);
  EXPECT_FALSE(IsFeatureSetupConnectionOperationInProgress());

  CalAttempFeatureSetupConnection();
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  CallCancelFeatureSetupConnection();
  EXPECT_FALSE(IsFeatureSetupConnectionOperationInProgress());

  CalAttempFeatureSetupConnection();
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kTimedOutConnecting);
  EXPECT_FALSE(IsFeatureSetupConnectionOperationInProgress());

  CalAttempFeatureSetupConnection();
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnected);
  EXPECT_TRUE(IsFeatureSetupConnectionOperationInProgress());

  SimulateFeatureSetupConnectionStatusChange(Status::kConnectionLost);
  EXPECT_FALSE(IsFeatureSetupConnectionOperationInProgress());
}

TEST_F(MultideviceHandlerTest, NotificationSetupFlow) {
  using Status = phonehub::NotificationAccessSetupOperation::Status;

  // Simulate success flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(
      Status::kSentMessageToPhoneAndWaitingForResponse);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kCompletedSuccessfully);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate cancel flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  CallCancelNotificationSetup();
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate failure via time-out flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kTimedOutConnecting);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate failure via connected then disconnected flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnectionDisconnected);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // If access has already been granted, a setup operation should not occur.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/true);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());
}

TEST_F(MultideviceHandlerTest, AppsSetupFlow) {
  InitWithFeatures(/* enabled_features */ {ash::features::kPhoneHub,
                                           ash::features::kEcheSWA},
                   /* disabled_features */ {});
  using Status = eche_app::AppsAccessSetupOperation::Status;

  // Simulate success flow.
  CallAttemptAppsSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(
      Status::kSentMessageToPhoneAndWaitingForResponse);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kCompletedSuccessfully);
  EXPECT_FALSE(IsAppsAccessSetupOperationInProgress());

  // Simulate cancel flow.
  CallAttemptAppsSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  CallCancelAppsSetup();
  EXPECT_FALSE(IsAppsAccessSetupOperationInProgress());

  // Simulate failure via time-out flow.
  CallAttemptAppsSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kTimedOutConnecting);
  EXPECT_FALSE(IsAppsAccessSetupOperationInProgress());

  // Simulate failure via connected then disconnected flow.
  CallAttemptAppsSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsAppsAccessSetupOperationInProgress());

  SimulateAppsOptInStatusChange(Status::kConnectionDisconnected);
  EXPECT_FALSE(IsAppsAccessSetupOperationInProgress());

  // If access has already been granted, a setup operation should not occur.
  CallAttemptAppsSetup(/*has_access_been_granted=*/true);
  EXPECT_FALSE(IsAppsAccessSetupOperationInProgress());
}

TEST_F(MultideviceHandlerTest, CameraRollSetupFlow) {
  using Status = phonehub::CombinedAccessSetupOperation::Status;
  fake_multidevice_feature_access_manager()
      ->SetFeatureSetupRequestSupportedInternal(true);

  // Simulate success flow.
  CallAttemptCameraRollSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(
      Status::kSentMessageToPhoneAndWaitingForResponse);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kCompletedSuccessfully);
  EXPECT_FALSE(IsCameraRollAccessSetupOperationInProgress());

  // Simulate cancel flow.
  CallAttemptCameraRollSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  CallCancelCameraRollSetup();
  EXPECT_FALSE(IsCameraRollAccessSetupOperationInProgress());

  // Simulate failure via time-out flow.
  CallAttemptCameraRollSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kTimedOutConnecting);
  EXPECT_FALSE(IsCameraRollAccessSetupOperationInProgress());

  // Simulate failure via connected then disconnected flow.
  CallAttemptCameraRollSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsCameraRollAccessSetupOperationInProgress());

  SimulateCameraRollOptInStatusChange(Status::kConnectionDisconnected);
  EXPECT_FALSE(IsCameraRollAccessSetupOperationInProgress());

  // If access has already been granted, a setup operation should not occur.
  CallAttemptCameraRollSetup(/*has_access_been_granted=*/true);
  EXPECT_FALSE(IsCameraRollAccessSetupOperationInProgress());
}

TEST_F(MultideviceHandlerTest, LogUmaMetricsForSetupFlow) {
  fake_multidevice_feature_access_manager()
      ->SetFeatureSetupRequestSupportedInternal(true);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDialogIntroActionHistogram, 0);

  base::Value::List set_up_screen_args;
  set_up_screen_args.Append(/*irrelivant_set_up_dialog=*/0);
  set_up_screen_args.Append(/*action_cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectTotalCount(kDialogIntroActionHistogram, 0);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*intro_screen_index=*/1);
  set_up_screen_args.Append(/*learn_more=*/2);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogIntroActionHistogram,
                                     /*learn_more=*/2, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*intro_screen_index=*/1);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogIntroActionHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*intro_screen_index=*/1);
  set_up_screen_args.Append(/*next=*/5);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogIntroActionHistogram,
                                     /*done=*/5, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*finish_on_phone_screen_index=*/2);
  set_up_screen_args.Append(/*learn_more=*/2);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogFinishOnPhoneActionHistogram,
                                     /*learn_more=*/2, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*finish_on_phone_screen_index=*/2);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogFinishOnPhoneActionHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connecting_screen_index=*/3);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogConnectingActionHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connected_screen_index=*/6);
  set_up_screen_args.Append(/*done=*/4);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogSetupFinishedActionHistogram,
                                     /*done=*/4, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connection_error_screen_index=*/4);
  set_up_screen_args.Append(/*try_again=*/5);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogConnectionErrorActionHistogram,
                                     /*try_again=*/5, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connection_error_screen_index=*/4);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogConnectionErrorActionHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connection_time_out_screen_index=*/5);
  set_up_screen_args.Append(/*try_again=*/5);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogConnectionTimeOutActionHistogram,
                                     /*try_again=*/5, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*connection_time_out_screen_index=*/5);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogConnectionTimeOutActionHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*set_a_pin_screen_index=*/7);
  set_up_screen_args.Append(/*cancel=*/3);
  test_web_ui()->HandleReceivedMessage("logPhoneHubPermissionSetUpScreenAction",
                                       set_up_screen_args);
  histogram_tester.ExpectBucketCount(kDialogSetAPinOrPasswordHistogram,
                                     /*cancel=*/3, 1);

  set_up_screen_args.clear();
  set_up_screen_args.Append(/*camera_roll_setup=*/3);
  test_web_ui()->HandleReceivedMessage(
      "logPhoneHubPermissionSetUpButtonClicked", set_up_screen_args);
  histogram_tester.ExpectBucketCount(kSetupButtonInSettingsClikedHistogram,
                                     /*camera_roll_setup=*/3, 1);
}

TEST_F(MultideviceHandlerTest, LogUmaMetricsForIntroScreenSetupMode) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDialogIntroScreenSetupModeHistogram, 0);

  base::Value::List set_up_mode_args;
  set_up_mode_args.Append(/*all_permissions=*/7);
  test_web_ui()->HandleReceivedMessage(
      "logPhoneHubPermissionOnboardingSetupMode", set_up_mode_args);
  histogram_tester.ExpectBucketCount(kDialogIntroScreenSetupModeHistogram,
                                     /*all_permissions=*/7, 1);
}

TEST_F(MultideviceHandlerTest, LogUmaMetricsForSetUpFinishedScreenSetupMode) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kDialogSetUpFinishedScreenSetupModeHistogram, 0);

  base::Value::List set_up_mode_args;
  set_up_mode_args.Append(/*notification_and_camera_roll=*/5);
  test_web_ui()->HandleReceivedMessage(
      "logPhoneHubPermissionOnboardingSetupResult", set_up_mode_args);
  histogram_tester.ExpectBucketCount(
      kDialogSetUpFinishedScreenSetupModeHistogram,
      /*notification_and_camera_roll=*/5, 1);
}

TEST_F(MultideviceHandlerTest, PageContentData) {
  InitWithFeatures(/* enabled_features */ {ash::features::kPhoneHub,
                                           ash::features::kEcheSWA},
                   /* disabled_features */ {});
  CallGetPageContentData();
  CallGetPageContentData();

  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      std::nullopt /* host_device */);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::
                               kHostSetLocallyButWaitingForBackendConfirmation,
                           test_device_);
  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified,
      test_device_);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::kHostVerified,
                           test_device_);

  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();
  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kEnabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);

  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);

  SimulateNearbyShareEnabledPrefChange(/*is_enabled=*/true,
                                       /*is_managed=*/false);
  SimulateNearbyShareEnabledPrefChange(/*is_enabled=*/true,
                                       /*is_managed=*/true);
  SimulateNearbyShareEnabledPrefChange(/*is_enabled=*/false,
                                       /*is_managed=*/false);
  SimulateNearbyShareEnabledPrefChange(/*is_enabled=*/false,
                                       /*is_managed=*/true);
  SimulateNearbyShareEnabledPrefChange(/*is_enabled=*/false,
                                       /*is_managed=*/true);
  SimulateAppsAccessStatusChanged(/*has_access_been_granted=*/true);
  SimulateCameraRollFilePermissionChanged(/*file_permission_granted=*/false);
  SimulateCameraRollFilePermissionChanged(/*file_permission_granted=*/true);
  SimulateCameraRollAccessstatusChanged(
      /*has_camera_roll_access_been_granted=*/true);
  SimulateCameraRollAccessstatusChanged(
      /*has_camera_roll_access_been_granted=*/false);
}

TEST_F(MultideviceHandlerTest, RetryPendingHostSetup) {
  CallRetryPendingHostSetup(true /* success */);
  CallRetryPendingHostSetup(false /* success */);
}

TEST_F(MultideviceHandlerTest, SetFeatureEnabledState) {
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      true /* enabled */, "authToken" /* auth_token */, true /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, "authToken" /* auth_token */, false /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, "authToken" /* auth_token */, true /* success */);
}

TEST_F(MultideviceHandlerTest, RemoveHostDevice) {
  CallRemoveHostDevice();
  CallRemoveHostDevice();
  CallRemoveHostDevice();
}

TEST_F(MultideviceHandlerTest, PageContentDataWhenEcheSWADisabled) {
  InitWithFeatures(
      /* enabled_features */ {ash::features::kPhoneHub},
      /* disabled_features */ {ash::features::kEcheSWA});

  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();

  feature_states_map[multidevice_setup::mojom::Feature::kEche] =
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy;
  SimulateFeatureStatesUpdate(feature_states_map);
}

TEST_F(MultideviceHandlerTest, PageContentDataWhenPhoneHubCameraRollDisabled) {
  InitWithFeatures(
      /* enabled_features */ {ash::features::kPhoneHub},
      /* disabled_features */ {ash::features::kPhoneHubCameraRoll});

  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();

  feature_states_map[multidevice_setup::mojom::Feature::kPhoneHubCameraRoll] =
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy;
  SimulateFeatureStatesUpdate(feature_states_map);
}

TEST_F(MultideviceHandlerTest, EnableScreenLockChanged) {
  InitWithFeatures(/* enabled_features */ {ash::features::kPhoneHub,
                                           ash::features::kEcheSWA},
                   {});
  SetUpHandlerWithEmptyManagers();

  SimulateEnableScreenLockChanged();
}

TEST_F(MultideviceHandlerTest, ScreenLockStatusChanged) {
  InitWithFeatures(/* enabled_features */ {ash::features::kPhoneHub,
                                           ash::features::kEcheSWA},
                   {});
  SetUpHandlerWithEmptyManagers();

  SimulateScreenLockStatusChanged();
}

TEST_F(MultideviceHandlerTest, ShowBrowserSyncSettings) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://settings/syncSetup/advanced"),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab));
  CallHandleShowBrowserSyncSettings();
}

}  // namespace ash::settings
