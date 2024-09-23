// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_sync_bridge.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

namespace syncer {
namespace {

using sync_pb::DataTypeState;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::Matcher;
using testing::NiceMock;
using testing::Not;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

using DeviceCountMap = std::map<DeviceInfo::FormFactor, int>;
using DeviceInfoList = std::vector<std::unique_ptr<DeviceInfo>>;
using StorageKeyList = DataTypeSyncBridge::StorageKeyList;
using RecordList = DataTypeStore::RecordList;
using StartCallback = DataTypeControllerDelegate::StartCallback;
using WriteBatch = DataTypeStore::WriteBatch;

const int kLocalSuffix = 0;

const sync_pb::SyncEnums_DeviceType kLocalDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
const DeviceInfo::OsType kLocalDeviceOS = DeviceInfo::OsType::kLinux;
const DeviceInfo::FormFactor kLocalDeviceFormFactor =
    DeviceInfo::FormFactor::kDesktop;

MATCHER_P(HasDeviceInfo, expected, "") {
  return arg.device_info().SerializeAsString() == expected.SerializeAsString();
}

MATCHER_P(EqualsProto, expected, "") {
  return arg.SerializeAsString() == expected.SerializeAsString();
}

MATCHER_P(ModelEqualsSpecifics, expected_specifics, "") {
  if (expected_specifics.has_sharing_fields() !=
      arg.sharing_info().has_value()) {
    return false;
  }

  if (expected_specifics.has_sharing_fields()) {
    auto& expected_fields = expected_specifics.sharing_fields();
    auto& arg_info = *arg.sharing_info();
    if (expected_fields.vapid_fcm_token() !=
            arg_info.vapid_target_info.fcm_token ||
        expected_fields.vapid_p256dh() != arg_info.vapid_target_info.p256dh ||
        expected_fields.vapid_auth_secret() !=
            arg_info.vapid_target_info.auth_secret ||
        expected_fields.sender_id_fcm_token_v2() !=
            arg_info.sender_id_target_info.fcm_token ||
        expected_fields.sender_id_p256dh_v2() !=
            arg_info.sender_id_target_info.p256dh ||
        expected_fields.sender_id_auth_secret_v2() !=
            arg_info.sender_id_target_info.auth_secret ||
        expected_fields.chime_representative_target_id() !=
            arg_info.chime_representative_target_id ||
        static_cast<size_t>(expected_fields.enabled_features_size()) !=
            arg_info.enabled_features.size()) {
      return false;
    }

    for (int i = 0; i < expected_fields.enabled_features_size(); ++i) {
      if (!arg_info.enabled_features.count(
              expected_fields.enabled_features(i))) {
        return false;
      }
    }
  }

  DataTypeSet expected_data_types = GetDataTypeSetFromSpecificsFieldNumberList(
      expected_specifics.invalidation_fields().interested_data_type_ids());
  if (expected_data_types != arg.interested_data_types()) {
    return false;
  }

  // Note that we ignore the device name here to avoid having to inject the
  // local device's.
  return expected_specifics.cache_guid() == arg.guid() &&
         expected_specifics.device_type() == arg.device_type() &&
         expected_specifics.sync_user_agent() == arg.sync_user_agent() &&
         expected_specifics.chrome_version() == arg.chrome_version() &&
         expected_specifics.signin_scoped_device_id() ==
             arg.signin_scoped_device_id() &&
         expected_specifics.model() == arg.model_name() &&
         expected_specifics.manufacturer() == arg.manufacturer_name() &&
         expected_specifics.feature_fields()
                 .send_tab_to_self_receiving_enabled() ==
             arg.send_tab_to_self_receiving_enabled() &&
         expected_specifics.feature_fields()
                 .send_tab_to_self_receiving_type() ==
             arg.send_tab_to_self_receiving_type() &&
         expected_specifics.invalidation_fields().instance_id_token() ==
             arg.fcm_registration_token();
}

Matcher<std::unique_ptr<EntityData>> HasSpecifics(
    const Matcher<sync_pb::EntitySpecifics>& m) {
  return testing::Pointee(testing::Field(&EntityData::specifics, m));
}

MATCHER_P(HasCacheGuid, cache_guid, "") {
  return arg.guid() == cache_guid;
}

MATCHER(HasLastUpdatedAboutNow, "") {
  const sync_pb::DeviceInfoSpecifics& specifics = arg.device_info();
  const base::Time now = base::Time::Now();
  const base::TimeDelta tolerance = base::Minutes(1);
  const base::Time actual_last_updated =
      ProtoTimeToTime(specifics.last_updated_timestamp());

  if (actual_last_updated < now - tolerance) {
    *result_listener << "which is too far in the past";
    return false;
  }
  if (actual_last_updated > now + tolerance) {
    *result_listener << "which is too far in the future";
    return false;
  }
  return true;
}

MATCHER(HasInstanceIdToken, "") {
  const sync_pb::DeviceInfoSpecifics& specifics = arg.device_info();
  if (specifics.invalidation_fields().instance_id_token().empty()) {
    *result_listener << "which is empty";
    return false;
  }
  return true;
}

MATCHER(HasAnyInterestedDataTypes, "") {
  const sync_pb::DeviceInfoSpecifics& specifics = arg.device_info();
  if (specifics.invalidation_fields().interested_data_type_ids().empty()) {
    *result_listener << "which is empty";
    return false;
  }
  return true;
}

std::string CacheGuidForSuffix(int suffix) {
  return base::StringPrintf("cache guid %d", suffix);
}

std::string ClientNameForSuffix(int suffix) {
  return base::StringPrintf("client name %d", suffix);
}

std::string SyncUserAgentForSuffix(int suffix) {
  return base::StringPrintf("sync user agent %d", suffix);
}

std::string ChromeVersionForSuffix(int suffix) {
  return base::StringPrintf("chrome version %d", suffix);
}

std::string GooglePlayServicesVersionForSuffix(int suffix) {
  return base::StringPrintf("apk version %d", suffix);
}

std::string SigninScopedDeviceIdForSuffix(int suffix) {
  return base::StringPrintf("signin scoped device id %d", suffix);
}

LocalDeviceNameInfo GetLocalDeviceNameInfoBlocking() {
  base::RunLoop run_loop;
  LocalDeviceNameInfo info;
  GetLocalDeviceNameInfo(base::BindLambdaForTesting(
      [&](LocalDeviceNameInfo local_device_name_info) {
        info = std::move(local_device_name_info);
        run_loop.Quit();
      }));
  run_loop.Run();
  return info;
}

std::string ModelForSuffix(int suffix) {
  return base::StringPrintf("model %d", suffix);
}

std::string ManufacturerForSuffix(int suffix) {
  return base::StringPrintf("manufacturer %d", suffix);
}

std::string SharingVapidFcmTokenForSuffix(int suffix) {
  return base::StringPrintf("sharing vapid fcm token %d", suffix);
}

std::string SharingVapidP256dhForSuffix(int suffix) {
  return base::StringPrintf("sharing vapid p256dh %d", suffix);
}

std::string SharingVapidAuthSecretForSuffix(int suffix) {
  return base::StringPrintf("sharing vapid auth secret %d", suffix);
}

std::string SharingSenderIdFcmTokenForSuffix(int suffix) {
  return base::StringPrintf("sharing sender-id fcm token %d", suffix);
}

std::string SharingChimeRepresentativeTargetIdForSuffix(int suffix) {
  return base::StringPrintf("chime representative target id %d", suffix);
}

std::string SharingSenderIdP256dhForSuffix(int suffix) {
  return base::StringPrintf("sharing sender-id p256dh %d", suffix);
}

std::string SharingSenderIdAuthSecretForSuffix(int suffix) {
  return base::StringPrintf("sharing sender-id auth secret %d", suffix);
}

sync_pb::SharingSpecificFields::EnabledFeatures SharingEnabledFeaturesForSuffix(
    int suffix) {
  return suffix % 2 ? sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2
                    : sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2;
}

std::string SyncInvalidationsInstanceIdTokenForSuffix(int suffix) {
  return base::StringPrintf("instance id token %d", suffix);
}

DataTypeSet SyncInvalidationsInterestedDataTypes() {
  return {BOOKMARKS};
}

DataTypeActivationRequest TestDataTypeActivationRequest(SyncMode sync_mode) {
  DataTypeActivationRequest request;
  request.cache_guid = CacheGuidForSuffix(kLocalSuffix);
  request.sync_mode = sync_mode;
  return request;
}

DeviceInfoSpecifics CreateSpecifics(
    int suffix,
    base::Time last_updated = base::Time::Now()) {
  DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(CacheGuidForSuffix(suffix));
  specifics.set_client_name(ClientNameForSuffix(suffix));
  specifics.set_device_type(kLocalDeviceType);
  specifics.set_sync_user_agent(SyncUserAgentForSuffix(suffix));
  specifics.set_chrome_version(ChromeVersionForSuffix(suffix));
  specifics.set_signin_scoped_device_id(SigninScopedDeviceIdForSuffix(suffix));
  specifics.set_model(ModelForSuffix(suffix));
  specifics.set_manufacturer(ManufacturerForSuffix(suffix));
  specifics.set_full_hardware_class("");
  specifics.set_last_updated_timestamp(TimeToProtoTime(last_updated));
  specifics.mutable_feature_fields()->set_send_tab_to_self_receiving_enabled(
      true);
  specifics.mutable_feature_fields()->set_send_tab_to_self_receiving_type(
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED);
  specifics.mutable_sharing_fields()->set_vapid_fcm_token(
      SharingVapidFcmTokenForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_vapid_p256dh(
      SharingVapidP256dhForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_vapid_auth_secret(
      SharingVapidAuthSecretForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_sender_id_fcm_token_v2(
      SharingSenderIdFcmTokenForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_chime_representative_target_id(
      SharingChimeRepresentativeTargetIdForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_sender_id_p256dh_v2(
      SharingSenderIdP256dhForSuffix(suffix));
  specifics.mutable_sharing_fields()->set_sender_id_auth_secret_v2(
      SharingSenderIdAuthSecretForSuffix(suffix));
  specifics.mutable_sharing_fields()->add_enabled_features(
      SharingEnabledFeaturesForSuffix(suffix));

  specifics.mutable_invalidation_fields()->set_instance_id_token(
      SyncInvalidationsInstanceIdTokenForSuffix(suffix));
  for (const DataType type : SyncInvalidationsInterestedDataTypes()) {
    specifics.mutable_invalidation_fields()->add_interested_data_type_ids(
        GetSpecificsFieldNumberFromDataType(type));
  }

  return specifics;
}

DeviceInfoSpecifics CreateGooglePlayServicesSpecifics(int suffix) {
  DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(CacheGuidForSuffix(suffix));
  specifics.set_client_name(ClientNameForSuffix(suffix));
  specifics.set_device_type(sync_pb::SyncEnums::TYPE_PHONE);
  specifics.set_os_type(sync_pb::SyncEnums::OS_TYPE_ANDROID);
  specifics.set_device_form_factor(
      sync_pb::SyncEnums::DEVICE_FORM_FACTOR_PHONE);
  specifics.mutable_google_play_services_version_info()->set_apk_version_name(
      GooglePlayServicesVersionForSuffix(suffix));
  return specifics;
}

DataTypeState StateWithEncryption(const std::string& encryption_key_name) {
  DataTypeState state;
  state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  state.set_cache_guid(CacheGuidForSuffix(kLocalSuffix));
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

// Creates an EntityData around a copy of the given specifics.
EntityData SpecificsToEntity(const DeviceInfoSpecifics& specifics) {
  EntityData data;
  *data.specifics.mutable_device_info() = specifics;
  return data;
}

std::string CacheGuidToTag(const std::string& guid) {
  return "DeviceInfo_" + guid;
}

// Helper method to reduce duplicated code between tests. Wraps the given
// specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
// returns an EntityChangeList containing them all. Order is maintained.
EntityChangeList EntityAddList(
    const std::vector<DeviceInfoSpecifics>& specifics_list) {
  EntityChangeList changes;
  for (const auto& specifics : specifics_list) {
    changes.push_back(EntityChange::CreateAdd(specifics.cache_guid(),
                                              SpecificsToEntity(specifics)));
  }
  return changes;
}

std::map<std::string, sync_pb::EntitySpecifics> DataBatchToSpecificsMap(
    std::unique_ptr<DataBatch> batch) {
  std::map<std::string, sync_pb::EntitySpecifics> storage_key_to_specifics;
  while (batch && batch->HasNext()) {
    auto [key, data] = batch->Next();
    storage_key_to_specifics[key] = data->specifics;
  }
  return storage_key_to_specifics;
}

class TestLocalDeviceInfoProvider : public MutableLocalDeviceInfoProvider {
 public:
  TestLocalDeviceInfoProvider() = default;

  TestLocalDeviceInfoProvider(const TestLocalDeviceInfoProvider&) = delete;
  TestLocalDeviceInfoProvider& operator=(const TestLocalDeviceInfoProvider&) =
      delete;

  ~TestLocalDeviceInfoProvider() override = default;

  // MutableLocalDeviceInfoProvider implementation.
  void Initialize(const std::string& cache_guid,
                  const std::string& session_name,
                  const std::string& manufacturer_name,
                  const std::string& model_name,
                  const std::string& full_hardware_class,
                  const DeviceInfo* device_info_restored_from_store) override {
    std::string last_fcm_registration_token;
    DataTypeSet last_interested_data_types;
    if (device_info_restored_from_store) {
      last_fcm_registration_token =
          device_info_restored_from_store->fcm_registration_token();
      last_interested_data_types =
          device_info_restored_from_store->interested_data_types();
    }

    std::set<sync_pb::SharingSpecificFields::EnabledFeatures>
        sharing_enabled_features{SharingEnabledFeaturesForSuffix(kLocalSuffix)};
    local_device_info_ = std::make_unique<DeviceInfo>(
        cache_guid, session_name, ChromeVersionForSuffix(kLocalSuffix),
        SyncUserAgentForSuffix(kLocalSuffix), kLocalDeviceType, kLocalDeviceOS,
        kLocalDeviceFormFactor, SigninScopedDeviceIdForSuffix(kLocalSuffix),
        manufacturer_name, model_name, full_hardware_class, base::Time(),
        DeviceInfoUtil::GetPulseInterval(),
        /*send_tab_to_self_receiving_enabled=*/
        true,
        /*send_tab_to_self_receiving_type=*/
        sync_pb::
            SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
        DeviceInfo::SharingInfo(
            {SharingVapidFcmTokenForSuffix(kLocalSuffix),
             SharingVapidP256dhForSuffix(kLocalSuffix),
             SharingVapidAuthSecretForSuffix(kLocalSuffix)},
            {SharingSenderIdFcmTokenForSuffix(kLocalSuffix),
             SharingSenderIdP256dhForSuffix(kLocalSuffix),
             SharingSenderIdAuthSecretForSuffix(kLocalSuffix)},
            SharingChimeRepresentativeTargetIdForSuffix(kLocalSuffix),
            sharing_enabled_features),
        /*paask_info=*/std::nullopt, last_fcm_registration_token,
        last_interested_data_types,
        /*floating_workspace_last_signin_timestamp=*/std::nullopt);
  }

  void Clear() override { local_device_info_.reset(); }

  void UpdateClientName(const std::string& client_name) override {
    ASSERT_TRUE(local_device_info_);
    local_device_info_->set_client_name(client_name);
  }

  void UpdateRecentSignInTime(base::Time time) override {
    ASSERT_TRUE(local_device_info_);
    local_device_info_->set_floating_workspace_last_signin_timestamp(time);
  }

  version_info::Channel GetChannel() const override {
    return version_info::Channel::UNKNOWN;
  }

  MOCK_METHOD(bool, IsUmaEnabledOnCrOSDevice, (), (const));

  const DeviceInfo* GetLocalDeviceInfo() const override {
    if (local_device_info_) {
      if (fcm_registration_token_) {
        local_device_info_->set_fcm_registration_token(
            *fcm_registration_token_);
      }
      if (interested_data_types_) {
        local_device_info_->set_interested_data_types(*interested_data_types_);
      }
      if (paask_info_) {
        auto copy = *paask_info_;
        local_device_info_->set_paask_info(std::move(copy));
      }
    }
    return local_device_info_.get();
  }

  base::CallbackListSubscription RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override {
    NOTIMPLEMENTED();
    return {};
  }

  void UpdateFCMRegistrationToken(const std::string& fcm_registration_token) {
    fcm_registration_token_ = fcm_registration_token;
  }

  void UpdateInterestedDataTypes(const DataTypeSet& data_types) {
    interested_data_types_ = data_types;
  }

  void UpdatePhoneAsASecurityKeyInfo(
      const DeviceInfo::PhoneAsASecurityKeyInfo& paask_info) {
    paask_info_ = paask_info;
  }

 private:
  std::unique_ptr<DeviceInfo> local_device_info_;
  std::optional<std::string> fcm_registration_token_;
  std::optional<DataTypeSet> interested_data_types_;
  std::optional<DeviceInfo::PhoneAsASecurityKeyInfo> paask_info_;
};  // namespace

class DeviceInfoSyncBridgeTest : public testing::Test,
                                 public DeviceInfoTracker::Observer {
 protected:
  DeviceInfoSyncBridgeTest()
      : store_(DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    DeviceInfoPrefs::RegisterProfilePrefs(pref_service_.registry());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    statistics_provider_ =
        std::make_unique<ash::system::ScopedFakeStatisticsProvider>();
#endif

    local_device_name_info_ = GetLocalDeviceNameInfoBlocking();
    // By default, mimic a real processor's behavior for IsTrackingMetadata().
    ON_CALL(mock_processor_, ModelReadyToSync)
        .WillByDefault([this](std::unique_ptr<MetadataBatch> batch) {
          ON_CALL(mock_processor_, IsTrackingMetadata())
              .WillByDefault(Return(
                  batch->GetDataTypeState().initial_sync_state() ==
                  sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE));
        });
  }

  ~DeviceInfoSyncBridgeTest() override {
    // Some tests may never initialize the bridge.
    if (bridge_) {
      bridge_->RemoveObserver(this);
    }

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  DeviceInfoSpecifics CreateLocalDeviceSpecifics(
      const base::Time last_updated = base::Time::Now()) {
    DeviceInfoSpecifics specifics = CreateSpecifics(kLocalSuffix, last_updated);
    specifics.set_model(local_device_name_info_.model_name);
    specifics.set_manufacturer(local_device_name_info_.manufacturer_name);
    return specifics;
  }

  void OnDeviceInfoChange() override { change_count_++; }

  // Initialized the bridge based on the current local device and store.
  void InitializeBridge() {
    auto local_device_info_provider =
        std::make_unique<TestLocalDeviceInfoProvider>();
    // Store a pointer to be able to update the local device info fields.
    local_device_info_provider_ = local_device_info_provider.get();
    bridge_ = std::make_unique<DeviceInfoSyncBridge>(
        std::move(local_device_info_provider),
        DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_processor_.CreateForwardingProcessor(),
        std::make_unique<DeviceInfoPrefs>(&pref_service_, &clock_));
    bridge_->AddObserver(this);
  }

  // Creates the bridge and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeBridge();
    task_environment_.RunUntilIdle();
  }

  void WaitForReadyToSync() {
    ON_CALL(*processor(), IsTrackingMetadata).WillByDefault(Return(true));
    base::RunLoop run_loop;
    EXPECT_CALL(*processor(), ModelReadyToSync)
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Mimics sync being enabled by the user with no remote data. Must be called
  // when the bridge is initialized.
  void EnableSyncAndMergeInitialData(SyncMode sync_mode) {
    DCHECK(bridge_);
    bridge()->OnSyncStarting(TestDataTypeActivationRequest(sync_mode));

    std::unique_ptr<MetadataChangeList> metadata_change_list =
        bridge()->CreateMetadataChangeList();

    metadata_change_list->UpdateDataTypeState(StateWithEncryption(""));
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
    bridge()->MergeFullSyncData(std::move(metadata_change_list),
                                EntityChangeList());
  }

  // Creates the bridge with no prior data on the store, and mimics sync being
  // enabled by the user with no remote data.
  void InitializeAndMergeInitialData(SyncMode sync_mode) {
    InitializeAndPump();
    EnableSyncAndMergeInitialData(sync_mode);
  }

  // Allows access to the store before that will ultimately be used to
  // initialize the bridge.
  DataTypeStore* store() {
    EXPECT_TRUE(store_);
    return store_.get();
  }

  // Get the number of times the bridge notifies observers of changes.
  int change_count() { return change_count_; }

  TestLocalDeviceInfoProvider* local_device() {
    return local_device_info_provider_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider* statistics_provider() {
    EXPECT_TRUE(statistics_provider_);
    return statistics_provider_.get();
  }
#endif

  // Allows access to the bridge after InitializeBridge() is called.
  DeviceInfoSyncBridge* bridge() {
    EXPECT_TRUE(bridge_);
    return bridge_.get();
  }

  MockDataTypeLocalChangeProcessor* processor() { return &mock_processor_; }
  // Should only be called after the bridge has been initialized. Will first
  // recover the bridge's store, so another can be initialized later, and then
  // deletes the bridge.
  void PumpAndShutdown() {
    ASSERT_TRUE(bridge_);
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(this);
  }

  void RestartBridge() {
    PumpAndShutdown();
    InitializeAndPump();
  }

  void ForcePulse() { bridge()->ForcePulseForTest(); }

  void RefreshLocalDeviceInfo() { bridge()->RefreshLocalDeviceInfoIfNeeded(); }

  void CommitToStoreAndWait(std::unique_ptr<WriteBatch> batch) {
    base::RunLoop loop;
    store()->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop, const std::optional<ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  void WriteDataToStore(
      const std::vector<DeviceInfoSpecifics>& specifics_list) {
    std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.cache_guid(), specifics.SerializeAsString());
    }
    CommitToStoreAndWait(std::move(batch));
  }

  void WriteToStoreWithMetadata(
      const std::vector<DeviceInfoSpecifics>& specifics_list,
      DataTypeState state) {
    std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.cache_guid(), specifics.SerializeAsString());
    }
    batch->GetMetadataChangeList()->UpdateDataTypeState(state);
    CommitToStoreAndWait(std::move(batch));
  }

  std::map<std::string, DeviceInfoSpecifics> ReadAllFromStore() {
    std::unique_ptr<DataTypeStore::RecordList> records;
    base::RunLoop loop;
    store()->ReadAllData(base::BindOnce(
        [](std::unique_ptr<DataTypeStore::RecordList>* output_records,
           base::RunLoop* loop, const std::optional<syncer::ModelError>& error,
           std::unique_ptr<DataTypeStore::RecordList> input_records) {
          EXPECT_FALSE(error) << error->ToString();
          EXPECT_THAT(input_records, NotNull());
          *output_records = std::move(input_records);
          loop->Quit();
        },
        &records, &loop));
    loop.Run();
    std::map<std::string, DeviceInfoSpecifics> result;
    if (records) {
      for (const DataTypeStore::Record& record : *records) {
        DeviceInfoSpecifics specifics;
        EXPECT_TRUE(specifics.ParseFromString(record.value));
        result.emplace(record.id, specifics);
      }
    }
    return result;
  }

  std::map<std::string, sync_pb::EntitySpecifics> GetAllData() {
    std::unique_ptr<DataBatch> batch = bridge_->GetAllDataForDebugging();
    EXPECT_NE(nullptr, batch);
    return DataBatchToSpecificsMap(std::move(batch));
  }

  std::map<std::string, sync_pb::EntitySpecifics> GetDataForCommit(
      const std::vector<std::string>& storage_keys) {
    std::unique_ptr<DataBatch> batch = bridge_->GetDataForCommit(storage_keys);
    EXPECT_NE(nullptr, batch);
    return DataBatchToSpecificsMap(std::move(batch));
  }

  const std::string& local_personalizable_name() const {
    return local_device_name_info_.personalizable_name;
  }

  const std::string& local_device_model_name() const {
    return local_device_name_info_.model_name;
  }

 private:
  base::SimpleTestClock clock_;

  int change_count_ = 0;

  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;

  // Holds the store.
  const std::unique_ptr<DataTypeStore> store_;

  // Stores the local device's name information.
  LocalDeviceNameInfo local_device_name_info_;

  TestingPrefServiceSimple pref_service_;

  // Not initialized immediately (upon test's constructor). This allows each
  // test case to modify the dependencies the bridge will be constructed with.
  std::unique_ptr<DeviceInfoSyncBridge> bridge_;

  raw_ptr<TestLocalDeviceInfoProvider> local_device_info_provider_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::system::ScopedFakeStatisticsProvider>
      statistics_provider_;
#endif
};

TEST_F(DeviceInfoSyncBridgeTest, BeforeSyncEnabled) {
  InitializeBridge();
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), IsEmpty());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), IsEmpty());
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagNormal) {
  InitializeBridge();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(guid), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagEmpty) {
  InitializeBridge();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(""), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalDataWithoutMetadata) {
  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  WriteDataToStore({specifics});
  InitializeAndPump();

  // Local data without sync metadata should be thrown away.
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalMetadata) {
  WriteToStoreWithMetadata(std::vector<DeviceInfoSpecifics>(),
                           StateWithEncryption("ekn"));
  InitializeAndPump();

  // Local metadata without data about the local device is corrupt.
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalDataAndMetadata) {
  const DeviceInfoSpecifics local_specifics = CreateLocalDeviceSpecifics();
  DataTypeState state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({local_specifics}, state);

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/_)));
  InitializeAndPump();

  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_THAT(*bridge()->GetDeviceInfo(local_specifics.cache_guid()),
              ModelEqualsSpecifics(local_specifics));
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithMultipleLocalDataAndMetadata) {
  const DeviceInfoSpecifics local_specifics = CreateLocalDeviceSpecifics();
  const DeviceInfoSpecifics remote_specifics = CreateSpecifics(1);
  DataTypeState state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({local_specifics, remote_specifics}, state);

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/_)));
  InitializeAndPump();

  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  EXPECT_THAT(*bridge()->GetDeviceInfo(local_specifics.cache_guid()),
              ModelEqualsSpecifics(local_specifics));
  EXPECT_THAT(*bridge()->GetDeviceInfo(remote_specifics.cache_guid()),
              ModelEqualsSpecifics(remote_specifics));
}

