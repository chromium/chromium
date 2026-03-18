// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::testing::Property;
using ::testing::UnorderedElementsAre;

class MockObserver : public AccessibilityAnnotationSyncBridge::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnAccessibilityAnnotationSyncBridgeLoaded, (), (override));

  MOCK_METHOD(void, OnAccessibilityAnnotationChanged, (), (override));
};

class AccessibilityAnnotationSyncBridgeTest : public testing::Test {
 protected:
  bool AddAccessibilityAnnotationSpecifics(
      const sync_pb::AccessibilityAnnotationSpecifics& specifics) {
    syncer::EntityChangeList add_changes;
    syncer::EntityData entity_data;
    *entity_data.specifics.mutable_accessibility_annotation() = specifics;
    add_changes.push_back(syncer::EntityChange::CreateAdd(
        specifics.id(), std::move(entity_data)));

    std::optional<syncer::ModelError> error =
        bridge_->ApplyIncrementalSyncChanges(
            bridge_->CreateMetadataChangeList(), std::move(add_changes));
    return !error.has_value();
  }

  bool AddAccessibilityAnnotation(const std::string& id) {
    sync_pb::AccessibilityAnnotationSpecifics specifics;
    specifics.set_id(id);
    return AddAccessibilityAnnotationSpecifics(specifics);
  }

  bool DeleteAccessibilityAnnotation(const std::string& id) {
    syncer::EntityChangeList delete_changes;
    delete_changes.push_back(
        syncer::EntityChange::CreateDelete(id, syncer::EntityData()));
    std::optional<syncer::ModelError> error =
        bridge_->ApplyIncrementalSyncChanges(
            bridge_->CreateMetadataChangeList(), std::move(delete_changes));
    return !error.has_value();
  }

  void AddInitialSpecificsToStore(const std::string& id) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    sync_pb::AccessibilityAnnotationSpecifics specifics;
    specifics.set_id(id);
    batch->WriteData(id, specifics.SerializeAsString());

    base::test::TestFuture<const std::optional<syncer::ModelError>&> future;
    store_->CommitWriteBatch(std::move(batch), future.GetCallback());
    ASSERT_FALSE(future.Get().has_value());
  }

  AccessibilityAnnotationSyncBridge* bridge() { return bridge_.get(); }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::DataTypeStore> store_ =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  std::unique_ptr<AccessibilityAnnotationSyncBridge> bridge_ =
      std::make_unique<AccessibilityAnnotationSyncBridge>(
          mock_processor_.CreateForwardingProcessor(),
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
              store_.get()));
};

TEST_F(AccessibilityAnnotationSyncBridgeTest, ApplyIncrementalSyncChanges) {
  ASSERT_TRUE(AddAccessibilityAnnotation("1"));
  ASSERT_TRUE(AddAccessibilityAnnotation("2"));

  std::vector<sync_pb::AccessibilityAnnotationSpecifics> annotations =
      bridge()->GetAllAnnotations();
  EXPECT_THAT(
      annotations,
      UnorderedElementsAre(
          Property(&sync_pb::AccessibilityAnnotationSpecifics::id, "1"),
          Property(&sync_pb::AccessibilityAnnotationSpecifics::id, "2")));

  ASSERT_TRUE(DeleteAccessibilityAnnotation("1"));

  EXPECT_EQ(bridge()->GetAllAnnotations().size(), 1u);
}

TEST_F(AccessibilityAnnotationSyncBridgeTest, DisableSyncChanges) {
  ASSERT_TRUE(AddAccessibilityAnnotation("1"));

  std::vector<sync_pb::AccessibilityAnnotationSpecifics> annotations =
      bridge()->GetAllAnnotations();
  ASSERT_EQ(annotations.size(), 1u);
  EXPECT_EQ(annotations[0].id(), "1");

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  EXPECT_TRUE(bridge()->GetAllAnnotations().empty());
}

