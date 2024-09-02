// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_usage_data_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::AutofillWalletUsageSpecifics;
using syncer::EntityData;
using syncer::MockDataTypeLocalChangeProcessor;
using testing::NiceMock;
using testing::Return;

const std::string kExpectedClientTagAndStorageKey =
    "VirtualCardUsageData|12345|google|https://www.google.com";

std::vector<VirtualCardUsageData> ExtractVirtualCardUsageDataFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<VirtualCardUsageData> usage_data;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    usage_data.push_back(VirtualCardUsageDataFromUsageSpecifics(
        data_pair.second->specifics.autofill_wallet_usage()));
  }
  return usage_data;
}

class AutofillWalletUsageDataSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    bridge_ = std::make_unique<AutofillWalletUsageDataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  std::vector<VirtualCardUsageData> GetVirtualCardUsageDataFromTable() {
    std::vector<VirtualCardUsageData> table_data;
    table()->GetAllVirtualCardUsageData(table_data);
    return table_data;
  }

  EntityData SpecificsToEntity(const AutofillWalletUsageSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet_usage() = specifics;
    return data;
  }

  EntityData VirtualCardUsageDataToEntity(
      const VirtualCardUsageData virtual_card_usage_data) {
    AutofillWalletUsageSpecifics specifics;
    AutofillWalletUsageData usage_data =
        AutofillWalletUsageData::ForVirtualCard(virtual_card_usage_data);
    SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(usage_data,
                                                               &specifics);
    return SpecificsToEntity(specifics);
  }

  PaymentsAutofillTable* table() { return &table_; }

  AutofillWalletUsageDataSyncBridge* bridge() { return bridge_.get(); }

  MockAutofillWebDataBackend& backend() { return backend_; }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 private:
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillSyncMetadataTable sync_metadata_table_;
  PaymentsAutofillTable table_;
  WebDatabase db_;
  NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<AutofillWalletUsageDataSyncBridge> bridge_;
};

TEST_F(AutofillWalletUsageDataSyncBridgeTest, VerifyGetClientTag) {
  AutofillWalletUsageSpecifics specifics;
  AutofillWalletUsageData data =
      AutofillWalletUsageData::ForVirtualCard(test::GetVirtualCardUsageData1());
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(data, &specifics);

  sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData
      virtual_card_usage_data = specifics.virtual_card_usage_data();

  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kExpectedClientTagAndStorageKey);
}

TEST_F(AutofillWalletUsageDataSyncBridgeTest, VerifyGetStorageKey) {
  AutofillWalletUsageSpecifics specifics;
  AutofillWalletUsageData data =
      AutofillWalletUsageData::ForVirtualCard(test::GetVirtualCardUsageData1());
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(data, &specifics);

  sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData
      virtual_card_usage_data = specifics.virtual_card_usage_data();

  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            kExpectedClientTagAndStorageKey);
}

// Tests that `GetDataForCommit()` returns all local usage data of matching
// usage data id.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, GetDataForCommit) {
  const VirtualCardUsageData usage_data1 = test::GetVirtualCardUsageData1();
  const VirtualCardUsageData usage_data2 = test::GetVirtualCardUsageData2();
  table()->SetVirtualCardUsageData({usage_data1, usage_data2});

  // Get data the data of `usage_data_1`.
  std::vector<VirtualCardUsageData> virtual_card_usage_data =
      ExtractVirtualCardUsageDataFromDataBatch(
          bridge()->GetDataForCommit({*usage_data1.usage_data_id()}));
  EXPECT_THAT(virtual_card_usage_data, testing::ElementsAre(usage_data1));
}

// Tests that `GetAllDataForDebugging()` returns all local usage data.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, GetAllDataForDebugging) {
  const VirtualCardUsageData usage_data1 = test::GetVirtualCardUsageData1();
  const VirtualCardUsageData usage_data2 = test::GetVirtualCardUsageData2();
  table()->SetVirtualCardUsageData({usage_data1, usage_data2});

  std::vector<VirtualCardUsageData> virtual_card_usage_data =
      ExtractVirtualCardUsageDataFromDataBatch(
          bridge()->GetAllDataForDebugging());
  EXPECT_THAT(virtual_card_usage_data,
              testing::UnorderedElementsAre(usage_data1, usage_data2));
}

