// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::WalletMetadataSpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::Return;
using testing::UnorderedElementsAre;

// Non-UTF8 server IDs.
const char kAddr1ServerId[] = "addr1\xEF\xBF\xBE";
const char kAddr2ServerId[] = "addr2\xEF\xBF\xBE";
const char kCard1ServerId[] = "card1\xEF\xBF\xBE";
const char kCard2ServerId[] = "card2\xEF\xBF\xBE";

// Base64 encodings of the server IDs, used as ids in WalletMetadataSpecifics
// (these are suitable for syncing, because they are valid UTF-8).
const char kAddr1SpecificsId[] = "YWRkcjHvv74=";
const char kAddr2SpecificsId[] = "YWRkcjLvv74=";
const char kCard1SpecificsId[] = "Y2FyZDHvv74=";
const char kCard2SpecificsId[] = "Y2FyZDLvv74=";

const std::string kAddr1StorageKey =
    GetStorageKeyForWalletMetadataTypeAndSpecificsId(
        WalletMetadataSpecifics::ADDRESS,
        kAddr1SpecificsId);
const std::string kCard1StorageKey =
    GetStorageKeyForWalletMetadataTypeAndSpecificsId(
        WalletMetadataSpecifics::CARD,
        kCard1SpecificsId);

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "address-YWRkcjHvv74=";
const char kCard1SyncTag[] = "card-Y2FyZDHvv74=";

const char kLocalAddr1ServerId[] = "e171e3ed-858a-4dd5-9bf3-8517f14ba5fc";
const char kLocalAddr2ServerId[] = "fa232b9a-f248-4e5a-8d76-d46f821c0c5f";

const char kLocaleString[] = "en-US";

const char kDefaultCacheGuid[] = "CacheGuid";

base::Time UseDateFromProtoValue(int64_t use_date_proto_value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(use_date_proto_value));
}

const base::Time kDefaultTime = UseDateFromProtoValue(100);

int64_t UseDateToProtoValue(base::Time use_date) {
  return use_date.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

std::string GetAddressStorageKey(const std::string& specifics_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      WalletMetadataSpecifics::ADDRESS, specifics_id);
}

std::string GetCardStorageKey(const std::string& specifics_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      WalletMetadataSpecifics::CARD, specifics_id);
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddressWithDetails(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date,
    bool has_converted = false) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::ADDRESS);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // False is the default value according to the constructor of AutofillProfile.
  specifics.set_address_has_converted(has_converted);
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddress(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForAddressWithDetails(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kDefaultTime));
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCardWithDetails(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date,
    const std::string& billing_address_id = "") {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::CARD);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // "" is the default value according to the constructor of AutofillProfile;
  // this field is Base64 encoded in the protobuf.
  specifics.set_card_billing_address_id(GetBase64EncodedId(billing_address_id));
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCard(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForCardWithDetails(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kDefaultTime));
}

AutofillProfile CreateServerProfileWithDetails(const std::string& server_id,
                                               size_t use_count,
                                               int64_t use_date,
                                               bool has_converted = false) {
  AutofillProfile profile = CreateServerProfile(server_id);
  profile.set_use_count(use_count);
  profile.set_use_date(UseDateFromProtoValue(use_date));
  profile.set_has_converted(has_converted);
  return profile;
}

CreditCard CreateServerCreditCardWithDetails(
    const std::string& server_id,
    size_t use_count,
    int64_t use_date,
    const std::string& billing_address_id = "") {
  CreditCard card = CreateServerCreditCard(server_id);
  card.set_use_count(use_count);
  card.set_use_date(UseDateFromProtoValue(use_date));
  card.set_billing_address_id(billing_address_id);
  return card;
}

AutofillProfile CreateLocalProfileWithDetails(size_t use_count,
                                              int64_t use_date) {
  AutofillProfile profile;
  DCHECK_EQ(profile.record_type(), AutofillProfile::LOCAL_PROFILE);
  profile.set_use_count(use_count);
  profile.set_use_date(UseDateFromProtoValue(use_date));
  return profile;
}

