// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::base::test::TestFuture;
using ::google_apis::ApiErrorCode;
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::Tasks;

class GlanceablesTasksClientImplTest : public BrowserWithTestWindowTest {
 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  GlanceablesTasksClientImpl client_;
};

TEST_F(GlanceablesTasksClientImplTest, GetTaskLists) {
  TestFuture<base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>> future;
  auto cancel_closure = client_.GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(GlanceablesTasksClientImplTest, GetTasks) {
  TestFuture<base::expected<std::unique_ptr<Tasks>, ApiErrorCode>> future;
  auto cancel_closure =
      client_.GetTasks(future.GetCallback(), "test-task-list-id");
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());
  EXPECT_TRUE(future.Get().has_value());
}

}  // namespace ash
