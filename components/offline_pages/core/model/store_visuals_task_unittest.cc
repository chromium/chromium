// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/store_visuals_task.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/get_visuals_task.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"

namespace offline_pages {
namespace {

const char kThumbnailData[] = "123abc";
const char kFaviconData[] = "favicon";
OfflinePageVisuals CreateVisualsItem(base::Time now) {
  return OfflinePageVisuals(1, now + kVisualsExpirationDelta, kThumbnailData,
                            kFaviconData);
}

class StoreVisualsTaskTest : public ModelTaskTestBase {
 public:
  ~StoreVisualsTaskTest() override = default;

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
};

TEST_F(StoreVisualsTaskTest, Success) {
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = CreateVisualsItem(OfflineTimeNow());
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, visuals.thumbnail));
  EXPECT_CALL(callback, Run(true, visuals.favicon));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), visuals.offline_id, visuals.thumbnail, callback.Get()));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(
      store(), visuals.offline_id, visuals.favicon, callback.Get()));

  EXPECT_EQ(visuals, MustReadVisuals(visuals.offline_id));
}

TEST_F(StoreVisualsTaskTest, AlreadyExists) {
  // Store the same thumbnail twice. The second operation should overwrite the
  // first.
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = CreateVisualsItem(OfflineTimeNow());
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, visuals.favicon));
  EXPECT_CALL(callback, Run(true, visuals.thumbnail));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), visuals.offline_id, visuals.thumbnail, callback.Get()));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(
      store(), visuals.offline_id, visuals.favicon, callback.Get()));

  EXPECT_EQ(visuals, MustReadVisuals(visuals.offline_id));

  test_clock.Advance(base::Days(1));
  visuals.thumbnail += "_extradata";
  visuals.expiration = OfflineTimeNow() + kVisualsExpirationDelta;
  EXPECT_CALL(callback, Run(true, visuals.thumbnail));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), visuals.offline_id, visuals.thumbnail, callback.Get()));

  OfflinePageVisuals got_visuals = MustReadVisuals(visuals.offline_id);
  EXPECT_EQ(visuals.thumbnail, got_visuals.thumbnail);
  EXPECT_EQ(visuals.expiration, got_visuals.expiration);
}

TEST_F(StoreVisualsTaskTest, IgnoreEmptyStrings) {
  TestScopedOfflineClock test_clock;
  OfflinePageVisuals visuals = CreateVisualsItem(OfflineTimeNow());
  visuals.favicon = std::string();
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, visuals.thumbnail));
  EXPECT_CALL(callback, Run(true, std::string()));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), visuals.offline_id, visuals.thumbnail, callback.Get()));
  EXPECT_EQ(visuals, MustReadVisuals(visuals.offline_id));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), visuals.offline_id, std::string(), callback.Get()));
  EXPECT_EQ(visuals.thumbnail, MustReadVisuals(visuals.offline_id).thumbnail);
}

TEST_F(StoreVisualsTaskTest, DbConnectionIsNull) {
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(false, kThumbnailData));
  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(store(), 1, kThumbnailData,
                                                   callback.Get()));

  EXPECT_CALL(callback, Run(false, kFaviconData));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(store(), 1, kFaviconData,
                                                 callback.Get()));
}

}  // namespace
}  // namespace offline_pages