CreditCard CreateLocalCreditCardWithDetails(size_t use_count,
                                            int64_t use_date) {
  CreditCard card;
  DCHECK_EQ(card.record_type(), CreditCard::LOCAL_CARD);
  card.set_use_count(use_count);
  card.set_use_date(UseDateFromProtoValue(use_date));
  return card;
}

AutofillProfile CreateServerProfileFromSpecifics(
    const WalletMetadataSpecifics& specifics) {
  return CreateServerProfileWithDetails(
      GetBase64DecodedId(specifics.id()), specifics.use_count(),
      specifics.use_date(), specifics.address_has_converted());
}

CreditCard CreateServerCreditCardFromSpecifics(
    const WalletMetadataSpecifics& specifics) {
  return CreateServerCreditCardWithDetails(
      GetBase64DecodedId(specifics.id()), specifics.use_count(),
      specifics.use_date(),
      GetBase64DecodedId(specifics.card_billing_address_id()));
}

void ExtractWalletMetadataSpecificsFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<WalletMetadataSpecifics>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.wallet_metadata());
  }
}

std::string WalletMetadataSpecificsAsDebugString(
    const WalletMetadataSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", use_count: " << specifics.use_count()
         << ", use_date: " << specifics.use_date()
         << ", card_billing_address_id: "
         << (specifics.has_card_billing_address_id()
                 ? specifics.card_billing_address_id()
                 : "not_set")
         << ", address_has_converted: "
         << (specifics.has_address_has_converted()
                 ? (specifics.address_has_converted() ? "true" : "false")
                 : "not_set")
         << "]";
  return output.str();
}