TEST_F(DeviceInfoSyncBridgeTest, GetData) {
  const DeviceInfoSpecifics local_specifics = CreateLocalDeviceSpecifics();
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  WriteToStoreWithMetadata(
      {local_specifics, specifics1, specifics2, specifics3},
      StateWithEncryption("ekn"));
  InitializeAndPump();

  EXPECT_THAT(GetDataForCommit({specifics1.cache_guid()}),
              UnorderedElementsAre(
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1))));

  EXPECT_THAT(
      GetDataForCommit({specifics1.cache_guid(), specifics3.cache_guid()}),
      UnorderedElementsAre(
          Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
          Pair(specifics3.cache_guid(), HasDeviceInfo(specifics3))));

  EXPECT_THAT(
      GetDataForCommit({specifics1.cache_guid(), specifics2.cache_guid(),
                        specifics3.cache_guid()}),
      UnorderedElementsAre(
          Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
          Pair(specifics2.cache_guid(), HasDeviceInfo(specifics2)),
          Pair(specifics3.cache_guid(), HasDeviceInfo(specifics3))));
}

TEST_F(DeviceInfoSyncBridgeTest, GetDataMissing) {
  InitializeAndPump();
  EXPECT_THAT(GetDataForCommit({"does_not_exist"}), IsEmpty());
}

