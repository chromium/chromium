// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include <memory>

#include "ash/components/phonehub/fake_camera_roll_manager.h"
#include "ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "ash/services/multidevice_setup/public/cpp/prefs.h"
#include "ash/webui/eche_app_ui/fake_apps_access_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/android_sms/fake_android_sms_app_manager.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace settings {

namespace {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
namespace multidevice_setup = ::ash::multidevice_setup;

using ::testing::Optional;

class TestMultideviceHandler : public MultideviceHandler {
 public:
  TestMultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::MultideviceFeatureAccessManager*
          multidevice_feature_access_manager,
      multidevice_setup::AndroidSmsPairingStateTracker*
          android_sms_pairing_state_tracker,
      android_sms::AndroidSmsAppManager* android_sms_app_manager,
      ash::eche_app::AppsAccessManager* apps_access_manager,
      ash::phonehub::CameraRollManager* camera_roll_manager)
      : MultideviceHandler(prefs,
                           multidevice_setup_client,
                           multidevice_feature_access_manager,
                           android_sms_pairing_state_tracker,
                           android_sms_app_manager,
                           apps_access_manager,
                           camera_roll_manager) {}
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
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kInstantTethering,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kMessages,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kSmartLock,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHub,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHubNotifications,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kWifiSync,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kEche,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost}};
}