std::vector<std::string> GetSortedSerializedSpecifics(
    const std::vector<WalletMetadataSpecifics>& specifics) {
  std::vector<std::string> serialized;
  for (const WalletMetadataSpecifics& entry : specifics) {
    serialized.push_back(entry.SerializeAsString());
  }
  std::sort(serialized.begin(), serialized.end());
  return serialized;
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << WalletMetadataSpecificsAsDebugString(arg) << "\n"
                     << "did not match expected\n"
                     << WalletMetadataSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

MATCHER_P(HasSpecifics, expected, "") {
  const WalletMetadataSpecifics& arg_specifics =
      arg->specifics.wallet_metadata();

  if (arg_specifics.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << WalletMetadataSpecificsAsDebugString(arg_specifics)
                     << "\ndid not match expected\n"
                     << WalletMetadataSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

}  // namespace

class AutofillWalletMetadataSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletMetadataSyncBridgeTest() {}

  AutofillWalletMetadataSyncBridgeTest(
      const AutofillWalletMetadataSyncBridgeTest&) = delete;
  AutofillWalletMetadataSyncBridgeTest& operator=(
      const AutofillWalletMetadataSyncBridgeTest&) = delete;

  ~AutofillWalletMetadataSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kDefaultTime);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_METADATA, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done = true) {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.set_initial_sync_state(
        initial_sync_done
            ? sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE
            : sync_pb::
                  ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_METADATA));
    model_type_state.set_cache_guid(kDefaultCacheGuid);
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_METADATA,
                                              model_type_state));
    bridge_ = std::make_unique<AutofillWalletMetadataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  void StopSyncing() {
    real_processor_->OnSyncStopping(syncer::CLEAR_METADATA);
  }

  void Shutdown() { real_processor_->OnSyncStopping(syncer::KEEP_METADATA); }

  void StartSyncing(
      const std::vector<WalletMetadataSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    request.cache_guid = kDefaultCacheGuid;
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    ReceiveUpdates(remote_data);
  }

  void ReceiveUpdates(const std::vector<WalletMetadataSpecifics>& remote_data) {
    // Make sure each update has an updated response version so that it does not
    // get filtered out as reflection by the processor.
    ++response_version;
    // After this update initial sync is for sure done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_state(
        sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    syncer::UpdateResponseDataList updates;
    for (const WalletMetadataSpecifics& specifics : remote_data) {
      updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(updates),
                                      /*gc_directive=*/absl::nullopt);
  }

  void ReceiveTombstones(
      const std::vector<WalletMetadataSpecifics>& remote_tombstones) {
    // Make sure each update has an updated response version so that it does not
    // get filtered out as reflection by the processor.
    ++response_version;
    // After this update initial sync is for sure done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_state(
        sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    syncer::UpdateResponseDataList updates;
    for (const WalletMetadataSpecifics& specifics : remote_tombstones) {
      updates.push_back(
          SpecificsToUpdateResponse(specifics, /*is_deleted=*/true));
    }
    real_processor_->OnUpdateReceived(state, std::move(updates),
                                      /*gc_directive=*/absl::nullopt);
  }

  EntityData SpecificsToEntity(const WalletMetadataSpecifics& specifics,
                               bool is_deleted = false) {
    EntityData data;
    *data.specifics.mutable_wallet_metadata() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_METADATA, bridge()->GetClientTag(data));
    if (is_deleted) {
      // Specifics had to be set in order to generate the client tag. Since
      // deleted entity is defined by specifics being empty, we need to clear
      // them now.
      data.specifics = sync_pb::EntitySpecifics();
    }
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const WalletMetadataSpecifics& specifics,
      bool is_deleted = false) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics, is_deleted);
    data.response_version = response_version;
    return data;
  }

  std::vector<WalletMetadataSpecifics> GetAllLocalData() {
    std::vector<WalletMetadataSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractWalletMetadataSpecificsFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  // Like GetAllData() but it also checks that cache is consistent with the disk
  // content.
  std::vector<WalletMetadataSpecifics> GetAllLocalDataInclRestart() {
    std::vector<WalletMetadataSpecifics> data_before = GetAllLocalData();
    ResetProcessor();
    ResetBridge();
    std::vector<WalletMetadataSpecifics> data_after = GetAllLocalData();
    EXPECT_THAT(GetSortedSerializedSpecifics(data_before),
                ElementsAreArray(GetSortedSerializedSpecifics(data_after)));
    return data_after;
  }

  std::vector<WalletMetadataSpecifics> GetLocalData(
      AutofillWalletMetadataSyncBridge::StorageKeyList storage_keys) {
    std::vector<WalletMetadataSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetData(storage_keys,
                      base::BindLambdaForTesting(
                          [&loop, &data](std::unique_ptr<DataBatch> batch) {
                            ExtractWalletMetadataSpecificsFromDataBatch(
                                std::move(batch), &data);
                            loop.Quit();
                          }));
    loop.Run();
    return data;
  }

  std::vector<std::string> GetLocalSyncMetadataStorageKeys() {
    std::vector<std::string> storage_keys;

    AutofillTable* table = AutofillTable::FromWebDatabase(&db_);
    syncer::MetadataBatch batch;
    if (table->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_METADATA, &batch)) {
      for (const auto& [storage_key, metadata] : batch.GetAllMetadata()) {
        storage_keys.push_back(storage_key);
      }
    }
    return storage_keys;
  }

  void AdvanceTestClockByTwoYears() {
    test_clock_.Advance(base::Days(365 * 2));
  }

  AutofillWalletMetadataSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  syncer::ClientTagBasedModelTypeProcessor* real_processor() {
    return real_processor_.get();
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  int response_version = 0;
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletMetadataSyncBridge> bridge_;
};

// The following 2 tests make sure client tags stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForAddress) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kAddr1SyncTag);
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForCard) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCard1SyncTag);
}