TEST_F(DeviceInfoSyncBridgeTest, GetAllData) {
  const DeviceInfoSpecifics local_specifics = CreateLocalDeviceSpecifics();
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  WriteToStoreWithMetadata({local_specifics, specifics1, specifics2},
                           StateWithEncryption("ekn"));
  InitializeAndPump();

  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(local_device()->GetLocalDeviceInfo()->guid(), _),
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
                  Pair(specifics2.cache_guid(), HasDeviceInfo(specifics2))));
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyIncrementalSyncChangesEmpty) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());

  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityChangeList());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyIncrementalSyncChangesInMemory) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error_on_add = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));

  EXPECT_FALSE(error_on_add);
  const DeviceInfo* info = bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  EXPECT_THAT(*info, ModelEqualsSpecifics(specifics));
  EXPECT_EQ(2, change_count());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateDelete(specifics.cache_guid()));
  auto error_on_delete = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));

  EXPECT_FALSE(error_on_delete);
  EXPECT_FALSE(bridge()->GetDeviceInfo(specifics.cache_guid()));
  EXPECT_EQ(3, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyIncrementalSyncChangesStore) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  DataTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateDataTypeState(state);

  auto error = bridge()->ApplyIncrementalSyncChanges(
      std::move(metadata_changes), EntityAddList({specifics}));
  EXPECT_FALSE(error);
  EXPECT_EQ(2, change_count());

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/IsEmpty())));
  RestartBridge();

  const DeviceInfo* info = bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  EXPECT_THAT(*info, ModelEqualsSpecifics(specifics));
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyIncrementalSyncChangesWithLocalGuid) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());

  ASSERT_TRUE(
      bridge()->GetDeviceInfo(local_device()->GetLocalDeviceInfo()->guid()));
  ASSERT_EQ(1, change_count());

  // The bridge should ignore updates using this specifics because its guid will
  // match the local device.
  EXPECT_CALL(*processor(), Put).Times(0);

  const DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  auto error_on_add = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));
  EXPECT_FALSE(error_on_add);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyDeleteNonexistent) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("guid"));
  EXPECT_CALL(*processor(), Delete).Times(0);
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeEmpty) {
  const std::string kLocalGuid = CacheGuidForSuffix(kLocalSuffix);

  InitializeAndPump();

  ASSERT_FALSE(local_device()->GetLocalDeviceInfo());
  ASSERT_FALSE(bridge()->IsPulseTimerRunningForTest());

  EXPECT_CALL(*processor(), Put(kLocalGuid, _, _));
  EXPECT_CALL(*processor(), Delete).Times(0);

  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  auto error = bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                                           EntityChangeList());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(kLocalGuid, local_device()->GetLocalDeviceInfo()->guid());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeLocalGuid) {
  const std::string kLocalGuid = CacheGuidForSuffix(kLocalSuffix);

  InitializeAndPump();

  ASSERT_FALSE(local_device()->GetLocalDeviceInfo());

  EXPECT_CALL(*processor(), Put(kLocalGuid, _, _));
  EXPECT_CALL(*processor(), Delete).Times(0);

  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  auto error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateLocalDeviceSpecifics()}));

  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(kLocalGuid, local_device()->GetLocalDeviceInfo()->guid());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevices) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  // CountActiveDevicesByType() may not behave as expected if there are multiple
  // devices with the same creation and modification time. So we need to ensure
  // different time here.
  ON_CALL(*processor(), GetEntityCreationTime)
      .WillByDefault(Return(base::Time::Now() - base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime)
      .WillByDefault(Return(base::Time::Now()));

  // Regardless of the time, these following two
  // ApplyIncrementalSyncChanges(...) calls have the same guid as the local
  // device.
  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateLocalDeviceSpecifics()}));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateLocalDeviceSpecifics()}));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  // A different guid will actually contribute to the count.
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({CreateSpecifics(1)}));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 2}}),
            bridge()->CountActiveDevicesByType());

  // Now set time to long ago in the past, it should not be active anymore.
  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateSpecifics(1, base::Time::Now() - base::Days(365))}));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithOverlappingTime) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  ASSERT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);

  // Time ranges are overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(3)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(2)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(2)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(5)));

  // With two devices, the local device gets ignored because it doesn't overlap.
  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2}));

  ASSERT_EQ(3u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 2}}),
            bridge()->CountActiveDevicesByType());

  // The third device is also overlapping with the first two (and the local one
  // is still excluded).
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 3}}),
            bridge()->CountActiveDevicesByType());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithNonOverlappingTime) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  ASSERT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);

  // Time ranges are non-overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(2)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(5)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(6)));

  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2, specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());
}