void VerifyPageContentDict(
    const base::Value* value,
    multidevice_setup::mojom::HostStatus expected_host_status,
    const absl::optional<multidevice::RemoteDeviceRef>& expected_host_device,
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map,
    bool expected_is_nearby_share_disallowed_by_policy_,
    bool expected_is_phone_hub_apps_access_granted_,
    bool expected_is_camera_roll_file_permission_granted_,
    bool expected_is_camera_roll_access_status_granted_) {
  const base::DictionaryValue* page_content_dict;
  EXPECT_TRUE(value->GetAsDictionary(&page_content_dict));

  absl::optional<int> mode = page_content_dict->FindIntKey("mode");
  ASSERT_TRUE(mode);
  EXPECT_EQ(static_cast<int>(expected_host_status), *mode);

  absl::optional<int> better_together_state =
      page_content_dict->FindIntKey("betterTogetherState");
  ASSERT_TRUE(better_together_state);
  auto it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite);
  EXPECT_EQ(static_cast<int>(it->second), *better_together_state);

  absl::optional<int> instant_tethering_state =
      page_content_dict->FindIntKey("instantTetheringState");
  ASSERT_TRUE(instant_tethering_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kInstantTethering);
  EXPECT_EQ(static_cast<int>(it->second), *instant_tethering_state);

  absl::optional<int> messages_state =
      page_content_dict->FindIntKey("messagesState");
  ASSERT_TRUE(messages_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kMessages);
  EXPECT_EQ(static_cast<int>(it->second), *messages_state);

  absl::optional<int> smart_lock_state =
      page_content_dict->FindIntKey("smartLockState");
  ASSERT_TRUE(smart_lock_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kSmartLock);
  EXPECT_EQ(static_cast<int>(it->second), *smart_lock_state);

  absl::optional<int> phone_hub_state =
      page_content_dict->FindIntKey("phoneHubState");
  ASSERT_TRUE(phone_hub_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kPhoneHub);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_state);

  absl::optional<int> phone_hub_notifications_state =
      page_content_dict->FindIntKey("phoneHubNotificationsState");
  ASSERT_TRUE(phone_hub_notifications_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubNotifications);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_notifications_state);

  absl::optional<int> phone_hub_camera_roll_state =
      page_content_dict->FindIntKey("phoneHubCameraRollState");
  ASSERT_TRUE(phone_hub_camera_roll_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_camera_roll_state);

  absl::optional<int> phone_hub_task_continuation_state =
      page_content_dict->FindIntKey("phoneHubTaskContinuationState");
  ASSERT_TRUE(phone_hub_task_continuation_state);
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_task_continuation_state);

  absl::optional<int> phone_hub_apps_state =
      page_content_dict->FindIntKey("phoneHubAppsState");
  ASSERT_TRUE(phone_hub_apps_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kEche);
  EXPECT_EQ(static_cast<int>(it->second), *phone_hub_apps_state);

  absl::optional<int> wifi_sync_state =
      page_content_dict->FindIntKey("wifiSyncState");
  ASSERT_TRUE(wifi_sync_state);
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kWifiSync);
  EXPECT_EQ(static_cast<int>(it->second), *wifi_sync_state);

  const std::string* host_device_name =
      page_content_dict->FindStringKey("hostDeviceName");
  if (expected_host_device) {
    ASSERT_TRUE(host_device_name);
    EXPECT_EQ(expected_host_device->name(), *host_device_name);
  } else {
    EXPECT_FALSE(host_device_name);
  }

  EXPECT_THAT(page_content_dict->FindBoolKey("isNearbyShareDisallowedByPolicy"),
              Optional(expected_is_nearby_share_disallowed_by_policy_));

  EXPECT_THAT(page_content_dict->FindBoolKey("isPhoneHubAppsAccessGranted"),
              Optional(expected_is_phone_hub_apps_access_granted_));

  EXPECT_THAT(
      page_content_dict->FindBoolKey("isCameraRollFilePermissionGranted"),
      Optional(expected_is_camera_roll_file_permission_granted_));

  EXPECT_THAT(
      page_content_dict->FindBoolKey("isPhoneHubPermissionsDialogSupported"),
      Optional(true));

  EXPECT_THAT(page_content_dict->FindIntKey("cameraRollAccessStatus"),
              Optional(expected_is_camera_roll_access_status_granted_ ? 2 : 1));
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
    fake_android_sms_pairing_state_tracker_ = std::make_unique<
        multidevice_setup::FakeAndroidSmsPairingStateTracker>();
    fake_android_sms_app_manager_ =
        std::make_unique<android_sms::FakeAndroidSmsAppManager>();
    fake_apps_access_manager_ =
        std::make_unique<ash::eche_app::FakeAppsAccessManager>(
            phonehub::MultideviceFeatureAccessManager::AccessStatus::
                kAvailableButNotGranted);
    fake_camera_roll_manager_ =
        std::make_unique<ash::phonehub::FakeCameraRollManager>();

    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterNearbySharingPrefs(prefs_->registry());
    prefs_->SetBoolean(::prefs::kNearbySharingEnabledPrefName, true);
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(true);

    prefs_->registry()->RegisterBooleanPref(
        multidevice_setup::kInstantTetheringAllowedPrefName,
        /*default_value=*/true);

    handler_ = std::make_unique<TestMultideviceHandler>(
        prefs_.get(), fake_multidevice_setup_client_.get(),
        fake_multidevice_feature_access_manager_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        fake_android_sms_app_manager_.get(), fake_apps_access_manager_.get(),
        fake_camera_roll_manager_.get());

    test_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&test_profile_));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(test_web_contents_.get());
    handler_->set_web_ui(test_web_ui_.get());

    handler_->RegisterMessages();
    handler_->AllowJavascript();

    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPhoneHub, chromeos::features::kEcheSWA,
         chromeos::features::kEchePhoneHubPermissionsOnboarding},
        {});
  }

  void CallGetPageContentData() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::Value args(base::Value::Type::LIST);
    args.Append("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getPageContentData",
                                         &base::Value::AsListValue(args));

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
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("removeHostDevice", &empty_args);
    EXPECT_EQ(num_remote_host_device_calls_before_call + 1u,
              fake_multidevice_setup_client()->num_remove_host_device_called());
  }

  void CallGetAndroidSmsInfo(bool expected_enabled, const GURL& expected_url) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.Append("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getAndroidSmsInfo",
                                         &base::Value::AsListValue(args));

    ASSERT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    ASSERT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(
        ContentSettingsPattern::FromURLNoWildcard(expected_url).ToString(),
        call_data.arg3()->FindKey("origin")->GetString());
    EXPECT_EQ(expected_enabled,
              call_data.arg3()->FindKey("enabled")->GetBool());
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
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("attemptNotificationSetup",
                                         &empty_args);
  }

  void CallCancelNotificationSetup() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("cancelNotificationSetup",
                                         &empty_args);
  }

  void CallAttemptAppsSetup(bool has_access_been_granted) {
    fake_apps_access_manager()->SetAccessStatusInternal(
        has_access_been_granted ? phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAccessGranted
                                : phonehub::MultideviceFeatureAccessManager::
                                      AccessStatus::kAvailableButNotGranted);
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("attemptAppsSetup", &empty_args);
  }

  void CallCancelAppsSetup() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("cancelAppsSetup", &empty_args);
  }

  void SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus host_status,
      const absl::optional<multidevice::RemoteDeviceRef>& host_device) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        std::make_pair(host_status, host_device));
    EXPECT_EQ(call_data_count_before_call + 2u,
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
    EXPECT_EQ(call_data_count_before_call + 2u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulatePairingStateUpdate(bool is_android_sms_pairing_complete) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_android_sms_pairing_state_tracker_->SetPairingComplete(
        is_android_sms_pairing_complete);
    EXPECT_EQ(call_data_count_before_call + 2u,
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
      if (did_managed_change)
        ++expected_call_count;
    } else {
      prefs_->RemoveManagedPref(::prefs::kNearbySharingEnabledPrefName);
      EXPECT_FALSE(
          prefs_->IsManagedPreference(::prefs::kNearbySharingEnabledPrefName));
      if (did_managed_change)
        ++expected_call_count;

      prefs_->SetBoolean(::prefs::kNearbySharingEnabledPrefName, is_enabled);
      if (did_enabled_change)
        ++expected_call_count;
    }
    EXPECT_EQ(is_enabled,
              prefs_->GetBoolean(::prefs::kNearbySharingEnabledPrefName));

    EXPECT_EQ(expected_call_count, test_web_ui()->call_data().size());

    if (expected_call_count == call_data_count_before_call)
      return;

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
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("retryPendingHostSetup", &empty_args);
    fake_multidevice_setup_client()->InvokePendingRetrySetHostNowCallback(
        success);
  }

  void CallSetUpAndroidSms() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setUpAndroidSms", &empty_args);
  }

  void CallSetFeatureEnabledState(multidevice_setup::mojom::Feature feature,
                                  bool enabled,
                                  const absl::optional<std::string>& auth_token,
                                  bool success) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::Value args(base::Value::Type::LIST);
    args.Append("handlerFunctionName");
    args.Append(static_cast<int>(feature));
    args.Append(enabled);
    if (auth_token)
      args.Append(*auth_token);

    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setFeatureEnabledState",
                                         &base::Value::AsListValue(args));
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

  android_sms::FakeAndroidSmsAppManager* fake_android_sms_app_manager() {
    return fake_android_sms_app_manager_.get();
  }

  phonehub::FakeMultideviceFeatureAccessManager*
  fake_multidevice_feature_access_manager() {
    return fake_multidevice_feature_access_manager_.get();
  }

  ash::eche_app::FakeAppsAccessManager* fake_apps_access_manager() {
    return fake_apps_access_manager_.get();
  }

  ash::phonehub::FakeCameraRollManager* fake_camera_roll_manager() {
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
    if (completed_successfully)
      call_data_count_before_call++;

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
        ->IsSetupOperationInProgress();
  }

  void SimulateAppsOptInStatusChange(
      ash::eche_app::AppsAccessSetupOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_apps_access_manager()->SetAppsSetupOperationStatus(status);

    bool completed_successfully =
        status ==
        ash::eche_app::AppsAccessSetupOperation::Status::kCompletedSuccessfully;
    if (completed_successfully)
      call_data_count_before_call++;

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

  const multidevice::RemoteDeviceRef test_device_;

  bool expected_is_nearby_share_disallowed_by_policy_ = false;
  bool expected_is_phone_hub_apps_access_granted_ = false;
  bool expected_is_camera_roll_file_permission_granted_ = true;
  bool expected_is_camera_roll_access_status_granted_ = false;

 private:
  void VerifyPageContent(const base::Value* value) {
    VerifyPageContentDict(
        value, fake_multidevice_setup_client_->GetHostStatus().first,
        fake_multidevice_setup_client_->GetHostStatus().second,
        fake_multidevice_setup_client_->GetFeatureStates(),
        expected_is_nearby_share_disallowed_by_policy_,
        expected_is_phone_hub_apps_access_granted_,
        expected_is_camera_roll_file_permission_granted_,
        expected_is_camera_roll_access_status_granted_);
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
  std::unique_ptr<multidevice_setup::FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;
  std::unique_ptr<ash::eche_app::FakeAppsAccessManager>
      fake_apps_access_manager_;
  std::unique_ptr<ash::phonehub::FakeCameraRollManager>
      fake_camera_roll_manager_;

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device_;
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map_;
  std::unique_ptr<android_sms::FakeAndroidSmsAppManager>
      fake_android_sms_app_manager_;

  std::unique_ptr<TestMultideviceHandler> handler_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

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
  using Status = ash::eche_app::AppsAccessSetupOperation::Status;

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

TEST_F(MultideviceHandlerTest, PageContentData) {
  CallGetPageContentData();
  CallGetPageContentData();

  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      absl::nullopt /* host_device */);
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

  SimulatePairingStateUpdate(/*is_android_sms_pairing_complete=*/true);

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

TEST_F(MultideviceHandlerTest, SetUpAndroidSms) {
  EXPECT_FALSE(fake_android_sms_app_manager()->has_installed_app());
  EXPECT_FALSE(fake_android_sms_app_manager()->has_launched_app());
  CallSetUpAndroidSms();
  EXPECT_TRUE(fake_android_sms_app_manager()->has_installed_app());
  EXPECT_TRUE(fake_android_sms_app_manager()->has_launched_app());
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

TEST_F(MultideviceHandlerTest, GetAndroidSmsInfo) {
  // Check that getAndroidSmsInfo returns correct value.
  CallGetAndroidSmsInfo(false /* expected_enabled */,
                        android_sms::GetAndroidMessagesURL(
                            true /* use_install_url */) /* expected_url */);

  // Change messages feature state and assert that the change
  // callback is fired.
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();
  feature_states_map[multidevice_setup::mojom::Feature::kMessages] =
      multidevice_setup::mojom::FeatureState::kEnabledByUser;

  size_t call_data_count_before_call = test_web_ui()->call_data().size();
  SimulateFeatureStatesUpdate(feature_states_map);
  const content::TestWebUI::CallData& call_data_1 =
      CallDataAtIndex(call_data_count_before_call + 1);
  EXPECT_EQ("cr.webUIListenerCallback", call_data_1.function_name());
  EXPECT_EQ("settings.onAndroidSmsInfoChange", call_data_1.arg1()->GetString());

  // Check that getAndroidSmsInfo returns update value.
  CallGetAndroidSmsInfo(true /* enabled */, android_sms::GetAndroidMessagesURL(
                                                true) /* expected_url */);

  // Now, update the installed URL. This should have resulted in another call.
  fake_android_sms_app_manager()->SetInstalledAppUrl(
      android_sms::GetAndroidMessagesURL(true /* use_install_url */,
                                         android_sms::PwaDomain::kStaging));
  const content::TestWebUI::CallData& call_data_2 =
      CallDataAtIndex(call_data_count_before_call + 4);
  EXPECT_EQ("cr.webUIListenerCallback", call_data_2.function_name());
  EXPECT_EQ("settings.onAndroidSmsInfoChange", call_data_2.arg1()->GetString());
  CallGetAndroidSmsInfo(
      true /* enabled */,
      android_sms::GetAndroidMessagesURL(
          true /* use_install_url */,
          android_sms::PwaDomain::kStaging) /* expected_url */);
}

}  // namespace settings

}  // namespace chromeos