// The following 2 tests make sure storage keys stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForAddress) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            GetAddressStorageKey(kAddr1SpecificsId));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForCard) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            GetCardStorageKey(kCard1SpecificsId));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  table()->SetServerProfiles({CreateServerProfile(kAddr1ServerId),
                              CreateServerProfile(kAddr2ServerId)});
  table()->SetServerCreditCards({CreateServerCreditCard(kCard1ServerId),
                                 CreateServerCreditCard(kCard2ServerId)});
  ResetBridge();

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForAddress(kAddr2SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForCard(kCard1SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForCard(kCard2SpecificsId))));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       GetData_ShouldNotReturnNonexistentData) {
  ResetBridge();
  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId)}),
              IsEmpty());
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetData_ShouldReturnSelectedData) {
  table()->SetServerProfiles({CreateServerProfile(kAddr1ServerId),
                              CreateServerProfile(kAddr2ServerId)});
  table()->SetServerCreditCards({CreateServerCreditCard(kCard1ServerId),
                                 CreateServerCreditCard(kCard2ServerId)});
  ResetBridge();

  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId),
                            GetCardStorageKey(kCard1SpecificsId)}),
              UnorderedElementsAre(
                  EqualsSpecifics(CreateWalletMetadataSpecificsForAddress(
                      kAddr1SpecificsId)),
                  EqualsSpecifics(CreateWalletMetadataSpecificsForCard(
                      kCard1SpecificsId))));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetData_ShouldReturnCompleteData) {
  AutofillProfile profile = CreateServerProfile(kAddr1ServerId);
  profile.set_use_count(5);
  profile.set_use_date(UseDateFromProtoValue(2));
  profile.set_has_converted(true);
  table()->SetServerProfiles({profile});

  CreditCard card = CreateServerCreditCard(kCard1ServerId);
  card.set_use_count(6);
  card.set_use_date(UseDateFromProtoValue(3));
  card.set_billing_address_id(kAddr1ServerId);
  table()->SetServerCreditCards({card});
  ResetBridge();

  // Expect to retrieve following specifics:
  WalletMetadataSpecifics profile_specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  profile_specifics.set_use_count(5);
  profile_specifics.set_use_date(2);
  profile_specifics.set_address_has_converted(true);

  WalletMetadataSpecifics card_specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  card_specifics.set_use_count(6);
  card_specifics.set_use_date(3);
  card_specifics.set_card_billing_address_id(kAddr1SpecificsId);

  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId),
                            GetCardStorageKey(kCard1SpecificsId)}),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics)));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       ApplyStopSyncChanges_ShouldWipeLocalDataWhenSyncStopped) {
  // Perform initial sync to create sync data & metadata.
  ResetBridge(/*initial_sync_done=*/false);
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);
  StartSyncing({profile, card});

  // Now stop sync. This should wipe the data but not notify the backend (as the
  // data bridge will do that).
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  StopSyncing();

  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       ApplyStopSyncChanges_ShouldKeepLocalDataOnShutdown) {
  // Perform initial sync to create sync data & metadata.
  ResetBridge(/*initial_sync_done=*/false);
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);
  StartSyncing({profile, card});

  // Now simulate shutting down the browser. This should not touch any of the
  // data and thus also not notify the backend.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  Shutdown();

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DontSendLowerValueToServerOnUpdate) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/2, /*use_date=*/5)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/6)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/4);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/2, /*use_date=*/5);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::UPDATE,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.server_id(), &updated_card));

  // Check that also the local metadata did not get updated.
  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(
          EqualsSpecifics(CreateWalletMetadataSpecificsForAddressWithDetails(
              kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5)),
          EqualsSpecifics(CreateWalletMetadataSpecificsForCardWithDetails(
              kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6))));
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is created (tests the case when metadata with higher use
// counts arrive before the data, the data bridge later notifies about creation
// for data that is already there).
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DontSendLowerValueToServerOnCreation) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/2, /*use_date=*/5)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/6)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/4);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/2, /*use_date=*/5);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::ADD,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::ADD, updated_card.server_id(), &updated_card));

  // Check that also the local metadata did not get updated.
  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(
          EqualsSpecifics(CreateWalletMetadataSpecificsForAddressWithDetails(
              kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5)),
          EqualsSpecifics(CreateWalletMetadataSpecificsForCardWithDetails(
              kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6))));
}

