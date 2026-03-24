// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_data_type_store_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;

sync_pb::AccessibilityAnnotationSpecifics CreateSpecifics(const std::string& id,
                                                          EntityType type) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id(id);
  switch (type) {
    case EntityType::kOrder:
      specifics.mutable_order()->set_order_id("order_123");
      break;
    default:
      break;
  }
  return specifics;
}

class SyncBridgeDataProviderTest : public ::testing::Test {
 public:
  SyncBridgeDataProviderTest() = default;
  ~SyncBridgeDataProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto mock_processor =
        std::make_unique<NiceMock<syncer::MockDataTypeLocalChangeProcessor>>();
    ON_CALL(*mock_processor, GetEntityModificationTime(_))
        .WillByDefault(Return(base::Time::Now()));

    backend_ = std::make_unique<AccessibilityAnnotatorBackend>(
        /*history_service=*/nullptr,
        syncer::TestDataTypeStoreService().GetStoreFactory(),
        std::move(mock_processor),
        temp_dir_.GetPath().AppendASCII("TestAccessibilityAnnotatorDatabase"));
    provider_ = std::make_unique<SyncBridgeDataProvider>(*backend_);
  }

 protected:
  void AddSpecificsToBridge(
      const std::vector<sync_pb::AccessibilityAnnotationSpecifics>& specifics) {
    syncer::EntityChangeList change_list;
    for (const auto& s : specifics) {
      syncer::EntityData data;
      *data.specifics.mutable_accessibility_annotation() = s;
      data.name = s.id();
      change_list.push_back(
          syncer::EntityChange::CreateAdd(s.id(), std::move(data)));
    }
    backend_->accessibility_annotation_sync_bridge()->MergeFullSyncData(
        backend_->accessibility_annotation_sync_bridge()
            ->CreateMetadataChangeList(),
        std::move(change_list));
  }

  SyncBridgeDataProvider* provider() { return provider_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorBackend> backend_;
  std::unique_ptr<SyncBridgeDataProvider> provider_;
};

TEST_F(SyncBridgeDataProviderTest, RetrieveAll) {
  AddSpecificsToBridge({CreateSpecifics("order_1", EntityType::kOrder)});

  std::vector<MemorySearchResult> results =
      provider()->RetrieveAll(QueryIntentType::kOrderId);

  // TODO(crbug.com/493849593): Add tests for entries.
  EXPECT_THAT(results, IsEmpty());
}

}  // namespace
}  // namespace accessibility_annotator
