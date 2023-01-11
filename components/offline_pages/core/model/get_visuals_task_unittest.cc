// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/get_visuals_task.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/offline_pages/task/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace offline_pages {
namespace {

OfflinePageVisuals TestVisuals(base::Time now) {
  OfflinePageVisuals visuals;
  visuals.offline_id = 1;
  visuals.expiration = now + kVisualsExpirationDelta;
  visuals.thumbnail = "123abc";
  visuals.favicon = "favicon";
  return visuals;
}

class GetVisualsTaskTest : public ModelTaskTestBase {
 public:
  ~GetVisualsTaskTest() override = default;

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

  void StoreVisuals(const OfflinePageVisuals& visuals) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), visuals.offline_id, visuals.thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(
        store(), visuals.offline_id, visuals.favicon, base::DoNothing()));
  }
};

TEST_F(GetVisualsTaskTest, NotFound) {
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        called = true;
        EXPECT_FALSE(result);
      });

  RunTask(std::make_unique<GetVisualsTask>(store(), 1, std::move(callback)));
  EXPECT_TRUE(called);
}

TEST_F(GetVisualsTaskTest, Found) {
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = TestVisuals(OfflineTimeNow());
  StoreVisuals(visuals);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(visuals, *result);
      });

  RunTask(
      std::make_unique<GetVisualsTask>(store(), visuals.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetVisualsTaskTest, FoundThumbnailOnly) {
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = TestVisuals(OfflineTimeNow());
  visuals.favicon = std::string();
  StoreVisuals(visuals);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(visuals, *result);
      });

  RunTask(
      std::make_unique<GetVisualsTask>(store(), visuals.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetVisualsTaskTest, FoundFaviconOnly) {
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = TestVisuals(OfflineTimeNow());
  visuals.thumbnail = std::string();
  StoreVisuals(visuals);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(visuals, *result);
      });

  RunTask(
      std::make_unique<GetVisualsTask>(store(), visuals.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetVisualsTaskTest, DbConnectionIsNull) {
  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(store(), 1, std::string(),
                                                   base::DoNothing()));

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        called = true;
        EXPECT_FALSE(result);
      });
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);

  RunTask(std::make_unique<GetVisualsTask>(store(), 1, std::move(callback)));

  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace offline_pages