// Verify that higher values of metadata are sent to the sync server when local
// metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       SendHigherValuesToServerOnLocalUpdate) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/2)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/4)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::UPDATE,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.server_id(), &updated_card));

  // Check that the local metadata got update as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off addition of metadata is sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       SendNewDataToServerOnLocalAddition) {
  ResetBridge();
  AutofillProfile new_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard new_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::ADD, new_profile.server_id(), &new_profile));
  bridge()->CreditCardChanged(
      CreditCardChange(CreditCardChange::ADD, new_card.server_id(), &new_card));

  // Check that the new metadata got created as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off addition of metadata is sent to the sync server (even
// though it is notified as an update). This tests that the bridge is robust and
// recreates metadata that may get deleted in the mean-time).
TEST_F(AutofillWalletMetadataSyncBridgeTest, SendNewDataToServerOnLocalUpdate) {
  ResetBridge();
  AutofillProfile new_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard new_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, new_profile.server_id(), &new_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, new_card.server_id(), &new_card));

  // Check that the new metadata got created as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off deletion of existing metadata is sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DeleteExistingDataFromServerOnLocalDeletion) {
  AutofillProfile existing_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard existing_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);
  table()->SetServerProfiles({existing_profile});
  table()->SetServerCreditCards({existing_card});
  ResetBridge();

  EXPECT_CALL(mock_processor(), Delete(kAddr1StorageKey, _));
  EXPECT_CALL(mock_processor(), Delete(kCard1StorageKey, _));
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::REMOVE,
                            existing_profile.server_id(), &existing_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::REMOVE, existing_card.server_id(), &existing_card));

  // Check that there is no metadata anymore.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// Verify that deletion of non-existing metadata is not sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DoNotDeleteNonExistingDataFromServerOnLocalDeletion) {
  AutofillProfile existing_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard existing_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);
  // Save only data and not metadata.
  table()->SetServerAddressesData({existing_profile});
  table()->SetServerCardsData({existing_card});
  ResetBridge();

  // Check that there is no metadata, from start on.
  ASSERT_THAT(GetAllLocalDataInclRestart(), IsEmpty());

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::REMOVE,
                            existing_profile.server_id(), &existing_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::REMOVE, existing_card.server_id(), &existing_card));

  // Check that there is also no metadata at the end.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// Verify that updates of local (non-sync) addresses are ignored.
TEST_F(AutofillWalletMetadataSyncBridgeTest, DoNotPropagateNonSyncAddresses) {
  // Add local data.
  AutofillProfile existing_profile =
      CreateLocalProfileWithDetails(/*use_count=*/10, /*use_date=*/20);
  table()->AddAutofillProfile(existing_profile);
  ResetBridge();

  // Check that there is no metadata, from start on.
  ASSERT_THAT(GetAllLocalDataInclRestart(), IsEmpty());

  EXPECT_CALL(mock_processor(), Put).Times(0);
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  existing_profile.set_use_count(11);
  existing_profile.set_use_date(UseDateFromProtoValue(21));
  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::UPDATE,
                            existing_profile.guid(), &existing_profile));

  // Check that there is also no metadata at the end.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// Verify that updates of local (non-sync) credit cards are ignored.
// Regression test for crbug.com/1206306.
TEST_F(AutofillWalletMetadataSyncBridgeTest, DoNotPropagateNonSyncCards) {
  // Local credit cards need crypto for storage.
  OSCryptMocker::SetUp();

  // Add local data.
  CreditCard existing_card =
      CreateLocalCreditCardWithDetails(/*use_count=*/30, /*use_date=*/40);
  table()->AddCreditCard(existing_card);
  ResetBridge();

  // Check that there is no metadata, from start on.
  ASSERT_THAT(GetAllLocalDataInclRestart(), IsEmpty());

  EXPECT_CALL(mock_processor(), Put).Times(0);
  // Local changes should not cause local DB writes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  existing_card.set_use_count(31);
  existing_card.set_use_date(UseDateFromProtoValue(41));
  bridge()->CreditCardChanged(CreditCardChange(
      AutofillProfileChange::UPDATE, existing_card.guid(), &existing_card));

  // Check that there is also no metadata at the end.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());

  OSCryptMocker::TearDown();
}