// Tests that when sync changes are applied, `ApplyIncrementalSyncChanges()`
// merges remotes changes into the local store.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, ApplyIncrementalSyncChanges) {
  VirtualCardUsageData virtual_card_usage_data1 =
      test::GetVirtualCardUsageData1();
  VirtualCardUsageData virtual_card_usage_data2 =
      test::GetVirtualCardUsageData2();

  // Add `virtual_card_usage_data1`.
  syncer::EntityChangeList entity_change_list_merge;
  entity_change_list_merge.push_back(syncer::EntityChange::CreateAdd(
      *virtual_card_usage_data1.usage_data_id(),
      VirtualCardUsageDataToEntity(virtual_card_usage_data1)));

  // `MergeFullSyncData()` returns an error if it fails.
  EXPECT_EQ(bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                                        std::move(entity_change_list_merge)),
            std::nullopt);
  // Expect `MergeFullSyncData()` was successful.
  EXPECT_THAT(GetVirtualCardUsageDataFromTable(),
              testing::UnorderedElementsAre(virtual_card_usage_data1));

  // Delete the existing `virtual_card_usage_data1` and add
  // `virtual_card_usage_data2`.
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(
      *virtual_card_usage_data1.usage_data_id()));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *virtual_card_usage_data2.usage_data_id(),
      VirtualCardUsageDataToEntity(virtual_card_usage_data2)));

  // Expect no changes to the remote usage data.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_USAGE));

  // `ApplyIncrementalSyncChanges()` returns an error if it fails.
  EXPECT_FALSE(bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list)));

  // Expect that the local data has changed.
  EXPECT_THAT(GetVirtualCardUsageDataFromTable(),
              testing::UnorderedElementsAre(virtual_card_usage_data2));

  // Update 'virtual_card_usage_data2'.
  syncer::EntityChangeList entity_change_list_update;
  VirtualCardUsageData virtual_card_usage_data2_update =
      VirtualCardUsageData(virtual_card_usage_data2.usage_data_id(),
                           virtual_card_usage_data2.instrument_id(),
                           VirtualCardUsageData::VirtualCardLastFour(u"5555"),
                           virtual_card_usage_data2.merchant_origin());
  entity_change_list_update.push_back(syncer::EntityChange::CreateUpdate(
      *virtual_card_usage_data2_update.usage_data_id(),
      VirtualCardUsageDataToEntity(virtual_card_usage_data2_update)));

  // Expect no changes to the remote usage data.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_USAGE));

  // `ApplyIncrementalSyncChanges()` returns an error if it fails.
  EXPECT_FALSE(bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      std::move(entity_change_list_update)));

  // Expect that the local data has changed.
  EXPECT_THAT(GetVirtualCardUsageDataFromTable(),
              testing::UnorderedElementsAre(virtual_card_usage_data2_update));
}

// Tests that when sync is stopped and the data type is disabled, client should
// remove all client data.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, ApplyDisableSyncChanges) {
  // Create a virtual card usage data in the client table.
  VirtualCardUsageData old_data = test::GetVirtualCardUsageData1();
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *old_data.usage_data_id(), VirtualCardUsageDataToEntity(old_data)));
  EXPECT_EQ(bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                                        std::move(entity_change_list)),
            std::nullopt);

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_USAGE));

  bridge()->ApplyDisableSyncChanges(/*delete_metadata_change_list=*/
                                    bridge()->CreateMetadataChangeList());

  EXPECT_TRUE(GetVirtualCardUsageDataFromTable().empty());
}

// Test to ensure whether the data being valid is logged correctly.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, ApplySyncData_LogDataValidity) {
  VirtualCardUsageData virtual_card_usage_data1 =
      test::GetVirtualCardUsageData1();

  // AutofillWalletUsageSpecifics with missing fields.
  AutofillWalletUsageSpecifics specifics;
  specifics.set_guid("guid");
  specifics.mutable_virtual_card_usage_data()->set_instrument_id(1234);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *virtual_card_usage_data1.usage_data_id(),
      VirtualCardUsageDataToEntity(virtual_card_usage_data1)));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      specifics.guid(), SpecificsToEntity(specifics)));

  EXPECT_CALL(backend(), CommitChanges());
  base::HistogramTester histogram_tester;
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(entity_change_list));
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardUsageData.SyncedUsageDataBeingValid", true, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardUsageData.SyncedUsageDataBeingValid", false, 1);
}

}  // namespace
}  // namespace autofill
