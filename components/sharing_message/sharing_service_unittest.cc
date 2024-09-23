// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_device_source.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "components/sharing_message/sharing_device_registration_result.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/vapid_key_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const char kDeviceName[] = "other_name";
const char kAuthorizedEntity[] = "authorized_entity";
constexpr base::TimeDelta kTimeout = base::Seconds(15);

SharingTargetDeviceInfo CreateFakeSharingTargetDeviceInfo(
    const std::string& guid,
    const std::string& client_name) {
  return SharingTargetDeviceInfo(guid, client_name,
                                 SharingDevicePlatform::kUnknown,
                                 /*pulse_interval=*/base::TimeDelta(),
                                 syncer::DeviceInfo::FormFactor::kUnknown,
                                 /*last_updated_timestamp=*/base::Time());
}

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}

  MockInstanceIDDriver(const MockInstanceIDDriver&) = delete;
  MockInstanceIDDriver& operator=(const MockInstanceIDDriver&) = delete;

  ~MockInstanceIDDriver() override = default;
};

class MockSharingHandlerRegistry : public SharingHandlerRegistry {
 public:
  MockSharingHandlerRegistry() = default;
  ~MockSharingHandlerRegistry() override = default;

  MOCK_METHOD1(GetSharingHandler,
               SharingMessageHandler*(
                   components_sharing_message::SharingMessage::PayloadCase
                       payload_case));
  MOCK_METHOD2(RegisterSharingHandler,
               void(std::unique_ptr<SharingMessageHandler> handler,
                    components_sharing_message::SharingMessage::PayloadCase
                        payload_case));
  MOCK_METHOD1(UnregisterSharingHandler,
               void(components_sharing_message::SharingMessage::PayloadCase
                        payload_case));
};

class MockSharingFCMHandler : public SharingFCMHandler {
  using SharingMessage = components_sharing_message::SharingMessage;

 public:
  MockSharingFCMHandler()
      : SharingFCMHandler(/*gcm_driver=*/nullptr,
                          /*sharing_fcm_sender=*/nullptr,
                          /*sync_preference=*/nullptr,
                          /*handler_registry=*/nullptr) {}
  ~MockSharingFCMHandler() override = default;

