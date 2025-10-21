// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_metadata_sync_bridge.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::test::EqualsProto;
using testing::Return;

class ValuableMetadataSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&entity_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"),
             &encryptor_);
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));

    bridge_ = std::make_unique<ValuableMetadataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  ValuableMetadataSyncBridge& bridge() { return *bridge_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  const os_crypt_async::Encryptor encryptor_ =
      os_crypt_async::GetTestEncryptorForTesting();
  EntityTable entity_table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  WebDatabase db_;
  syncer::MockDataTypeLocalChangeProcessor mock_processor_;
  std::unique_ptr<ValuableMetadataSyncBridge> bridge_;
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncAutofillValuableMetadata};
};

// Tests that IsEntityDataValid() returns true for valid entity data.
TEST_F(ValuableMetadataSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData entity_data;
  sync_pb::AutofillValuableMetadataSpecifics* specifics =
      entity_data.specifics.mutable_autofill_valuable_metadata();
  specifics->set_valuable_id("some_id");
  EXPECT_TRUE(bridge().IsEntityDataValid(entity_data));
}

// Tests that IsEntityDataValid() returns false for entity data with an empty
// valuable_id.
TEST_F(ValuableMetadataSyncBridgeTest, IsEntityDataValid_EmptyValuableId) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_autofill_valuable_metadata();
  EXPECT_FALSE(bridge().IsEntityDataValid(entity_data));
}

// Tests that GetClientTag() and GetStorageKey() return the correct valuable_id.
TEST_F(ValuableMetadataSyncBridgeTest, GetClientTagAndStorageKey) {
  syncer::EntityData entity_data;
  sync_pb::AutofillValuableMetadataSpecifics* specifics =
      entity_data.specifics.mutable_autofill_valuable_metadata();
  specifics->set_valuable_id("some_id");

  EXPECT_EQ(bridge().GetClientTag(entity_data), "some_id");
  EXPECT_EQ(bridge().GetStorageKey(entity_data), "some_id");
}

// Tests that TrimAllSupportedFieldsFromRemoteSpecifics() correctly trims
// all supported fields from the specifics.
TEST_F(ValuableMetadataSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillValuableMetadataSpecifics* metadata =
      entity_specifics.mutable_autofill_valuable_metadata();
  metadata->set_valuable_id("some_id");
  metadata->set_use_count(5);
  metadata->set_last_used_date_unix_epoch_micros(12345);
  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics)
                .ByteSizeLong(),
            0u);
}
// Test that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(ValuableMetadataSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics_PreserveUnsupportedFields) {
  sync_pb::EntitySpecifics trimmed_entity_specifics;
  sync_pb::AutofillValuableMetadataSpecifics*
      valuable_metadata_specifics_with_only_unknown_fields =
          trimmed_entity_specifics.mutable_autofill_valuable_metadata();

  // Set an unsupported field in the top-level message.
  *valuable_metadata_specifics_with_only_unknown_fields
       ->mutable_unknown_fields() = "unsupported_fields";

  // Create a copy and set a value to the same nested message that already
  // contains an unsupported field.
  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_autofill_valuable_metadata() =
      *valuable_metadata_specifics_with_only_unknown_fields;
  entity_specifics.mutable_autofill_valuable_metadata()->set_valuable_id(
      "some_id");

  EXPECT_THAT(
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics),
      EqualsProto(trimmed_entity_specifics));
}

}  // namespace
}  // namespace autofill
