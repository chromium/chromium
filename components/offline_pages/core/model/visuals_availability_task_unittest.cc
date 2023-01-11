// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/visuals_availability_task.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

OfflinePageVisuals TestVisuals() {
  return OfflinePageVisuals(1, store_utils::FromDatabaseTime(1234),
                            "some thumbnail", "some favicon");
}

using VisualsAvailableCallback =
    VisualsAvailabilityTask::VisualsAvailableCallback;

class VisualsAvailabilityTaskTest : public ModelTaskTestBase {
 public:
  void StoreVisuals(const OfflinePageVisuals& visuals) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), visuals.offline_id, visuals.thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(
        store(), visuals.offline_id, visuals.favicon, base::DoNothing()));
  }
};

TEST_F(VisualsAvailabilityTaskTest, CorrectlyFindsById_ThumbnailAndFavicon) {
  OfflinePageVisuals visuals = TestVisuals();
  StoreVisuals(visuals);

  base::MockCallback<VisualsAvailableCallback> exists_callback;
  VisualsAvailability availability = {true, true};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<VisualsAvailabilityTask>(store(), visuals.offline_id,
                                                    exists_callback.Get()));
}

TEST_F(VisualsAvailabilityTaskTest, CorrectlyFindsById_ThumbnailOnly) {
  OfflinePageVisuals visuals = TestVisuals();
  visuals.favicon = std::string();
  StoreVisuals(visuals);

  base::MockCallback<VisualsAvailableCallback> exists_callback;
  VisualsAvailability availability = {true, false};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<VisualsAvailabilityTask>(store(), visuals.offline_id,
                                                    exists_callback.Get()));
}

TEST_F(VisualsAvailabilityTaskTest, CorrectlyFindsById_FaviconOnly) {
  OfflinePageVisuals visuals = TestVisuals();
  visuals.thumbnail = std::string();
  StoreVisuals(visuals);

  base::MockCallback<VisualsAvailableCallback> exists_callback;
  VisualsAvailability availability = {false, true};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<VisualsAvailabilityTask>(store(), visuals.offline_id,
                                                    exists_callback.Get()));
}

TEST_F(VisualsAvailabilityTaskTest, RowDoesNotExist) {
  const int64_t invalid_offline_id = 2;

  base::MockCallback<VisualsAvailableCallback> doesnt_exist_callback;
  VisualsAvailability availability = {false, false};
  EXPECT_CALL(doesnt_exist_callback, Run(availability));
  RunTask(std::make_unique<VisualsAvailabilityTask>(
      store(), invalid_offline_id, doesnt_exist_callback.Get()));
}

TEST_F(VisualsAvailabilityTaskTest, DbConnectionIsNull) {
  OfflinePageVisuals visuals;
  visuals.offline_id = 1;
  visuals.expiration = store_utils::FromDatabaseTime(1234);
  visuals.thumbnail = "123abc";
  StoreVisuals(visuals);

  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  base::MockCallback<VisualsAvailableCallback> exists_callback;
  VisualsAvailability availability = {false, false};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<VisualsAvailabilityTask>(store(), visuals.offline_id,
                                                    exists_callback.Get()));
}

}  // namespace
}  // namespace offline_pages
