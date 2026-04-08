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
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

testing::Matcher<MemorySearchResult> MatchesMemorySearchResult(
    EntryType expected_type,
    std::u16string_view expected_value) {
  return AllOf(Field(&MemorySearchResult::type, expected_type),
               Field(&MemorySearchResult::value, expected_value));
}

sync_pb::AccessibilityAnnotationSpecifics CreateSpecifics(const std::string& id,
                                                          EntityType type) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id(id);
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
  EXPECT_CALL(callback, Run(UnorderedElementsAre(MatchesMemorySearchResult(
                            EntryType::kOrderId, u"order_1"))))
      .WillOnce([&]() { run_loop.Quit(); });

  provider()->RetrieveAll(EntryType::kOrderId, callback.Get());
  run_loop.Run();
}

TEST_F(SyncBridgeDataProviderTest, RetrieveAll_MultipleEntriesAndFiltering) {
  backend_.SetSyncAnnotations(
      {CreateSpecifics("1", EntityType::kOrder),
       CreateSpecifics("2", EntityType::kFlightReservation),
       CreateSpecifics("3", EntityType::kOrder)});

  base::RunLoop run_loop;
  base::MockCallback<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callback;
  EXPECT_CALL(callback,
              Run(UnorderedElementsAre(
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_1"),
                  MatchesMemorySearchResult(EntryType::kOrderId, u"order_3"))))
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
