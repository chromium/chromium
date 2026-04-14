// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

testing::Matcher<MemorySearchResult> MatchesMemorySearchResult(
    EntryType expected_type,
    std::u16string_view expected_value) {
  return AllOf(Field(&MemorySearchResult::type, expected_type),
               Field(&MemorySearchResult::value, expected_value));
}

sync_pb::AccessibilityAnnotationSpecifics
CreateSpecifics(const std::string& id, EntityType type, int64_t timestamp = 0) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id(id);

  if (timestamp > 0) {
    auto* source = specifics.add_sources();
    source->mutable_gmail_source()->set_received_time_unix_epoch_seconds(
        timestamp);
  }

  switch (type) {
    case EntityType::kOrder:
      specifics.mutable_order()->set_order_id("order_" + id);
      break;
    case EntityType::kFlightReservation:
      specifics.mutable_flight_reservation()->set_flight_number("flight_" + id);
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
    provider_ = std::make_unique<SyncBridgeDataProvider>(backend_);
  }

 protected:
  SyncBridgeDataProvider* provider() { return provider_.get(); }

  base::test::TaskEnvironment task_environment_;
  TestAccessibilityAnnotatorBackend backend_;
  std::unique_ptr<SyncBridgeDataProvider> provider_;
};

TEST_F(SyncBridgeDataProviderTest, RetrieveAll_SingleEntry) {
  backend_.SetSyncAnnotations({CreateSpecifics("1", EntityType::kOrder)});

  base::RunLoop run_loop;
  base::MockCallback<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callback;
  EXPECT_CALL(callback, Run(ElementsAre(MatchesMemorySearchResult(
                            EntryType::kOrderId, u"order_1"))))
      .WillOnce([&]() { run_loop.Quit(); });

  provider()->RetrieveAll(EntryType::kOrderId, callback.Get());
  run_loop.Run();
}

TEST_F(SyncBridgeDataProviderTest,
       RetrieveAll_MultipleEntriesAndFiltering_SortsByTimestamp) {
  backend_.SetSyncAnnotations(
      {CreateSpecifics("1", EntityType::kOrder, /*timestamp=*/100),
       CreateSpecifics("2", EntityType::kFlightReservation, /*timestamp=*/400),
       CreateSpecifics("3", EntityType::kOrder, /*timestamp=*/200),
       CreateSpecifics("4", EntityType::kOrder, /*timestamp=*/50)});

  base::RunLoop run_loop;
  base::MockCallback<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callback;
  // Order 3 has timestamp 200, Order 1 has 100, Order 4 has 50.
  // We expect them to be sorted in descending order of timestamp.
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_3"),
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_1"),
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_4"))))
      .WillOnce([&]() { run_loop.Quit(); });
  provider()->RetrieveAll(EntryType::kOrderId, callback.Get());
  run_loop.Run();
}

TEST_F(SyncBridgeDataProviderTest,
       RetrieveAll_MultipleSourcesForSameEntity_SortsByMaxTimestamp) {
  sync_pb::AccessibilityAnnotationSpecifics specifics1 =
      CreateSpecifics("1", EntityType::kOrder, /*timestamp=*/100);

  sync_pb::AccessibilityAnnotationSpecifics specifics2;
  specifics2.set_id("2");
  specifics2.mutable_order()->set_order_id("order_2");
  auto* source2_1 = specifics2.add_sources();
  source2_1->mutable_gmail_source()->set_received_time_unix_epoch_seconds(50);
  auto* source2_2 = specifics2.add_sources();
  source2_2->mutable_calendar_source()->set_modified_time_unix_epoch_seconds(
      400);
  auto* source2_3 = specifics2.add_sources();
  source2_3->mutable_photos_source()->set_creation_time_unix_epoch_seconds(150);

  sync_pb::AccessibilityAnnotationSpecifics specifics3;
  specifics3.set_id("3");
  specifics3.mutable_order()->set_order_id("order_3");
  auto* source3_1 = specifics3.add_sources();
  source3_1->mutable_photos_source()->set_creation_time_unix_epoch_seconds(200);
  auto* source3_2 = specifics3.add_sources();
  source3_2->mutable_gmail_source()->set_received_time_unix_epoch_seconds(10);

  backend_.SetSyncAnnotations({specifics1, specifics2, specifics3});

  base::RunLoop run_loop;
  base::MockCallback<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callback;
  // Order 2 has max timestamp 400, Order 3 has max timestamp 200, Order 1 has
  // 100. We expect them to be sorted in descending order of their max
  // timestamp.
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_2"),
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_3"),
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_1"))))
      .WillOnce([&]() { run_loop.Quit(); });
  provider()->RetrieveAll(EntryType::kOrderId, callback.Get());
  run_loop.Run();
}

TEST_F(SyncBridgeDataProviderTest, RetrieveAll_EmptyBackend) {
  backend_.SetSyncAnnotations({});

  base::RunLoop run_loop;
  base::MockCallback<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callback;
  EXPECT_CALL(callback, Run(IsEmpty())).WillOnce([&]() { run_loop.Quit(); });
  provider()->RetrieveAll(EntryType::kOrderId, callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace accessibility_annotator
