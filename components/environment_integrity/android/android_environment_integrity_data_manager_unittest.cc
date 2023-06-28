// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace environment_integrity {

class AndroidEnvironmentIntegrityDataManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  AndroidEnvironmentIntegrityDataManagerTest() = default;
};

TEST_F(AndroidEnvironmentIntegrityDataManagerTest, GetAndSetHandle) {
  GURL url = GURL("https://foo.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  content::StoragePartition* storage_partition =
      main_rfh()->GetStoragePartition();
  environment_integrity::AndroidEnvironmentIntegrityDataManager::
      CreateForStoragePartition(storage_partition);
  AndroidEnvironmentIntegrityDataManager* manager =
      environment_integrity::AndroidEnvironmentIntegrityDataManager::
          GetForStoragePartition(storage_partition);

  url::Origin origin = url::Origin::Create(url);
  manager->GetHandle(origin, base::BindLambdaForTesting(
                                 [](absl::optional<int64_t> maybe_handle) {
                                   // No result before the handle has been set
                                   // for the origin.
                                   EXPECT_FALSE(maybe_handle);
                                 }));

  int64_t handle = 123;
  manager->SetHandle(origin, handle);
  manager->GetHandle(origin,
                     base::BindLambdaForTesting(
                         [handle](absl::optional<int64_t> maybe_handle) {
                           // Result should match the set handle.
                           ASSERT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(*maybe_handle, handle);
                         }));

  int64_t new_handle = 234;
  manager->SetHandle(origin, new_handle);
  manager->GetHandle(origin,
                     base::BindLambdaForTesting(
                         [new_handle](absl::optional<int64_t> maybe_handle) {
                           // Result should match the updated handle.
                           ASSERT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(*maybe_handle, new_handle);
                         }));
}

}  // namespace environment_integrity