// Verify that old orphan metadata gets deleted on startup.
TEST_F(AutofillWalletMetadataSyncBridgeTest, DeleteOldOrphanMetadataOnStartup) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  // Save only metadata and not data - simulate an orphan.
  table()->AddServerAddressMetadata(
      CreateServerProfileFromSpecifics(profile).GetMetadata());
  table()->AddServerCardMetadata(
      CreateServerCreditCardFromSpecifics(card).GetMetadata());

  // Make the orphans old by advancing time.
  AdvanceTestClockByTwoYears();

  EXPECT_CALL(mock_processor(), Delete(kAddr1StorageKey, _));
  EXPECT_CALL(mock_processor(), Delete(kCard1StorageKey, _));
  EXPECT_CALL(*backend(), CommitChanges());

  ResetBridge();

  ASSERT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// Verify that recent orphan metadata does not get deleted on startup.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DoNotDeleteOldNonOrphanMetadataOnStartup) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  // Save both data and metadata - these are not orphans.
  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});

  // Make the entities old by advancing time.
  AdvanceTestClockByTwoYears();

  // Since the entities are non-oprhans, they should not get deleted.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  ResetBridge();

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Verify that recent orphan metadata does not get deleted on startup.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DoNotDeleteRecentOrphanMetadataOnStartup) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  // Save only metadata and not data - simulate an orphan.
  table()->AddServerAddressMetadata(
      CreateServerProfileFromSpecifics(profile).GetMetadata());
  table()->AddServerCardMetadata(
      CreateServerCreditCardFromSpecifics(card).GetMetadata());

  // We do not advance time so the orphans are recent, should not get deleted.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  ResetBridge();

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Test that both local cards and local profiles that are not in the remote data
// set are uploaded during initial sync. This should rarely happen in practice
// because we wipe local data when disabling sync. Still there are corner cases
// such as when PDM manages to change metadata before the metadata bridge
// performs initial sync.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       InitialSync_UploadUniqueLocalData) {
  WalletMetadataSpecifics preexisting_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics preexisting_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  table()->SetServerProfiles(
      {CreateServerProfileFromSpecifics(preexisting_profile)});
  table()->SetServerCreditCards(
      {CreateServerCreditCardFromSpecifics(preexisting_card)});

  // Have different entities on the server.
  WalletMetadataSpecifics remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr2SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard2SpecificsId, /*use_count=*/30, /*use_date=*/40);

  // The bridge should upload the unique local entities and store the remote
  // ones locally.
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(preexisting_profile), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(preexisting_card), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ResetBridge(/*initial_sync_done=*/false);
  StartSyncing({remote_profile, remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(preexisting_profile),
                                   EqualsSpecifics(preexisting_card),
                                   EqualsSpecifics(remote_profile),
                                   EqualsSpecifics(remote_card)));
}

// Test that the initial sync correctly distinguishes data that is unique in the
// local data set from data that is both in the local data and in the remote
// data. We should only upload the local data. This should rarely happen in
// practice because we wipe local data when disabling sync. Still there are
// corner cases such as when PDM manages to change metadata before the metadata
// bridge performs initial sync.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       InitialSync_UploadOnlyUniqueLocalData) {
  WalletMetadataSpecifics preexisting_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics preexisting_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  table()->SetServerProfiles(
      {CreateServerProfileFromSpecifics(preexisting_profile)});
  table()->SetServerCreditCards(
      {CreateServerCreditCardFromSpecifics(preexisting_card)});

  // The remote profile has the same id as local profile, only is newer.
  WalletMetadataSpecifics remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/15, /*use_date=*/25);
  WalletMetadataSpecifics remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard2SpecificsId, /*use_count=*/30, /*use_date=*/40);

  // Upload _only_ the unique local data, only the card.
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(preexisting_card), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ResetBridge(/*initial_sync_done=*/false);
  StartSyncing({remote_profile, remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(preexisting_card),
                                   EqualsSpecifics(remote_profile),
                                   EqualsSpecifics(remote_card)));
}

