// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_usage_data_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
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
const char kDefaultCacheGuid[] = "CacheGuid";
const std::string kExpectedClientTagAndStorageKey =
    "VirtualCardUsageData|12345|https://www.google.com|google";

}  // namespace

class AutofillWalletUsageDataSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletUsageDataSyncBridgeTest() = default;
  ~AutofillWalletUsageDataSyncBridgeTest() override = default;
  AutofillWalletUsageDataSyncBridgeTest(
      const AutofillWalletUsageDataSyncBridgeTest&) = delete;
  AutofillWalletUsageDataSyncBridgeTest& operator=(
      const AutofillWalletUsageDataSyncBridgeTest&) = delete;

  void SetUp() override {
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    // Fake that initial sync has been done (so that the bridge immediately
    // records metrics).
    ResetBridge(/*initial_sync_done=*/true);
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_USAGE, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_USAGE));
    model_type_state.set_cache_guid(kDefaultCacheGuid);
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_USAGE,
                                              model_type_state));
    bridge_ = std::make_unique<AutofillWalletUsageDataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  EntityData SpecificsToEntity(const AutofillWalletUsageSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet_usage() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_USAGE, bridge()->GetClientTag(data));
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
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletUsageDataSyncBridge> bridge_;
};

TEST_F(AutofillWalletUsageDataSyncBridgeTest, VerifyGetClientTag) {
  AutofillWalletUsageSpecifics specifics;
  AutofillWalletUsageData data =
      test::GetAutofillWalletUsageDataForVirtualCard();
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(data, &specifics);

  sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData
      virtual_card_usage_data = specifics.virtual_card_usage_data();

  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kExpectedClientTagAndStorageKey);
}

TEST_F(AutofillWalletUsageDataSyncBridgeTest, VerifyGetStorageKey) {
  AutofillWalletUsageSpecifics specifics;
  AutofillWalletUsageData data =
      test::GetAutofillWalletUsageDataForVirtualCard();
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(data, &specifics);

  sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData
      virtual_card_usage_data = specifics.virtual_card_usage_data();

  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            kExpectedClientTagAndStorageKey);
}

}  // namespace autofill