TEST_F(DeviceInfoSyncBridgeTest,
       CountActiveDevicesWithNonOverlappingTimeAndDistinctType) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  ASSERT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  // We avoid TYPE_LINUX below to prevent collisions with the local device,
  // exposed as Linux by LocalDeviceInfoProviderMock.
  specifics1.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
  specifics2.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
  specifics3.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);

  // Time ranges are non-overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(2)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(5)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(6)));

  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2, specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 3},
                            {DeviceInfo::FormFactor::kPhone, 1}}),
            bridge()->CountActiveDevicesByType());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithMalformedTimestamps) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  ASSERT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);

  // Time ranges are overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(Return(base::Time::UnixEpoch() + base::Minutes(2)));

  // With two devices, the local device gets ignored because it doesn't overlap.
  bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2}));

  ASSERT_EQ(3u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());
}

TEST_F(DeviceInfoSyncBridgeTest,
       ShouldFilterOutNonChromeClientsFromDeviceTracker) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  // Local device.
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 1}}),
            bridge()->CountActiveDevicesByType());

  // CountActiveDevicesByType() may not behave as expected if there are multiple
  // devices with the same creation and modification time. So we need to ensure
  // different time here.
  ON_CALL(*processor(), GetEntityCreationTime)
      .WillByDefault(Return(base::Time::Now() - base::Minutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime)
      .WillByDefault(Return(base::Time::Now()));

  // A different guid will contribute to the count.
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({CreateSpecifics(1)}));
  ASSERT_THAT(GetAllData(), SizeIs(2));
  ASSERT_THAT(bridge()->GetAllDeviceInfo(), SizeIs(2));
  ASSERT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 2}}),
            bridge()->CountActiveDevicesByType());
  ASSERT_THAT(bridge()->GetDeviceInfo(CacheGuidForSuffix(1)), NotNull());

  // If the Chrome version is not present, it should not be exposed as Chrome
  // device.
  sync_pb::DeviceInfoSpecifics specifics2 =
      CreateGooglePlayServicesSpecifics(2);
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({specifics2}));
  ASSERT_THAT(GetAllData(), SizeIs(3));
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), SizeIs(3));
  EXPECT_THAT(bridge()->GetAllChromeDeviceInfo(), SizeIs(2));
  EXPECT_THAT(bridge()->GetAllChromeDeviceInfo(),
              Not(Contains(Pointee(HasCacheGuid(CacheGuidForSuffix(2))))));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 2}}),
            bridge()->CountActiveDevicesByType());
  EXPECT_THAT(bridge()->GetDeviceInfo(CacheGuidForSuffix(2)), IsNull());

  // If only the non-legacy field is present, the device should still be exposed
  // in DeviceInfoTracker.
  sync_pb::DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  specifics3.clear_chrome_version();
  specifics3.mutable_chrome_version_info()->set_version_number("someversion");
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({specifics3}));
  ASSERT_THAT(GetAllData(), SizeIs(4));
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), SizeIs(4));
  EXPECT_THAT(bridge()->GetAllChromeDeviceInfo(), SizeIs(3));
  EXPECT_EQ(DeviceCountMap({{kLocalDeviceFormFactor, 3}}),
            bridge()->CountActiveDevicesByType());
  EXPECT_THAT(bridge()->GetDeviceInfo(CacheGuidForSuffix(3)), NotNull());
}