// Test that remote deletions are ignored.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       RemoteDeletion_ShouldNotDeleteExistingLocalData) {
  // Perform initial sync to create sync data & metadata.
  ResetBridge(/*initial_sync_done=*/false);
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);
  StartSyncing({profile, card});

  // Verify that both the processor and the local DB contain sync metadata.
  ASSERT_TRUE(real_processor()->IsTrackingEntityForTest(kAddr1StorageKey));
  ASSERT_TRUE(real_processor()->IsTrackingEntityForTest(kCard1StorageKey));
  ASSERT_THAT(GetLocalSyncMetadataStorageKeys(),
              UnorderedElementsAre(kAddr1StorageKey, kCard1StorageKey));

  // Now delete the profile.
  // We still need to commit the updated progress marker and sync metadata.
  EXPECT_CALL(*backend(), CommitChanges());
  // Changes should _not_ happen in the local autofill database.
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  ReceiveTombstones({profile, card});

  // Verify that even though the processor does not track these entities any
  // more and the sync metadata is gone, the actual data entities still exist in
  // the local DB.
  EXPECT_FALSE(real_processor()->IsTrackingEntityForTest(kAddr1StorageKey));
  EXPECT_FALSE(real_processor()->IsTrackingEntityForTest(kCard1StorageKey));
  EXPECT_THAT(GetLocalSyncMetadataStorageKeys(), IsEmpty());
  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

enum RemoteChangesMode {
  INITIAL_SYNC_ADD,  // Initial sync -> ADD changes.
  LATER_SYNC_ADD,    // Later sync; the client receives the data for the first
                     // time -> ADD changes.
  LATER_SYNC_UPDATE  // Later sync; the client has received the data before ->
                     // UPDATE changes.
};

// Parametrized fixture for tests that apply in the same way for all
// RemoteChangesModes.
class AutofillWalletMetadataSyncBridgeRemoteChangesTest
    : public testing::WithParamInterface<RemoteChangesMode>,
      public AutofillWalletMetadataSyncBridgeTest {
 public:
  AutofillWalletMetadataSyncBridgeRemoteChangesTest() {}

  AutofillWalletMetadataSyncBridgeRemoteChangesTest(
      const AutofillWalletMetadataSyncBridgeRemoteChangesTest&) = delete;
  AutofillWalletMetadataSyncBridgeRemoteChangesTest& operator=(
      const AutofillWalletMetadataSyncBridgeRemoteChangesTest&) = delete;

  ~AutofillWalletMetadataSyncBridgeRemoteChangesTest() override {}

  void ResetBridgeWithPotentialInitialSync(
      const std::vector<WalletMetadataSpecifics>& remote_data) {
    ResetBridge(/*initial_sync_done=*/GetParam() != INITIAL_SYNC_ADD);

    if (GetParam() == LATER_SYNC_UPDATE) {
      StartSyncing(remote_data);
    }
  }

  void ReceivePotentiallyInitialUpdates(
      const std::vector<WalletMetadataSpecifics>& remote_data) {
    if (GetParam() != LATER_SYNC_UPDATE) {
      StartSyncing(remote_data);
    } else {
      ReceiveUpdates(remote_data);
    }
  }
};

// No upstream communication or local DB change happens if the server sends an
// empty update.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest, EmptyUpdateIgnored) {
  ResetBridgeWithPotentialInitialSync({});

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({});

  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// No upstream communication or local DB change happens if the server sends the
