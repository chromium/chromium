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

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    GURL url = GURL("https://foo.com");
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    content::StoragePartition* storage_partition =
        main_rfh()->GetStoragePartition();
    environment_integrity::AndroidEnvironmentIntegrityDataManager::
        CreateForStoragePartition(storage_partition);
  }
};

TEST_F(AndroidEnvironmentIntegrityDataManagerTest, GetAndSetHandle) {
  content::StoragePartition* storage_partition =
      main_rfh()->GetStoragePartition();
  AndroidEnvironmentIntegrityDataManager* manager =
      environment_integrity::AndroidEnvironmentIntegrityDataManager::
          GetForStoragePartition(storage_partition);

  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
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

TEST_F(AndroidEnvironmentIntegrityDataManagerTest, ClearDataForOrigin) {
  content::StoragePartition* storage_partition =
      main_rfh()->GetStoragePartition();
  AndroidEnvironmentIntegrityDataManager* manager =
      environment_integrity::AndroidEnvironmentIntegrityDataManager::
          GetForStoragePartition(storage_partition);

  url::Origin origin1 = url::Origin::Create(GURL("https://foo.com"));
  int64_t handle1 = 123;
  url::Origin origin2 = url::Origin::Create(GURL("https://bar.com"));
  int64_t handle2 = 234;

  manager->SetHandle(origin1, handle1);
  manager->SetHandle(origin2, handle2);

  manager->GetHandle(origin1,
                     base::BindLambdaForTesting(
                         [handle1](absl::optional<int64_t> maybe_handle) {
                           // Result should match set handle.
                           EXPECT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(maybe_handle.value(), handle1);
                         }));
  manager->GetHandle(origin2,
                     base::BindLambdaForTesting(
                         [handle2](absl::optional<int64_t> maybe_handle) {
                           // Result should match set handle.
                           EXPECT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(maybe_handle.value(), handle2);
                         }));

  manager->OnStorageKeyDataCleared(
      content::StoragePartition::REMOVE_DATA_MASK_ENVIRONMENT_INTEGRITY,
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(origin1)),
      base::Time::Now(), base::Time::Now());

  manager->GetHandle(origin1, base::BindLambdaForTesting(
                                  [](absl::optional<int64_t> maybe_handle) {
                                    // No result for deleted handle.
                                    EXPECT_FALSE(maybe_handle.has_value());
                                  }));
  manager->GetHandle(origin2,
                     base::BindLambdaForTesting(
                         [handle2](absl::optional<int64_t> maybe_handle) {
                           // Result should match set handle.
                           EXPECT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(maybe_handle.value(), handle2);
                         }));
}

TEST_F(AndroidEnvironmentIntegrityDataManagerTest, ClearAllData) {
  content::StoragePartition* storage_partition =
      main_rfh()->GetStoragePartition();
  AndroidEnvironmentIntegrityDataManager* manager =
      environment_integrity::AndroidEnvironmentIntegrityDataManager::
          GetForStoragePartition(storage_partition);

  url::Origin origin1 = url::Origin::Create(GURL("https://foo.com"));
  int64_t handle1 = 123;
  url::Origin origin2 = url::Origin::Create(GURL("https://bar.com"));
  int64_t handle2 = 234;

  manager->SetHandle(origin1, handle1);
  manager->SetHandle(origin2, handle2);

  manager->GetHandle(origin1,
                     base::BindLambdaForTesting(
                         [handle1](absl::optional<int64_t> maybe_handle) {
                           // Result should match set handle.
                           EXPECT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(maybe_handle.value(), handle1);
                         }));
  manager->GetHandle(origin2,
                     base::BindLambdaForTesting(
                         [handle2](absl::optional<int64_t> maybe_handle) {
                           // Result should match set handle.
                           EXPECT_TRUE(maybe_handle.has_value());
                           EXPECT_EQ(maybe_handle.value(), handle2);
                         }));

  url::Origin opaque_origin;
  manager->OnStorageKeyDataCleared(
      content::StoragePartition::REMOVE_DATA_MASK_ENVIRONMENT_INTEGRITY,
      base::NullCallback(), base::Time::Now(), base::Time::Now());

  manager->GetHandle(origin1, base::BindLambdaForTesting(
                                  [](absl::optional<int64_t> maybe_handle) {
                                    // No result for deleted handle.
                                    EXPECT_FALSE(maybe_handle.has_value());
                                  }));
  manager->GetHandle(origin2, base::BindLambdaForTesting(
                                  [](absl::optional<int64_t> maybe_handle) {
                                    // No result for deleted handle.
                                    EXPECT_FALSE(maybe_handle.has_value());
                                  }));
}

}  // namespace environment_integrity