  MOCK_METHOD0(StartListening, void());
  MOCK_METHOD0(StopListening, void());
};

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration(
      PrefService* pref_service,
      SharingSyncPreference* prefs,
      VapidKeyManager* vapid_key_manager,
      instance_id::InstanceIDDriver* instance_id_driver,
      syncer::SyncService* sync_service)
      : vapid_key_manager_(vapid_key_manager) {}
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    registration_attempts_++;
    // Simulate SharingDeviceRegistration calling GetOrCreateKey.
    vapid_key_manager_->GetOrCreateKey();
    std::move(callback).Run(result_);
  }

  void UnregisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    unregistration_attempts_++;
    std::move(callback).Run(result_);
  }

  bool IsClickToCallSupported() const override { return false; }

  bool IsSharedClipboardSupported() const override { return false; }

  bool IsSmsFetcherSupported() const override { return false; }

  bool IsRemoteCopySupported() const override { return false; }

  bool IsOptimizationGuidePushNotificationSupported() const override {
    return false;
  }

  void SetEnabledFeaturesForTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features)
      override {}

  void SetResult(SharingDeviceRegistrationResult result) { result_ = result; }

  int registration_attempts() { return registration_attempts_; }
  int unregistration_attempts() { return unregistration_attempts_; }

 private:
  raw_ptr<VapidKeyManager> vapid_key_manager_;
  SharingDeviceRegistrationResult result_ =
      SharingDeviceRegistrationResult::kSuccess;
  int registration_attempts_ = 0;
  int unregistration_attempts_ = 0;
};

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ =
        new SharingSyncPreference(&prefs_, &fake_device_info_sync_service);
    vapid_key_manager_ = new VapidKeyManager(sync_prefs_, &test_sync_service_);
    sharing_device_registration_ = new FakeSharingDeviceRegistration(
        /* pref_service= */ nullptr, sync_prefs_, vapid_key_manager_,
        &mock_instance_id_driver_, &test_sync_service_);
    handler_registry_ = new testing::NiceMock<MockSharingHandlerRegistry>();
    fcm_handler_ = new testing::NiceMock<MockSharingFCMHandler>();
    device_source_ = new testing::NiceMock<MockSharingDeviceSource>();
    sharing_message_sender_ = new testing::NiceMock<MockSharingMessageSender>();
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());

    ON_CALL(*device_source_, IsReady()).WillByDefault(testing::Return(true));
  }

  ~SharingServiceTest() override {
    // Make sure we're creating a SharingService so it can take ownership of the
    // local objects.
    GetSharingService();
  }

  void OnMessageSent(
      SharingSendMessageResult result,
      std::unique_ptr<components_sharing_message::ResponseMessage> response) {
    send_message_result_ = std::make_optional(result);
    send_message_response_ = std::move(response);
  }

  const std::optional<SharingSendMessageResult>& send_message_result() {
    return send_message_result_;
  }

  const components_sharing_message::ResponseMessage* send_message_response() {
    return send_message_response_.get();
  }

  void OnDeviceCandidatesInitialized() {
    device_candidates_initialized_ = true;
  }

 protected:
  // Lazily initialized so we can test the constructor.
  SharingService* GetSharingService() {
    if (!sharing_service_) {
      sharing_service_ = std::make_unique<SharingService>(
          base::WrapUnique(sync_prefs_.get()),
          base::WrapUnique(vapid_key_manager_.get()),
          base::WrapUnique(sharing_device_registration_.get()),
          base::WrapUnique(sharing_message_sender_.get()),
          base::WrapUnique(device_source_.get()),
          base::WrapUnique(handler_registry_.get()),
          base::WrapUnique(fcm_handler_.get()), &test_sync_service_,
          &favicon_service_, &send_tab_to_self_model_,
          base::SingleThreadTaskRunner::GetCurrentDefault());
    }
    task_environment_.RunUntilIdle();
    return sharing_service_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_features_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service;
  syncer::TestSyncService test_sync_service_;
  testing::NiceMock<favicon::MockFaviconService> favicon_service_;
  send_tab_to_self::TestSendTabToSelfModel send_tab_to_self_model_;
  sync_preferences::TestingPrefServiceSyncable prefs_;

 private:
  // `sharing_service_` must outlive the raw_ptrs below.
  std::unique_ptr<SharingService> sharing_service_;

 protected:
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  raw_ptr<testing::NiceMock<MockSharingHandlerRegistry>> handler_registry_;
  raw_ptr<testing::NiceMock<MockSharingFCMHandler>> fcm_handler_;
  raw_ptr<testing::NiceMock<MockSharingDeviceSource>> device_source_;

  raw_ptr<SharingSyncPreference> sync_prefs_;
  raw_ptr<VapidKeyManager> vapid_key_manager_;
  raw_ptr<FakeSharingDeviceRegistration> sharing_device_registration_;
  raw_ptr<testing::NiceMock<MockSharingMessageSender>> sharing_message_sender_;
  bool device_candidates_initialized_ = false;

 private:
  std::optional<SharingSendMessageResult> send_message_result_;
  std::unique_ptr<components_sharing_message::ResponseMessage>
      send_message_response_;
};

bool ProtoEquals(const google::protobuf::MessageLite& expected,
                 const google::protobuf::MessageLite& actual) {
  std::string expected_serialized, actual_serialized;
  expected.SerializeToString(&expected_serialized);
  actual.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
}  // namespace

TEST_F(SharingServiceTest, GetDeviceCandidates_Empty) {
  EXPECT_CALL(*device_source_, GetDeviceCandidates(::testing::_))
      .WillOnce(
          [](sync_pb::SharingSpecificFields::EnabledFeatures required_feature)
              -> std::vector<SharingTargetDeviceInfo> { return {}; });

  std::vector<SharingTargetDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Tracked) {
  EXPECT_CALL(*device_source_, GetDeviceCandidates(::testing::_))
      .WillOnce(
          [](sync_pb::SharingSpecificFields::EnabledFeatures required_feature) {
            std::vector<SharingTargetDeviceInfo> device_candidates;
            device_candidates.push_back(CreateFakeSharingTargetDeviceInfo(
                base::Uuid::GenerateRandomV4().AsLowercaseString(),
                kDeviceName));
            return device_candidates;
          });

  std::vector<SharingTargetDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);

  ASSERT_EQ(1u, candidates.size());
}

