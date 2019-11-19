// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/cleanup_visuals_task.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/get_visuals_task.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"

namespace offline_pages {
namespace {

class CleanupVisualsTaskTest : public ModelTaskTestBase {
 public:
  ~CleanupVisualsTaskTest() override {}

  std::unique_ptr<OfflinePageVisuals> ReadVisuals(int64_t offline_id) {
    std::unique_ptr<OfflinePageVisuals> visuals;
    auto callback = [&](std::unique_ptr<OfflinePageVisuals> result) {
      visuals = std::move(result);
    };
    RunTask(std::make_unique<GetVisualsTask>(
        store(), offline_id, base::BindLambdaForTesting(callback)));
    return visuals;
  }

  OfflinePageVisuals MustReadVisuals(int64_t offline_id) {
    std::unique_ptr<OfflinePageVisuals> visuals = ReadVisuals(offline_id);
    CHECK(visuals);
    return *visuals;
  }

  void StoreVisuals(int64_t offline_id,
                    std::string thumbnail,
                    std::string favicon) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), offline_id, thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(store(), offline_id, favicon,
                                                   base::DoNothing()));
  }
};

TEST_F(CleanupVisualsTaskTest, DbConnectionIsNull) {
  base::MockCallback<CleanupVisualsCallback> callback;
  EXPECT_CALL(callback, Run(false)).Times(1);
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  RunTask(std::make_unique<CleanupVisualsTask>(
      store(), store_utils::FromDatabaseTime(1000), callback.Get()));
}

TEST_F(CleanupVisualsTaskTest, CleanupNoVisuals) {
  base::MockCallback<CleanupVisualsCallback> callback;
  EXPECT_CALL(callback, Run(true)).Times(1);

  base::HistogramTester histogram_tester;
  RunTask(std::make_unique<CleanupVisualsTask>(
      store(), store_utils::FromDatabaseTime(1000), callback.Get()));

  histogram_tester.ExpectUniqueSample("OfflinePages.CleanupThumbnails.Count", 0,
                                      1);
}

TEST_F(CleanupVisualsTaskTest, CleanupAllCombinations) {
  // Two conditions contribute to thumbnail cleanup: does a corresponding
  // OfflinePageItem exist, and is the thumbnail expired. All four combinations
  // of these states are tested.

  // Start slightly above base::Time() to avoid negative time below.
  TestScopedOfflineClock test_clock;
  test_clock.SetNow(base::Time() + base::TimeDelta::FromDays(1));

  // 1. Has item, not expired.
  OfflinePageItem item1 = generator()->CreateItem();
  store_test_util()->InsertItem(item1);

  OfflinePageVisuals visuals1(item1.offline_id,
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb1", "favicon1");
  StoreVisuals(visuals1.offline_id, visuals1.thumbnail, visuals1.favicon);

  // 2. Has item, expired.
  OfflinePageItem item2 = generator()->CreateItem();
  store_test_util()->InsertItem(item2);
  test_clock.Advance(base::TimeDelta::FromSeconds(-1));
  OfflinePageVisuals visuals2(item2.offline_id,
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb2", "favicon2");
  StoreVisuals(visuals2.offline_id, visuals2.thumbnail, visuals2.favicon);

  // 3. No item, not expired.
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  OfflinePageVisuals visuals3(store_utils::GenerateOfflineId(),
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb3", "favicon3");
  StoreVisuals(visuals3.offline_id, visuals3.thumbnail, visuals3.favicon);

  // 4. No item, expired. This one gets removed.
  test_clock.Advance(base::TimeDelta::FromSeconds(-1));
  OfflinePageVisuals visuals4(store_utils::GenerateOfflineId(),
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb4", "favicon4");
  StoreVisuals(visuals4.offline_id, visuals4.thumbnail, visuals4.favicon);

  base::MockCallback<CleanupVisualsCallback> callback;
  EXPECT_CALL(callback, Run(true)).Times(1);

  test_clock.Advance(kVisualsExpirationDelta + base::TimeDelta::FromSeconds(1));

  base::HistogramTester histogram_tester;
  RunTask(std::make_unique<CleanupVisualsTask>(store(), OfflineTimeNow(),
                                               callback.Get()));
  EXPECT_EQ(visuals1, MustReadVisuals(visuals1.offline_id));
  EXPECT_EQ(visuals2, MustReadVisuals(visuals2.offline_id));
  EXPECT_EQ(visuals3, MustReadVisuals(visuals3.offline_id));
  EXPECT_EQ(nullptr, ReadVisuals(visuals4.offline_id).get());

  histogram_tester.ExpectUniqueSample("OfflinePages.CleanupThumbnails.Count", 1,
                                      1);
}

}  // namespace
}  // namespace offline_pages