TEST_F(DeviceInfoSyncBridgeTest, SendLocalData) {
  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  InitializeAndMergeInitialData(SyncMode::kFull);
  EXPECT_EQ(1, change_count());
  testing::Mock::VerifyAndClearExpectations(processor());

  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  ForcePulse();
  EXPECT_EQ(2, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyDisableSyncChanges) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(1, change_count());
  ASSERT_FALSE(ReadAllFromStore().empty());
  ASSERT_TRUE(bridge()->IsPulseTimerRunningForTest());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));

  ASSERT_FALSE(error);
  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(2, change_count());

  // Should clear out all local data and notify observers.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(3, change_count());
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_FALSE(bridge()->IsPulseTimerRunningForTest());

  // Reloading from storage shouldn't contain remote data.
  RestartBridge();
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());

  // If sync is re-enabled and the remote data is now empty, we shouldn't
  // contain remote data.
  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              EntityChangeList());
  // Local device.
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, ExpireOldEntriesUponStartup) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(1, change_count());
  ASSERT_FALSE(ReadAllFromStore().empty());

  const DeviceInfoSpecifics specifics_old =
      CreateSpecifics(1, base::Time::Now() - base::Days(57));
  const DeviceInfoSpecifics specifics_fresh =
      CreateSpecifics(1, base::Time::Now() - base::Days(55));
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics_old, specifics_fresh}));

  ASSERT_FALSE(error);
  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(2, change_count());

  // Reloading from storage should expire the old remote entity (but keep the
  // fresh one).
  RestartBridge();
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  // Make sure this is well persisted to the DB store.
  EXPECT_THAT(ReadAllFromStore(),
              UnorderedElementsAre(
                  Pair(local_device()->GetLocalDeviceInfo()->guid(), _),
                  Pair(specifics_fresh.cache_guid(), _)));
}