TEST_F(SharingServiceTest, SendMessageToDeviceSuccess) {
  SharingTargetDeviceInfo device_info = CreateFakeSharingTargetDeviceInfo(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), kDeviceName);

  components_sharing_message::ResponseMessage expected_response_message;

  auto run_callback = [&](const SharingTargetDeviceInfo& device_info,
                          base::TimeDelta response_timeout,
                          components_sharing_message::SharingMessage message,
                          SharingMessageSender::DelegateType delegate_type,
                          SharingMessageSender::ResponseCallback callback) {
    std::unique_ptr<components_sharing_message::ResponseMessage>
        response_message =
            std::make_unique<components_sharing_message::ResponseMessage>();
    response_message->CopyFrom(expected_response_message);
    std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                            std::move(response_message));
    return base::DoNothing();
  };

  ON_CALL(*sharing_message_sender_,
          SendMessageToDevice(testing::_, testing::_, testing::_, testing::_,
                              testing::_))
      .WillByDefault(testing::Invoke(run_callback));

  GetSharingService()->SendMessageToDevice(
      device_info, kTimeout, components_sharing_message::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, send_message_result());
  ASSERT_TRUE(send_message_response());
  EXPECT_TRUE(ProtoEquals(expected_response_message, *send_message_response()));
}

TEST_F(SharingServiceTest, SendTabEntryAddedLocally) {
  scoped_features_.InitAndEnableFeature(
      send_tab_to_self::kSendTabToSelfIOSPushNotifications);

  const std::string title = "title";
  const std::string device_name = "device name";
  const std::string host = "www.example.com";
  const std::string destination_url = "https://www.example.com/";
  const std::string icon_url = "https://www.example.com/favicon.ico";
  const std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  EXPECT_CALL(*device_source_, GetDeviceByGuid(guid))
      .Times(2)
      .WillRepeatedly([](const std::string& guid)
                          -> std::optional<SharingTargetDeviceInfo> {
        return SharingTargetDeviceInfo(guid, kDeviceName,
                                       SharingDevicePlatform::kIOS,
                                       /*pulse_interval=*/base::TimeDelta(),
                                       syncer::DeviceInfo::FormFactor::kUnknown,
                                       /*last_updated_timestamp=*/base::Time());
      });

  ON_CALL(favicon_service_, GetLargestRawFaviconForPageURL)
      .WillByDefault([icon_url](auto, auto, auto,
                                favicon_base::FaviconRawBitmapCallback callback,
                                auto) {
        favicon_base::FaviconRawBitmapResult result;
        result.icon_url = GURL(icon_url);
        std::move(callback).Run(result);
        base::CancelableTaskTracker::TaskId kTaskId = 1;
        return kTaskId;
      });

  // Create the expected proto.
  sync_pb::UnencryptedSharingMessage message;
  sync_pb::SendTabToSelfPush* push_notification_entry =
      message.mutable_send_tab_message();
  push_notification_entry->set_title(l10n_util::GetStringFUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_TITLE_USER_GIVEN_DEVICE_NAME,
      base::UTF8ToUTF16(device_name)));
  push_notification_entry->set_text(l10n_util::GetStringFUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_BODY, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(host)));
  push_notification_entry->set_destination_url(destination_url);
  push_notification_entry->set_placeholder_title(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_TITLE));
  push_notification_entry->set_placeholder_body(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_BODY));
  push_notification_entry->set_entry_unique_guid(guid);
  auto* icon = push_notification_entry->add_icon();
  icon->set_url(icon_url);

  EXPECT_CALL(*sharing_message_sender_,
              SendUnencryptedMessageToDevice(testing::_,
                                             base::test::EqualsProto(message),
                                             testing::_, testing::_));

  send_tab_to_self::SendTabToSelfEntry entry =
      send_tab_to_self::SendTabToSelfEntry(guid, GURL(destination_url), title,
                                           base::Time(), device_name, guid);
  GetSharingService()->EntryAddedLocally(&entry);
}

TEST_F(SharingServiceTest, SendTabEntryAddedLocally_FeatureDisabled) {
  scoped_features_.InitAndDisableFeature(
      send_tab_to_self::kSendTabToSelfIOSPushNotifications);

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  EXPECT_CALL(*device_source_, GetDeviceByGuid(guid)).Times(0);

  ON_CALL(favicon_service_, GetLargestRawFaviconForPageURL)
      .WillByDefault([](auto, auto, auto,
                        favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        base::CancelableTaskTracker::TaskId kTaskId = 1;
        return kTaskId;
      });

  EXPECT_CALL(*sharing_message_sender_, SendUnencryptedMessageToDevice)
      .Times(0);

  send_tab_to_self::SendTabToSelfEntry entry =
      send_tab_to_self::SendTabToSelfEntry(
          "guid", GURL("https://www.example.com"), "title", base::Time(),
          "device name", guid);
  GetSharingService()->EntryAddedLocally(&entry);
}

