// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/journey_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/mock_data_type_store.h"
#include "components/sync/test/unknown_field_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {
namespace {

using base::test::EqualsProto;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;

constexpr char kTestJourneyId[] = "test_guid";
constexpr char kTestTitle[] = "test title";
constexpr char kTestEmoji[] = "test emoji";

sync_pb::JourneySpecifics CreateTestJourneySpecifics(
    const std::string& journey_id = kTestJourneyId) {
  sync_pb::JourneySpecifics specifics;
  specifics.set_journey_id(journey_id);
  specifics.set_title(kTestTitle);
  specifics.set_emoji(kTestEmoji);
  specifics.set_creation_time_windows_epoch_micros(100);
  return specifics;
}

syncer::EntityData CreateTestJourneyEntityData(
    const std::string& id = kTestJourneyId) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_journey() = CreateTestJourneySpecifics(id);
  entity_data.name = id;
  return entity_data;
}

class JourneysSyncBridgeTest : public testing::Test {
 public:
  JourneysSyncBridgeTest() = default;
  ~JourneysSyncBridgeTest() override = default;

  JourneysSyncBridge CreateBridge() {
    return JourneysSyncBridge(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

TEST_F(JourneysSyncBridgeTest, ModelReadyToSyncOnInitialization) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_processor_, ModelReadyToSync(_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  JourneysSyncBridge bridge = CreateBridge();
  run_loop.Run();
}

TEST_F(JourneysSyncBridgeTest, ReportErrorOnStoreCreationFailure) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_processor_, ReportError(_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  JourneysSyncBridge bridge(
      mock_processor_.CreateForwardingProcessor(),
      base::BindOnce([](syncer::DataType type,
                        syncer::DataTypeStore::InitCallback callback) {
        std::move(callback).Run(
            syncer::ModelError(
                FROM_HERE,
                syncer::ModelError::Type::kDataTypeStoreBackendDbOpenFailed),
            /*store=*/nullptr);
      }));
  run_loop.Run();
}

TEST_F(JourneysSyncBridgeTest, ReportErrorOnStoreReadAllDataFailure) {
  base::RunLoop run_loop;
  auto mock_store = std::make_unique<NiceMock<syncer::MockDataTypeStore>>();
  EXPECT_CALL(*mock_store, ReadAllData(_))
      .WillOnce([](syncer::DataTypeStore::ReadAllDataCallback callback) {
        std::move(callback).Run(
            syncer::ModelError(
                FROM_HERE,
                syncer::ModelError::Type::kDataTypeStoreBackendDbReadFailed),
            /*records=*/nullptr);
      });

  EXPECT_CALL(mock_processor_, ReportError(_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  JourneysSyncBridge bridge(
      mock_processor_.CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::MoveStoreToFactory(std::move(mock_store)));
  run_loop.Run();
}

TEST_F(JourneysSyncBridgeTest, IsEntityDataValid) {
  JourneysSyncBridge bridge = CreateBridge();
  syncer::EntityData data = CreateTestJourneyEntityData(kTestJourneyId);
  EXPECT_TRUE(bridge.IsEntityDataValid(data));

  syncer::EntityData invalid_data = CreateTestJourneyEntityData("");
  EXPECT_FALSE(bridge.IsEntityDataValid(invalid_data));
}

TEST_F(JourneysSyncBridgeTest, GetClientTagAndStorageKey) {
  JourneysSyncBridge bridge = CreateBridge();
  syncer::EntityData entity_data = CreateTestJourneyEntityData(kTestJourneyId);

  EXPECT_EQ(kTestJourneyId, bridge.GetClientTag(entity_data));
  EXPECT_EQ(kTestJourneyId, bridge.GetStorageKey(entity_data));
}

TEST_F(JourneysSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  JourneysSyncBridge bridge = CreateBridge();
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_journey() = CreateTestJourneySpecifics();

  EXPECT_THAT(bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics),
              EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(JourneysSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesUnknownFields) {
  JourneysSyncBridge bridge = CreateBridge();
  sync_pb::EntitySpecifics specifics;
  sync_pb::JourneySpecifics* journey_specifics = specifics.mutable_journey();
  *journey_specifics = CreateTestJourneySpecifics();
  syncer::test::AddUnknownFieldToProto(*journey_specifics, "unknown_field");

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_TRUE(trimmed_specifics.has_journey());
  EXPECT_THAT(trimmed_specifics.journey(),
              syncer::test::HasUnknownField("unknown_field"));
}

TEST_F(JourneysSyncBridgeTest,
       ApplyDisableSyncChangesDeletesAllDataAndMetadata) {
  auto mock_store = std::make_unique<NiceMock<syncer::MockDataTypeStore>>();
  syncer::MockDataTypeStore* raw_mock_store = mock_store.get();

  JourneysSyncBridge bridge(
      mock_processor_.CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::MoveStoreToFactory(std::move(mock_store)));

  EXPECT_CALL(*raw_mock_store, DeleteAllDataAndMetadata(_, _));
  bridge.ApplyDisableSyncChanges(bridge.CreateMetadataChangeList());
}

}  // namespace
}  // namespace history::journeys