TEST_F(DeviceInfoSyncBridgeTest, RefreshLocalDeviceInfo) {
  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  InitializeAndMergeInitialData(SyncMode::kFull);
  EXPECT_EQ(1, change_count());
  testing::Mock::VerifyAndClearExpectations(processor());

  // Check that the device is not updated if nothing has been changed.
  RefreshLocalDeviceInfo();
  EXPECT_EQ(1, change_count());

  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  ASSERT_THAT(local_device()->GetLocalDeviceInfo()->fcm_registration_token(),
              IsEmpty());
  local_device()->UpdateFCMRegistrationToken(
      SyncInvalidationsInstanceIdTokenForSuffix(kLocalSuffix));
  RefreshLocalDeviceInfo();
  EXPECT_EQ(2, change_count());

  // Setting Phone-as-a-security-key fields should trigger an update.
  ASSERT_FALSE(local_device()->GetLocalDeviceInfo()->paask_info().has_value());
  DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = 123;
  paask_info.contact_id = {1, 2, 3, 4};
  paask_info.secret = {5, 6, 7, 8};
  paask_info.id = 321;
  paask_info.peer_public_key_x962 = {10, 11, 12, 13};
  local_device()->UpdatePhoneAsASecurityKeyInfo(paask_info);

  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  RefreshLocalDeviceInfo();
  EXPECT_EQ(3, change_count());

  // Rotating the PaaSK key should not trigger an update. These fields depend
  // on the current time and are only updated when an update would be sent
  // anyway.
  paask_info.secret = {9, 10, 11, 12};
  paask_info.id = 322;
  local_device()->UpdatePhoneAsASecurityKeyInfo(paask_info);
  RefreshLocalDeviceInfo();
  EXPECT_EQ(3, change_count());

  // But updating other PaaSK fields does trigger an update.
  paask_info.tunnel_server_domain = 124;
  local_device()->UpdatePhoneAsASecurityKeyInfo(paask_info);
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  RefreshLocalDeviceInfo();
  EXPECT_EQ(4, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, DeviceNameForTransportOnlySyncMode) {
  InitializeAndMergeInitialData(SyncMode::kTransportOnly);
  ASSERT_EQ(1, change_count());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());

  EXPECT_EQ(local_device_model_name(),
            local_device()->GetLocalDeviceInfo()->client_name());
}

TEST_F(DeviceInfoSyncBridgeTest, DeviceNameForFullSyncMode) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1, change_count());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());

  EXPECT_EQ(local_personalizable_name(),
            local_device()->GetLocalDeviceInfo()->client_name());
}

// Tests local client name when device is initially synced with transport only
// sync mode, but the sync mode is not available after restart since it is not
// persisted.
TEST_F(DeviceInfoSyncBridgeTest,
       DeviceNameForTransportOnlySyncMode_RestartBridge) {
  std::string expected_device_name = local_device_model_name();
  InitializeAndMergeInitialData(SyncMode::kTransportOnly);

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  ASSERT_EQ(expected_device_name,
            local_device()->GetLocalDeviceInfo()->client_name());

  EXPECT_CALL(*processor(),
              Put(local_device()->GetLocalDeviceInfo()->guid(), _, _))
      .Times(0);
  RestartBridge();
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(expected_device_name,
            local_device()->GetLocalDeviceInfo()->client_name());
}

