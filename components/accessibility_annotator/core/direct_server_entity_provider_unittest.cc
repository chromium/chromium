// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/direct_server_entity_provider.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "components/sync/test/test_data_type_store_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::base::test::RunClosure;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Ref;
using ::testing::SizeIs;

sync_pb::AccessibilityAnnotationSpecifics CreateSpecifics(const std::string& id,
                                                          EntityType type) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id(id);
  switch (type) {
    case EntityType::kOrder:
      specifics.mutable_order()->set_order_id("order_123");
      break;
    case EntityType::kShipment:
      specifics.mutable_shipment()->set_tracking_number("track_123");
      break;
    default:
      // Add more types as needed for tests.
      break;
  }
  return specifics;
}

class MockEntityDataProviderObserver : public EntityDataProvider::Observer {
 public:
  MOCK_METHOD(void,
              OnEntityDataChanged,
              (EntityDataProvider & provider, EntityTypeEnumSet types),
              (override));
};

class DirectServerEntityProviderTest : public testing::Test {
 public:
  DirectServerEntityProviderTest() = default;
  ~DirectServerEntityProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = std::make_unique<AccessibilityAnnotatorBackend>(
        version_info::Channel::UNKNOWN,
        syncer::TestDataTypeStoreService().GetStoreFactory(),
        temp_dir_.GetPath().AppendASCII("TestAccessibilityAnnotatorDatabase"));
    provider_ = std::make_unique<DirectServerEntityProvider>(*backend_);
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

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorBackend> backend_;
  std::unique_ptr<DirectServerEntityProvider> provider_;
  testing::NiceMock<MockEntityDataProviderObserver> mock_observer_;
};

TEST_F(DirectServerEntityProviderTest, GetEntitiesReturnsEmpty) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(IsEmpty())).WillOnce(RunClosure(run_loop.QuitClosure()));
  provider_->GetEntities(/*types=*/{}, cb.Get());
  run_loop.Run();
}

TEST_F(DirectServerEntityProviderTest, GetEntities_AllTypes) {
  AddSpecificsToBridge({CreateSpecifics("1", EntityType::kOrder),
                        CreateSpecifics("2", EntityType::kShipment)});

  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(testing::UnorderedElementsAre(
                      testing::Field(&Entity::entity_id, "1"),
                      testing::Field(&Entity::entity_id, "2"))))
      .WillOnce([&](std::vector<Entity> /*entities*/) { run_loop.Quit(); });
  provider_->GetEntities(EntityTypeEnumSet::All(), cb.Get());
  run_loop.Run();
}

TEST_F(DirectServerEntityProviderTest, GetEntities_SubsetTypes) {
  AddSpecificsToBridge({CreateSpecifics("1", EntityType::kOrder),
                        CreateSpecifics("2", EntityType::kShipment)});

  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(SizeIs(1))).WillOnce([&](std::vector<Entity> entities) {
    EXPECT_EQ(entities[0].entity_id, "1");
    EXPECT_EQ(entities[0].GetType(), EntityType::kOrder);
    run_loop.Quit();
  });
  provider_->GetEntities({EntityType::kOrder}, cb.Get());
  run_loop.Run();
}

TEST_F(DirectServerEntityProviderTest, GetEntities_NoTypes) {
  AddSpecificsToBridge({CreateSpecifics("1", EntityType::kOrder),
                        CreateSpecifics("2", EntityType::kShipment)});

  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(IsEmpty())).WillOnce(RunClosure(run_loop.QuitClosure()));
  provider_->GetEntities(/*types=*/{}, cb.Get());
  run_loop.Run();
}

TEST_F(DirectServerEntityProviderTest, GetEntities_TypeNotPresent) {
  AddSpecificsToBridge({CreateSpecifics("1", EntityType::kOrder)});

  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(IsEmpty())).WillOnce(RunClosure(run_loop.QuitClosure()));
  provider_->GetEntities({EntityType::kShipment}, cb.Get());
  run_loop.Run();
}

TEST_F(DirectServerEntityProviderTest, ObserverNotifiedOnBridgeLoaded) {
  provider_->AddObserver(&mock_observer_);
  base::RunLoop run_loop;

  EXPECT_CALL(mock_observer_,
              OnEntityDataChanged(Ref(*provider_), EntityTypeEnumSet::All()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  provider_->OnAccessibilityAnnotationSyncBridgeLoaded();
  run_loop.Run();

  provider_->RemoveObserver(&mock_observer_);
}

TEST_F(DirectServerEntityProviderTest, ObserverNotifiedOnAnnotationChanged) {
  provider_->AddObserver(&mock_observer_);
  base::RunLoop run_loop;

  EXPECT_CALL(mock_observer_,
              OnEntityDataChanged(Ref(*provider_), EntityTypeEnumSet::All()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  provider_->OnAccessibilityAnnotationChanged();
  run_loop.Run();

  provider_->RemoveObserver(&mock_observer_);
}

}  // namespace

}  // namespace accessibility_annotator
