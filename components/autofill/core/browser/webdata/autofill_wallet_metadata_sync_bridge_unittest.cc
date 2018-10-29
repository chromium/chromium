// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::WalletMetadataSpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::ElementsAre;
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

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "address-YWRkcjHvv74=";
const char kCard1SyncTag[] = "card-Y2FyZDHvv74=";

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

base::Time UseDateFromProtoValue(int64_t use_date_proto_value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(use_date_proto_value));
}

int64_t UseDateToProtoValue(base::Time use_date) {
  return use_date.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddressWithUseStats(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::ADDRESS);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // Set the default value according to the constructor of AutofillProfile.
  specifics.set_address_has_converted(false);
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddress(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForAddressWithUseStats(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kJune2017));
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCardWithUseStats(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::CARD);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // Set the default value according to the constructor of AutofillProfile.
  specifics.set_card_billing_address_id("");
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCard(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForCardWithUseStats(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kJune2017));
}

AutofillProfile CreateServerProfileWithUseStats(const std::string& server_id,
                                                size_t use_count,
                                                int64_t use_date) {
  AutofillProfile profile = CreateServerProfile(server_id);
  profile.set_use_count(use_count);
  profile.set_use_date(UseDateFromProtoValue(use_date));
  return profile;
}

CreditCard CreateServerCreditCardWithUseStats(const std::string& server_id,
                                              size_t use_count,
                                              int64_t use_date) {
  CreditCard card = CreateServerCreditCard(server_id);
  card.set_use_count(use_count);
  card.set_use_date(UseDateFromProtoValue(use_date));
  return card;
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
         << ", use_date: " << UseDateFromProtoValue(specifics.use_date())
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
  ~AutofillWalletMetadataSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
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
            syncer::AUTOFILL_WALLET_METADATA, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_.reset(new AutofillWalletMetadataSyncBridge(
        mock_processor_.CreateForwardingProcessor(), &backend_));
  }

  EntityData SpecificsToEntity(const WalletMetadataSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_wallet_metadata() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_WALLET_METADATA, bridge()->GetClientTag(data));
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

  AutofillWalletMetadataSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletMetadataSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridgeTest);
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
            kAddr1SpecificsId);
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForCard) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            kCard1SpecificsId);
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
  EXPECT_THAT(GetLocalData({kAddr1SpecificsId}), IsEmpty());
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetData_ShouldReturnSelectedData) {
  table()->SetServerProfiles({CreateServerProfile(kAddr1ServerId),
                              CreateServerProfile(kAddr2ServerId)});
  table()->SetServerCreditCards({CreateServerCreditCard(kCard1ServerId),
                                 CreateServerCreditCard(kCard2ServerId)});
  ResetBridge();

  EXPECT_THAT(GetLocalData({kAddr1SpecificsId, kCard1SpecificsId}),
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

  EXPECT_THAT(GetLocalData({kAddr1SpecificsId, kCard1SpecificsId}),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics)));
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DontSendLowerValueToServerOnSingleChange) {
  table()->SetServerProfiles({CreateServerProfileWithUseStats(
      kAddr1ServerId, /*use_count=*/2, /*use_date=*/5)});
  table()->SetServerCreditCards({CreateServerCreditCardWithUseStats(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/6)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithUseStats(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/4);
  CreditCard updated_card = CreateServerCreditCardWithUseStats(
      kCard1ServerId, /*use_count=*/2, /*use_date=*/5);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, updated_profile.guid(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.guid(), &updated_card));
}

// Verify that one-off addition of metadata is not sent to the sync
// server. Metadata add and delete trigger multiple changes notification
// instead.
TEST_F(AutofillWalletMetadataSyncBridgeTest, DontAddToServerOnSingleChange) {
  table()->SetServerProfiles({CreateServerProfileWithUseStats(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/2)});
  table()->SetServerCreditCards({CreateServerCreditCardWithUseStats(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/4)});
  ResetBridge();

  AutofillProfile new_profile = CreateServerProfileWithUseStats(
      kAddr2ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard new_card = CreateServerCreditCardWithUseStats(
      kCard2ServerId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, new_profile.guid(), &new_profile));
  bridge()->CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, new_card.guid(), &new_card));
}

// Verify that higher values of metadata are sent to the sync server when local
// metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       SendHigherValuesToServerOnLocalSingleChange) {
  table()->SetServerProfiles({CreateServerProfileWithUseStats(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/2)});
  table()->SetServerCreditCards({CreateServerCreditCardWithUseStats(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/4)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithUseStats(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard updated_card = CreateServerCreditCardWithUseStats(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithUseStats(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithUseStats(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1SpecificsId, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1SpecificsId, HasSpecifics(expected_card_specifics), _));

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, updated_profile.guid(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.guid(), &updated_card));
}

}  // namespace autofill