// Tests local client name when device is initially synced with full sync mode,
// but the sync mode is not available after restart since it is not persisted.
TEST_F(DeviceInfoSyncBridgeTest, DeviceNameForFullSyncMode_RestartBridge) {
  std::string expected_device_name = local_personalizable_name();
  InitializeAndMergeInitialData(SyncMode::kFull);

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  ASSERT_EQ(expected_device_name,
            local_device()->GetLocalDeviceInfo()->client_name());

  EXPECT_CALL(*processor(),
              Put(local_device()->GetLocalDeviceInfo()->guid(), _, _))
      .Times(0);
  RestartBridge();
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(expected_device_name,
            local_device()->GetLocalDeviceInfo()->client_name());
}

TEST_F(DeviceInfoSyncBridgeTest, RefreshLocalDeviceNameForSyncModeToggle) {
  std::string expected_device_name_full_sync = local_personalizable_name();
  std::string expected_device_name_transport_only = local_device_model_name();

  // Initialize with full sync mode.
  InitializeAndMergeInitialData(SyncMode::kFull);
  const syncer::DeviceInfo* device = local_device()->GetLocalDeviceInfo();

  ASSERT_TRUE(device);
  ASSERT_EQ(expected_device_name_full_sync, device->client_name());

  // Toggle to transport only sync mode.
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.
  bridge()->OnSyncStarting(
      TestDataTypeActivationRequest(SyncMode::kTransportOnly));

  device = local_device()->GetLocalDeviceInfo();
  ASSERT_TRUE(device);
  ASSERT_EQ(expected_device_name_transport_only, device->client_name());

  // Toggle to full sync mode.
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.
  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));

  device = local_device()->GetLocalDeviceInfo();
  ASSERT_TRUE(device);
  ASSERT_EQ(expected_device_name_full_sync, device->client_name());
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldSendInvalidationFields) {
  EXPECT_CALL(*processor(),
              Put(_,
                  HasSpecifics(
                      AllOf(HasInstanceIdToken(), HasAnyInterestedDataTypes())),
                  _));
  InitializeAndPump();
  local_device()->UpdateFCMRegistrationToken(
      SyncInvalidationsInstanceIdTokenForSuffix(kLocalSuffix));
  local_device()->UpdateInterestedDataTypes(
      SyncInvalidationsInterestedDataTypes());
  EnableSyncAndMergeInitialData(SyncMode::kFull);
}

TEST_F(DeviceInfoSyncBridgeTest,
       ShouldNotifyWhenAdditionalInterestedDataTypesSynced) {
  InitializeAndPump();
  local_device()->UpdateInterestedDataTypes({syncer::BOOKMARKS});
  EnableSyncAndMergeInitialData(SyncMode::kFull);

  base::MockRepeatingCallback<void(const DataTypeSet&)> callback;
  bridge()->SetCommittedAdditionalInterestedDataTypesCallback(callback.Get());
  local_device()->UpdateInterestedDataTypes(
      {syncer::BOOKMARKS, syncer::SESSIONS});

  bridge()->RefreshLocalDeviceInfoIfNeeded();

  const std::string guid = local_device()->GetLocalDeviceInfo()->guid();
  EXPECT_CALL(*processor(), IsEntityUnsynced(guid)).WillOnce(Return(false));

  EXPECT_CALL(callback, Run(DataTypeSet({syncer::SESSIONS})));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityChangeList());
}

TEST_F(DeviceInfoSyncBridgeTest,
       ShouldNotNotifyWithoutAdditionalInterestedDataTypes) {
  InitializeAndPump();
  local_device()->UpdateInterestedDataTypes(
      {syncer::BOOKMARKS, syncer::SESSIONS});
  EnableSyncAndMergeInitialData(SyncMode::kFull);

  base::MockRepeatingCallback<void(const DataTypeSet&)> callback;
  bridge()->SetCommittedAdditionalInterestedDataTypesCallback(callback.Get());
  local_device()->UpdateInterestedDataTypes({syncer::BOOKMARKS});

  bridge()->RefreshLocalDeviceInfoIfNeeded();

  std::string guid = local_device()->GetLocalDeviceInfo()->guid();
  EXPECT_CALL(*processor(), IsEntityUnsynced(guid)).WillOnce(Return(false));
  EXPECT_CALL(callback, Run).Times(0);
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityChangeList());
}

// This test mimics the case when OnSyncStarting is called before the metadata
// is loaded from the storage.
TEST_F(DeviceInfoSyncBridgeTest,
       ShouldCleanUpMetadataOnInvalidCacheGuidAfterReadMetadata) {
  const std::string kInvalidCacheGuid = "invalid_cache_guid";

  DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  specifics.set_cache_guid(kInvalidCacheGuid);

  DataTypeState data_type_state = StateWithEncryption("ekn");
  data_type_state.set_cache_guid(kInvalidCacheGuid);

  WriteToStoreWithMetadata({specifics}, data_type_state);

  InitializeBridge();
  ASSERT_FALSE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  ASSERT_FALSE(bridge()->IsRecentLocalCacheGuid(kInvalidCacheGuid));

  EXPECT_CALL(*processor(), IsTrackingMetadata).WillRepeatedly(Return(false));
  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));

  EXPECT_TRUE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  EXPECT_FALSE(bridge()->IsRecentLocalCacheGuid(kInvalidCacheGuid));
  EXPECT_THAT(bridge()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo(),
              IsNull());

  base::RunLoop run_loop;
  EXPECT_CALL(*processor(), ModelReadyToSync)
      .WillOnce([this, &run_loop](std::unique_ptr<MetadataBatch> batch) {
        // Mimic CTBMTP behaviour on invalid cache GUID.
        std::unique_ptr<MetadataChangeList> change_list =
            bridge()->CreateMetadataChangeList();
        change_list->ClearMetadata(CacheGuidForSuffix(kLocalSuffix));
        change_list->ClearDataTypeState();
        bridge()->ApplyDisableSyncChanges(std::move(change_list));

        bridge()->OnSyncStarting(
            TestDataTypeActivationRequest(SyncMode::kFull));

        run_loop.Quit();
      });
  run_loop.Run();

  EXPECT_TRUE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  EXPECT_FALSE(bridge()->IsRecentLocalCacheGuid(kInvalidCacheGuid));
  EXPECT_THAT(bridge()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo(),
              IsNull());
}