// same data as we have locally.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest, SameDataIgnored) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({profile, card});

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Tests that if the remote use stats are higher / newer, they should win over
// local stats.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_RemoteWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile),
                                   EqualsSpecifics(updated_remote_card)));
}

// Tests that if the local use stats are higher / newer, they should win over
// remote stats.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_LocalWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(profile), _));
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Tests that the conflicts are resolved component-wise (a higher use_count is
// taken from local data, a newer use_data is taken from remote data).
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_BothWin1) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/50);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile),
                                   EqualsSpecifics(merged_card)));
}

// Tests that the conflicts are resolved component-wise, like the previous test,
// only the other way around (a higher use_count is taken from remote data, a
// newer use_data is taken from local data).
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_BothWin2) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile),
                                   EqualsSpecifics(merged_card)));
}

// No merge logic is applied if local data has initial use_count (=1). In this
// situation, we just take over the remote entity completely.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferRemoteIfLocalHasInitialUseCount) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/1, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile),
                                   EqualsSpecifics(updated_remote_card)));
}

// Tests that with a conflict in billing_address_id, we prefer an ID of a local
// profile over an ID of a server profile. In this test, the preferred ID is in
// the remote update that we need to store locally.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferLocalBillingAddressId_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that with a conflict in billing_address_id, we prefer an ID of a local
// profile over an ID of a server profile. In this test, the preferred ID is in
// the local data that we need to upstream.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferLocalBillingAddressId_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that if both addresses have billing address ids of local profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// remote entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfLocalIds_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kLocalAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that if both addresses have billing address ids of local profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// local entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfLocalIds_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that if both addresses have billing address ids of server profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// remote entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that if both addresses have billing address ids of server profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// local entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that the conflict resolution happens component-wise. To avoid
// combinatorial explosion, this only tests when both have billing address ids
// of server profiles, one entity is more recently used but the other entity has
// a higher use_count. We should pick the billing_address_id of the newer one
// but have the use_count updated to the maximum as well. In this test, the
// remote entity is the more recently used.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_BothWin1) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_card)));
}

// Tests that the conflict resolution happens component-wise. To avoid
// combinatorial explosion, this only tests when both have billing address ids
// of server profiles, one entity is more recently used but the other entity has
// a higher use_count. We should pick the billing_address_id of the newer one
// but have the use_count updated to the maximum as well. In this test, the
// local entity is the more recently used.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_BothWin2) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6,
          /*billing_address_id=*/kAddr2ServerId);

  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_card)));
}

// Tests that if the has_converted bit differs, we always end up with the true
// value. This test has the remote entity converted which should get updated in
// the local DB.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_RemoteWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/false);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile)));
}

// Tests that if the has_converted bit differs, we always end up with the true
// value. This test has the local entity converted which should get upstreamed.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_LocalWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/false);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(profile), _));
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker.
  EXPECT_CALL(*backend(), CommitChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(profile)));
}

// Tests that the conflict resolution happens component-wise. If one entity
// has_converted but the other entity has higher use_count, we should end up
// with an entity that has_converted and has the higher use_count. This test has
// the remote entity converted.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_BothWin1) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/false);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile)));
}

// Tests that the conflict resolution happens component-wise. If one entity
// has_converted but the other entity has higher use_count, we should end up
// with an entity that has_converted and has the higher use_count. This test has
// the local entity converted.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_BothWin2) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/false);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillWalletMetadataSyncBridgeRemoteChangesTest,
                         ::testing::Values(INITIAL_SYNC_ADD,
                                           LATER_SYNC_ADD,
                                           LATER_SYNC_UPDATE));

}  // namespace autofill

namespace sync_pb {

// Makes the GMock matchers print out a readable version of the protobuf.
void PrintTo(const WalletMetadataSpecifics& specifics, std::ostream* os) {
  *os << autofill::WalletMetadataSpecificsAsDebugString(specifics);
}

}  // namespace sync_pb