TEST_F(SharingServiceTest, SendTabEntryAddedLocally_NonIOSDevice) {
  scoped_features_.InitAndEnableFeature(
      send_tab_to_self::kSendTabToSelfIOSPushNotifications);

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  EXPECT_CALL(*device_source_, GetDeviceByGuid(guid))
      .WillOnce([](const std::string& guid)
                    -> std::optional<SharingTargetDeviceInfo> {
        return SharingTargetDeviceInfo(guid, kDeviceName,
                                       SharingDevicePlatform::kAndroid,
                                       /*pulse_interval=*/base::TimeDelta(),
                                       syncer::DeviceInfo::FormFactor::kUnknown,
                                       /*last_updated_timestamp=*/base::Time());
      });

  EXPECT_CALL(*sharing_message_sender_, SendUnencryptedMessageToDevice)
      .Times(0);

  send_tab_to_self::SendTabToSelfEntry entry =
      send_tab_to_self::SendTabToSelfEntry(
          "guid", GURL("https://www.example.com"), "title", base::Time(),
          "device name", guid);
  GetSharingService()->EntryAddedLocally(&entry);
}

TEST_F(SharingServiceTest, DeviceRegistration) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // As device is already registered, won't attempt registration anymore.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  auto vapid_key = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(vapid_key);
  std::vector<uint8_t> vapid_key_info;
  ASSERT_TRUE(vapid_key->ExportPrivateKey(&vapid_key_info));

  // Registration will be attempeted as VAPID key has changed.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  sync_prefs_->SetVapidKey(vapid_key_info);
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationPreferenceNotAvailable) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // As sync preferences is not available, registration shouldn't start.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(0, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransportMode) {
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransientError) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Retry will be scheduled on transient error received.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kFcmTransientError);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::REGISTERING,
            GetSharingService()->GetStateForTesting());

  // Retry should be scheduled by now. Next retry after 5 minutes will be
  // successful.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  task_environment_.FastForwardBy(
      base::Milliseconds(kRetryBackoffPolicy.initial_delay_ms));
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceUnregistrationSyncDisabled) {
  test_sync_service_.SetSignedOut();

  // Create new SharingService instance with sync disabled at constructor.
  GetSharingService();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegisterAndUnregister) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  // Create new SharingService instance with feature enabled at constructor.
  GetSharingService();
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Change sync to configuring, which will be ignored.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable sync and un-registration should happen.
  test_sync_service_.SetSignedOut();
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Should be able to register once again when sync is back on.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable syncing of preference and un-registration should happen.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(2, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, StartListeningToFCMAtConstructor) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  // Create new SharingService instance with FCM already registered at
  // constructor.
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  GetSharingService();
}

TEST_F(SharingServiceTest, GetDeviceByGuid) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  EXPECT_CALL(*device_source_, GetDeviceByGuid(guid))
      .WillOnce([](const std::string& guid)
                    -> std::optional<SharingTargetDeviceInfo> {
        return CreateFakeSharingTargetDeviceInfo(guid, "Dell Computer sno one");
      });

  std::optional<SharingTargetDeviceInfo> device_info =
      GetSharingService()->GetDeviceByGuid(guid);
  ASSERT_TRUE(device_info.has_value());
  EXPECT_EQ("Dell Computer sno one", device_info->client_name());
}

TEST_F(SharingServiceTest, AddSharingHandler) {
  EXPECT_CALL(*handler_registry_,
              RegisterSharingHandler(testing::_, testing::_))
      .Times(1);
  GetSharingService()->RegisterSharingHandler(
      nullptr,
      components_sharing_message::SharingMessage::kSharedClipboardMessage);
}

TEST_F(SharingServiceTest, RemoveSharingHandler) {
  EXPECT_CALL(*handler_registry_, UnregisterSharingHandler(testing::_))
      .Times(1);
  GetSharingService()->UnregisterSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage);
}