// This test mimics the opposite case when OnSyncStarting is called after the
// metadata is loaded from the storage.
TEST_F(DeviceInfoSyncBridgeTest,
       ShouldCleanUpMetadataOnInvalidCacheGuidAfterSyncStarting) {
  const std::string kInvalidCacheGuid = "invalid_cache_guid";

  DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  specifics.set_cache_guid(kInvalidCacheGuid);

  DataTypeState data_type_state = StateWithEncryption("ekn");
  data_type_state.set_cache_guid(kInvalidCacheGuid);

  WriteToStoreWithMetadata({specifics}, data_type_state);
  InitializeBridge();
  ASSERT_FALSE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  ASSERT_FALSE(bridge()->IsRecentLocalCacheGuid(kInvalidCacheGuid));

  // Wait until the metadata is loaded.
  base::RunLoop run_loop;
  EXPECT_CALL(*processor(), IsTrackingMetadata).WillOnce(Return(true));
  EXPECT_CALL(*processor(), ModelReadyToSync)
      .WillOnce([&run_loop](std::unique_ptr<MetadataBatch> batch) {
        run_loop.Quit();
      });
  run_loop.Run();

  EXPECT_FALSE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  EXPECT_TRUE(bridge()->IsRecentLocalCacheGuid(kInvalidCacheGuid));
  EXPECT_THAT(bridge()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo(),
              NotNull());

  // Mimic CTBMTP behaviour on invalid cache GUID.
  EXPECT_CALL(*processor(), IsTrackingMetadata).WillRepeatedly(Return(false));
  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));

  std::unique_ptr<MetadataChangeList> change_list =
      bridge()->CreateMetadataChangeList();
  change_list->ClearMetadata(CacheGuidForSuffix(kLocalSuffix));
  change_list->ClearDataTypeState();
  bridge()->ApplyDisableSyncChanges(std::move(change_list));

  bridge()->OnSyncStarting(TestDataTypeActivationRequest(SyncMode::kFull));

  // Check that the cache GUID is in recent devices.
  EXPECT_TRUE(
      bridge()->IsRecentLocalCacheGuid(CacheGuidForSuffix(kLocalSuffix)));
  EXPECT_THAT(bridge()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo(),
              IsNull());
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldInvokeCallbackOnReadAllMetadata) {
  const DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  const DataTypeState data_type_state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({specifics}, data_type_state);

  InitializeBridge();
  // Wait until the metadata is loaded.
  base::RunLoop run_loop;
  EXPECT_CALL(*processor(), IsTrackingMetadata).WillOnce(Return(true));
  EXPECT_CALL(*processor(), ModelReadyToSync)
      .WillOnce([&run_loop](std::unique_ptr<MetadataBatch> batch) {
        run_loop.Quit();
      });
  run_loop.Run();

  // Check that the bridge won't call SendLocalData() during initial merge.
  EXPECT_CALL(*processor(), Put).Times(0);

  // Check that the bridge has notified observers even if the local data hasn't
  // been changed.
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldRemoveDeviceInfoOnTombstone) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  std::optional<ModelError> error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));
  ASSERT_FALSE(error);
  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());

  EntityChangeList changes;
  changes.push_back(EntityChange::CreateDelete(specifics.cache_guid()));
  error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(changes));
  ASSERT_FALSE(error);

  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_NE(bridge()->GetAllDeviceInfo().front()->guid(),
            specifics.cache_guid());
}

TEST_F(DeviceInfoSyncBridgeTest,
       ShouldReuploadOnceAfterLocalDeviceInfoTombstone) {
  InitializeAndMergeInitialData(SyncMode::kFull);
  ASSERT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  EntityChangeList changes;
  changes.push_back(
      EntityChange::CreateDelete(CacheGuidForSuffix(kLocalSuffix)));

  // An incoming deletion for the local device info should result in a reupload.
  // The reupload should only be triggered once, to prevent any possible
  // ping-pong between devices.
  EXPECT_CALL(*processor(), Put(CacheGuidForSuffix(kLocalSuffix), _, _));
  std::optional<ModelError> error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(changes));
  ASSERT_FALSE(error);

  // The local device info should still exist.
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  changes.clear();
  changes.push_back(
      EntityChange::CreateDelete(CacheGuidForSuffix(kLocalSuffix)));
  error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(changes));
  ASSERT_FALSE(error);

  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldForceLocalDeviceInfoUpload) {
  const DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  const DataTypeState data_type_state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({specifics}, data_type_state);

  InitializeBridge();

  bridge()->ForcePulseForTest();

  // Check that the bridge calls SendLocalData() during initialization.
  EXPECT_CALL(*processor(), Put);

  WaitForReadyToSync();
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldNotUploadRecentLocalDeviceUponStartup) {
  const DeviceInfoSpecifics specifics = CreateLocalDeviceSpecifics();
  const DataTypeState data_type_state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({specifics}, data_type_state);

  InitializeBridge();

  // Check that the bridge doesn't call SendLocalData() during initialization
  // (because the local device info has recent last update time).
  EXPECT_CALL(*processor(), Put).Times(0);

  WaitForReadyToSync();
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldUploadOutdatedLocalDeviceInfo) {
  // Create an outdated local device info which should be reuploaded during
  // initialization.
  const DeviceInfoSpecifics specifics =
      CreateLocalDeviceSpecifics(base::Time::Now() - base::Days(10));
  const DataTypeState data_type_state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({specifics}, data_type_state);

  InitializeBridge();

  // Check that the bridge calls SendLocalData() during initialization.
  EXPECT_CALL(*processor(), Put);

  WaitForReadyToSync();
}

TEST_F(DeviceInfoSyncBridgeTest, ShouldDeriveOsFromDeviceType) {
  const DeviceInfoSpecifics local_specifics = CreateLocalDeviceSpecifics();
  WriteToStoreWithMetadata({local_specifics}, StateWithEncryption("ekn"));
  InitializeAndPump();

  // Test LINUX desktop device info.
  EXPECT_EQ(bridge()->GetDeviceInfo(local_specifics.cache_guid())->os_type(),
            kLocalDeviceOS);
  EXPECT_THAT(
      bridge()->GetDeviceInfo(local_specifics.cache_guid())->form_factor(),
      kLocalDeviceFormFactor);

  // Test Android phone device info.
  {
    DeviceInfoSpecifics remote_specifics = CreateSpecifics(1);
    remote_specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
    bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityAddList({remote_specifics}));
    EXPECT_THAT(
        bridge()->GetDeviceInfo(remote_specifics.cache_guid())->os_type(),
        DeviceInfo::OsType::kAndroid);
    EXPECT_THAT(
        bridge()->GetDeviceInfo(remote_specifics.cache_guid())->form_factor(),
        DeviceInfo::FormFactor::kPhone);
  }

  // Test IOS phone device info specifying the manufacturer.
  {
    DeviceInfoSpecifics remote_specifics = CreateSpecifics(1);
    remote_specifics.set_manufacturer("Apple Inc.");
    remote_specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
    bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityAddList({remote_specifics}));
    EXPECT_THAT(
        bridge()->GetDeviceInfo(remote_specifics.cache_guid())->os_type(),
        DeviceInfo::OsType::kIOS);
    EXPECT_THAT(
        bridge()->GetDeviceInfo(remote_specifics.cache_guid())->form_factor(),
        DeviceInfo::FormFactor::kPhone);
  }
}

}  // namespace

}  // namespace syncer
