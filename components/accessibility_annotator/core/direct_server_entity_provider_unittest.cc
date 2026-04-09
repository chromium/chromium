// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/direct_server_entity_provider.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Ref;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

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
    provider_ = std::make_unique<DirectServerEntityProvider>(backend_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAccessibilityAnnotatorBackend backend_;
  std::unique_ptr<DirectServerEntityProvider> provider_;
  testing::NiceMock<MockEntityDataProviderObserver> mock_observer_;
};

TEST_F(DirectServerEntityProviderTest, GetEntitiesReturnsEmpty) {
  backend_.SetSyncAnnotations({});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities(/*types=*/{}, future.GetCallback());
  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(DirectServerEntityProviderTest, GetEntities_AllTypes) {
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder),
                               CreateSpecifics("2", EntityType::kShipment)});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities(EntityTypeEnumSet::All(), future.GetCallback());
  EXPECT_THAT(future.Get(),
              UnorderedElementsAre(Field(&Entity::entity_id, "1"),
                                   Field(&Entity::entity_id, "2")));
}

TEST_F(DirectServerEntityProviderTest, GetEntities_SubsetTypes) {
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder),
                               CreateSpecifics("2", EntityType::kShipment)});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities({EntityType::kOrder}, future.GetCallback());

  std::vector<Entity> entities = future.Take();
  ASSERT_THAT(entities, SizeIs(1));
  EXPECT_THAT(entities[0],
              AllOf(Field(&Entity::entity_id, "1"),
                    Property(&Entity::GetType, EntityType::kOrder)));
}

TEST_F(DirectServerEntityProviderTest, GetEntities_NoTypes) {
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder),
                               CreateSpecifics("2", EntityType::kShipment)});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities(/*types=*/{}, future.GetCallback());
  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(DirectServerEntityProviderTest, GetEntities_TypeNotPresent) {
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder)});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities({EntityType::kShipment}, future.GetCallback());
  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(DirectServerEntityProviderTest, GetEntitiesRecordsSuccessMetrics) {
  base::HistogramTester histogram_tester;
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder),
                               CreateSpecifics("2", EntityType::kShipment)});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities(EntityTypeEnumSet::All(), future.GetCallback());
  EXPECT_THAT(future.Get(), SizeIs(2));

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.DirectServerProvider.EntityCount", 2, 1);
  histogram_tester.ExpectTotalCount(
      "AccessibilityAnnotator.DirectServerProvider.GetEntitiesLatency", 1);
}

TEST_F(DirectServerEntityProviderTest, GetEntitiesRecordsNoDataMetrics) {
  base::HistogramTester histogram_tester;
  backend_.SetSyncAnnotations({});

  base::test::TestFuture<std::vector<Entity>> future;
  provider_->GetEntities(/*types=*/{}, future.GetCallback());
  EXPECT_THAT(future.Get(), IsEmpty());

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.DirectServerProvider.EntityCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "AccessibilityAnnotator.DirectServerProvider.GetEntitiesLatency", 1);
}

TEST_F(DirectServerEntityProviderTest, ObserverNotifiedOnBridgeLoaded) {
  provider_->AddObserver(&mock_observer_);

  EXPECT_CALL(mock_observer_,
              OnEntityDataChanged(Ref(*provider_), EntityTypeEnumSet::All()));

  provider_->OnAccessibilityAnnotationSyncBridgeLoaded();

  provider_->RemoveObserver(&mock_observer_);
}

TEST_F(DirectServerEntityProviderTest, ObserverNotifiedOnAnnotationChanged) {
  provider_->AddObserver(&mock_observer_);

  EXPECT_CALL(mock_observer_,
              OnEntityDataChanged(Ref(*provider_), EntityTypeEnumSet::All()));

  provider_->OnAccessibilityAnnotationChanged();

  provider_->RemoveObserver(&mock_observer_);
}

}  // namespace

}  // namespace accessibility_annotator
