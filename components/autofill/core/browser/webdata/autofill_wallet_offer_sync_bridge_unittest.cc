// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::ScopedTempDir;
using sync_pb::AutofillOfferSpecifics;
using sync_pb::ModelTypeState;
using syncer::EntityData;
using syncer::MockModelTypeChangeProcessor;
using testing::NiceMock;
using testing::Return;

const char kLocaleString[] = "en-US";
const char kDefaultCacheGuid[] = "CacheGuid";
}  // namespace

class AutofillWalletOfferSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletOfferSyncBridgeTest() = default;
  ~AutofillWalletOfferSyncBridgeTest() override = default;
  AutofillWalletOfferSyncBridgeTest(const AutofillWalletOfferSyncBridgeTest&) =
      delete;
  AutofillWalletOfferSyncBridgeTest& operator=(
      const AutofillWalletOfferSyncBridgeTest&) = delete;

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
            syncer::AUTOFILL_WALLET_OFFER, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_OFFER));
    model_type_state.set_cache_guid(kDefaultCacheGuid);
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_OFFER,
                                              model_type_state));
    bridge_ = std::make_unique<AutofillWalletOfferSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  EntityData SpecificsToEntity(const AutofillOfferSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_offer() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_OFFER, bridge()->GetClientTag(data));
    return data;
  }

  AutofillTable* table() { return &table_; }

  AutofillWalletOfferSyncBridge* bridge() { return bridge_.get(); }

 private:
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletOfferSyncBridge> bridge_;
};

TEST_F(AutofillWalletOfferSyncBridgeTest, VerifyGetClientTag) {
  AutofillOfferSpecifics specifics;
  AutofillOfferData data = test::GetCardLinkedOfferData();
  SetAutofillOfferSpecificsFromOfferData(data, &specifics);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            base::NumberToString(data.offer_id));
}

TEST_F(AutofillWalletOfferSyncBridgeTest, VerifyGetStorageKey) {
  AutofillOfferSpecifics specifics;
  AutofillOfferData data = test::GetCardLinkedOfferData();
  SetAutofillOfferSpecificsFromOfferData(data, &specifics);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            base::NumberToString(data.offer_id));
}

}  // namespace autofill