TEST_F(AccessibilityAnnotationSyncBridgeTest, ReadAllDataOnStartup) {
  // Pre-populate the underlying store directly before the bridge reads it.
  AddInitialSpecificsToStore("1");

  // Re-creating the bridge will force it to read the initial data from the
  // store.
  bridge_.reset();
  bridge_ = std::make_unique<AccessibilityAnnotationSyncBridge>(
      mock_processor_.CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  testing::NiceMock<MockObserver> observer;
  base::RunLoop run_loop;
  bridge_->AddObserver(&observer);
  ON_CALL(observer, OnAccessibilityAnnotationSyncBridgeLoaded)
      .WillByDefault(base::test::RunClosure(run_loop.QuitClosure()));
  run_loop.Run();
  bridge_->RemoveObserver(&observer);

  std::vector<sync_pb::AccessibilityAnnotationSpecifics> annotations =
      bridge()->GetAllAnnotations();
  ASSERT_EQ(annotations.size(), 1u);
  EXPECT_EQ(annotations[0].id(), "1");
}

TEST_F(AccessibilityAnnotationSyncBridgeTest,
       ObserverAddedOnIncrementalChanges) {
  testing::StrictMock<MockObserver> observer;
  bridge()->AddObserver(&observer);

  EXPECT_CALL(observer, OnAccessibilityAnnotationChanged());

  ASSERT_TRUE(AddAccessibilityAnnotation("1"));

  bridge()->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotationSyncBridgeTest, GetAnnotationsByTypes) {
  sync_pb::AccessibilityAnnotationSpecifics specifics1;
  specifics1.set_id("1");
  specifics1.mutable_order()->set_order_id("order_123");

  sync_pb::AccessibilityAnnotationSpecifics specifics2;
  specifics2.set_id("2");
  specifics2.mutable_shipment()->set_tracking_number("track_123");

  sync_pb::AccessibilityAnnotationSpecifics specifics3;
  specifics3.set_id("3");
  specifics3.mutable_vehicle()->set_vehicle_make("make");

  ASSERT_TRUE(AddAccessibilityAnnotationSpecifics(specifics1));
  ASSERT_TRUE(AddAccessibilityAnnotationSpecifics(specifics2));
  ASSERT_TRUE(AddAccessibilityAnnotationSpecifics(specifics3));

  // Test single type.
  std::vector<sync_pb::AccessibilityAnnotationSpecifics> orders =
      bridge()->GetAnnotationsByTypes({EntityType::kOrder});
  EXPECT_EQ(orders.size(), 1u);
  EXPECT_EQ(orders[0].id(), "1");

  // Test multiple types.
  std::vector<sync_pb::AccessibilityAnnotationSpecifics> orders_and_vehicles =
      bridge()->GetAnnotationsByTypes(
          {EntityType::kOrder, EntityType::kVehicle});
  EXPECT_EQ(orders_and_vehicles.size(), 2u);
  EXPECT_THAT(
      orders_and_vehicles,
      UnorderedElementsAre(
          Property(&sync_pb::AccessibilityAnnotationSpecifics::id, "1"),
          Property(&sync_pb::AccessibilityAnnotationSpecifics::id, "3")));

  // Test no match.
  std::vector<sync_pb::AccessibilityAnnotationSpecifics> passports =
      bridge()->GetAnnotationsByTypes({EntityType::kPassport});
  EXPECT_TRUE(passports.empty());

  // Test all types.
  std::vector<sync_pb::AccessibilityAnnotationSpecifics> all_annotations =
      bridge()->GetAnnotationsByTypes(EntityTypeEnumSet::All());
  EXPECT_EQ(all_annotations.size(), 3u);
}

TEST_F(AccessibilityAnnotationSyncBridgeTest,
       ObserverRemovedOnIncrementalChanges) {
  ASSERT_TRUE(AddAccessibilityAnnotation("1"));

  testing::StrictMock<MockObserver> observer;
  bridge()->AddObserver(&observer);

  EXPECT_CALL(observer, OnAccessibilityAnnotationChanged());

  ASSERT_TRUE(DeleteAccessibilityAnnotation("1"));

  bridge()->RemoveObserver(&observer);
}

}  // namespace

}  // namespace accessibility_annotator
