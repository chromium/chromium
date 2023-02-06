// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_usage_data_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::ScopedTempDir;
using sync_pb::AutofillWalletUsageSpecifics;
using sync_pb::ModelTypeState;
using syncer::EntityData;
using syncer::MockModelTypeChangeProcessor;
using testing::NiceMock;
using testing::Return;

const char kLocaleString[] = "en-US";
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

}  // namespace

class AutofillWalletUsageDataSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    bridge_ = std::make_unique<AutofillWalletUsageDataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  EntityData SpecificsToEntity(const AutofillWalletUsageSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet_usage() = specifics;
    return data;
  }

  AutofillTable* table() { return &table_; }

  AutofillWalletUsageDataSyncBridge* bridge() { return bridge_.get(); }

 private:
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  NiceMock<MockModelTypeChangeProcessor> mock_processor_;
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

// Tests that `GetData()` returns all local usage data of matching usage data
// id.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, GetData) {
  const VirtualCardUsageData usage_data1 = test::GetVirtualCardUsageData1();
  const VirtualCardUsageData usage_data2 = test::GetVirtualCardUsageData2();
  table()->SetVirtualCardUsageData({usage_data1, usage_data2});

  // Synchronously get data the data of `usage_data_1`.
  std::vector<VirtualCardUsageData> virtual_card_usage_data;
  base::RunLoop loop;
  bridge()->GetData(
      {*usage_data1.usage_data_id()},
      base::BindLambdaForTesting([&](std::unique_ptr<syncer::DataBatch> batch) {
        virtual_card_usage_data =
            ExtractVirtualCardUsageDataFromDataBatch(std::move(batch));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_THAT(virtual_card_usage_data, testing::ElementsAre(usage_data1));
}

// Tests that `GetAllDataForDebugging()` returns all local usage data.
TEST_F(AutofillWalletUsageDataSyncBridgeTest, GetAllDataForDebugging) {
  const VirtualCardUsageData usage_data1 = test::GetVirtualCardUsageData1();
  const VirtualCardUsageData usage_data2 = test::GetVirtualCardUsageData2();
  table()->SetVirtualCardUsageData({usage_data1, usage_data2});

  std::vector<VirtualCardUsageData> virtual_card_usage_data;
  base::RunLoop loop;
  bridge()->GetAllDataForDebugging(
      base::BindLambdaForTesting([&](std::unique_ptr<syncer::DataBatch> batch) {
        virtual_card_usage_data =
            ExtractVirtualCardUsageDataFromDataBatch(std::move(batch));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_THAT(virtual_card_usage_data,
              testing::UnorderedElementsAre(usage_data1, usage_data2));
}

}  // namespace autofill
