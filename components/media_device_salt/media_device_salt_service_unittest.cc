// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_service.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/bind.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace media_device_salt {

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using StorageKeyMatcher =
    ::content::StoragePartition::StorageKeyMatcherFunction;

blink::StorageKey StorageKey1() {
  return blink::StorageKey::CreateFromStringForTesting("https://example1.com");
}

blink::StorageKey StorageKey2() {
  return blink::StorageKey::CreateFromStringForTesting("https://example2.com");
}

blink::StorageKey StorageKey3() {
  return blink::StorageKey::CreateFromStringForTesting("https://example3.com");
}

}  // namespace

class MediaDeviceSaltServiceTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    if (UsePerStorageKeySalts()) {
      feature_list_.InitWithFeatures(
          {kMediaDeviceIdPartitioning, kMediaDeviceIdRandomSaltsPerStorageKey},
          {});
    } else {
      feature_list_.InitWithFeatures({kMediaDeviceIdPartitioning},
                                     {kMediaDeviceIdRandomSaltsPerStorageKey});
    }
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        &browser_context_);

    user_prefs::UserPrefs::Set(&browser_context_, &pref_service_);
    ASSERT_FALSE(pref_service_.FindPreference(
        media_device_salt::prefs::kMediaDeviceIdSalt));

    MediaDeviceIDSalt::RegisterProfilePrefs(pref_service_.registry());
    ASSERT_TRUE(pref_service_.FindPreference(
        media_device_salt::prefs::kMediaDeviceIdSalt));

    service_ = std::make_unique<MediaDeviceSaltService>(&pref_service_,
                                                        base::FilePath());
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&browser_context_);
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  const sync_preferences::TestingPrefServiceSyncable& pref_service() const {
    return pref_service_;
  }
  MediaDeviceSaltService* service() const { return service_.get(); }

  bool UsePerStorageKeySalts() const { return GetParam(); }

  std::string GetSalt(const blink::StorageKey& storage_key) const {
    base::test::TestFuture<const std::string&> future;
    service_->GetSalt(storage_key, future.GetCallback());
    return future.Get();
  }

  void DeleteSaltUsingMatcher(const blink::StorageKey& storage_key) const {
    base::test::TestFuture<void> future;
    auto matcher = base::BindLambdaForTesting(
        [&storage_key](const blink::StorageKey& candidate_key) {
          return candidate_key == storage_key;
        });
    service_->DeleteSalts(base::Time::Min(), base::Time::Max(), matcher,
                          future.GetCallback());
    CHECK(future.Wait());
  }

  void DeleteSalts(const std::vector<blink::StorageKey>& keys) {
    base::test::TestFuture<void> future;
    auto matcher = base::BindLambdaForTesting(
        [&keys](const blink::StorageKey& candidate_key) {
          return base::Contains(keys, candidate_key);
        });
    service_->DeleteSalts(base::Time::Min(), base::Time::Max(), matcher,
                          future.GetCallback());
    CHECK(future.Wait());
  }

  void DeleteSaltsInTimeRange(base::Time delete_begin, base::Time delete_end) {
    base::test::TestFuture<void> future;
    service_->DeleteSalts(delete_begin, delete_end, StorageKeyMatcher(),
                          future.GetCallback());
    CHECK(future.Wait());
  }

  void DeleteSingleSalt(const blink::StorageKey& storage_key) {
    base::test::TestFuture<void> future;
    service_->DeleteSalt(storage_key, future.GetCallback());
    CHECK(future.Wait());
  }

  std::vector<blink::StorageKey> GetAllStorageKeys() {
    base::test::TestFuture<std::vector<blink::StorageKey>> future;
    service_->GetAllStorageKeys(future.GetCallback());
    return future.Get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  content::TestBrowserContext browser_context_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<MediaDeviceSaltService> service_;
};

TEST_P(MediaDeviceSaltServiceTest, ResetGlobalSaltFiresDeviceChange) {
  feature_list().Reset();
  feature_list().InitAndDisableFeature(kMediaDeviceIdPartitioning);
  base::SystemMonitor monitor;
  ASSERT_EQ(base::SystemMonitor::Get(), &monitor);
  testing::StrictMock<base::MockDevicesChangedObserver> observer;
  monitor.AddDevicesChangedObserver(&observer);

  EXPECT_CALL(observer, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO));
  EXPECT_CALL(observer,
              OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE));
  service()->DeleteSalts(base::Time::Min(), base::Time::Max(),
                         StorageKeyMatcher(), base::DoNothing());
  task_environment().RunUntilIdle();

  monitor.RemoveDevicesChangedObserver(&observer);
}

TEST_P(MediaDeviceSaltServiceTest, DeleteSingleSaltUsingMatcher) {
  // Deletion of individual salts is not supported when using the global salt.
  std::string salt1 = GetSalt(StorageKey1());
  std::string salt2 = GetSalt(StorageKey2());
  EXPECT_FALSE(salt1.empty());
  EXPECT_EQ(salt1 != salt2, UsePerStorageKeySalts());

  DeleteSaltUsingMatcher(StorageKey1());
  std::string salt1b = GetSalt(StorageKey1());
  std::string salt2b = GetSalt(StorageKey2());
  EXPECT_FALSE(salt1b.empty());
  EXPECT_EQ(salt1 != salt1b, UsePerStorageKeySalts());
  EXPECT_EQ(salt2b, salt2);

  DeleteSaltUsingMatcher(StorageKey2());
  std::string salt1c = GetSalt(StorageKey1());
  std::string salt2c = GetSalt(StorageKey2());
  EXPECT_EQ(salt1c, salt1b);
  EXPECT_EQ(salt2c != salt2b, UsePerStorageKeySalts());
  EXPECT_EQ(salt2c != salt1c, UsePerStorageKeySalts());
}

TEST_P(MediaDeviceSaltServiceTest, DeleteMultipleSaltsUsingMatcher) {
  // Deletion of individual salts is not supported when using the global salt.
  std::string salt1 = GetSalt(StorageKey1());
  std::string salt2 = GetSalt(StorageKey2());
  std::string salt3 = GetSalt(StorageKey3());

  DeleteSalts({StorageKey1(), StorageKey2()});
  std::string salt1b = GetSalt(StorageKey1());
  std::string salt2b = GetSalt(StorageKey2());
  std::string salt3b = GetSalt(StorageKey3());

  EXPECT_EQ(salt1 != salt1b, UsePerStorageKeySalts());
  EXPECT_EQ(salt2 != salt2b, UsePerStorageKeySalts());
  EXPECT_EQ(salt3, salt3b);
}

TEST_P(MediaDeviceSaltServiceTest, DeleteSingleSalt) {
  // Deletion of individual salts is not supported when using the global salt.
  std::string salt1 = GetSalt(StorageKey1());
  std::string salt2 = GetSalt(StorageKey2());
  EXPECT_FALSE(salt1.empty());
  EXPECT_EQ(salt1 != salt2, UsePerStorageKeySalts());

  DeleteSingleSalt(StorageKey1());
  std::string salt1b = GetSalt(StorageKey1());
  std::string salt2b = GetSalt(StorageKey2());
  EXPECT_FALSE(salt1b.empty());
  EXPECT_EQ(salt1 != salt1b, UsePerStorageKeySalts());
  EXPECT_EQ(salt2b, salt2);

  DeleteSingleSalt(StorageKey2());
  std::string salt1c = GetSalt(StorageKey1());
  std::string salt2c = GetSalt(StorageKey2());
  EXPECT_EQ(salt1c, salt1b);
  EXPECT_EQ(salt2c != salt2b, UsePerStorageKeySalts());
  EXPECT_EQ(salt2c != salt1c, UsePerStorageKeySalts());
}

TEST_P(MediaDeviceSaltServiceTest, DeleteSaltsInTimeRange) {
  // Deletion of salts by time range is supported with both the global and
  // per-storage-key salts.
  base::Time time1 = base::Time::Now();
  std::string salt1 = GetSalt(StorageKey1());
  std::string salt2 = GetSalt(StorageKey2());
  base::Time time3 = base::Time::Now();
  std::string salt3 = GetSalt(StorageKey3());
  base::Time time4 = base::Time::Now();

  // No salts deleted
  DeleteSaltsInTimeRange(time4, base::Time::Max());
  std::string salt1b = GetSalt(StorageKey1());
  std::string salt2b = GetSalt(StorageKey2());
  std::string salt3b = GetSalt(StorageKey3());
  EXPECT_EQ(salt1b, salt1);
  EXPECT_EQ(salt2b, salt2);
  EXPECT_EQ(salt3b, salt3);

  // Only salt for StorageKey3 deleted
  DeleteSaltsInTimeRange(time3, base::Time::Max());
  std::string salt1c = GetSalt(StorageKey1());
  std::string salt2c = GetSalt(StorageKey2());
  std::string salt3c = GetSalt(StorageKey3());
  EXPECT_EQ(salt1c, salt1b);
  EXPECT_EQ(salt2c, salt2b);
  EXPECT_NE(salt3c, salt3b);

  // Salts for StorageKey1 and StorageKey2 deleted
  DeleteSaltsInTimeRange(time1, time3);
  std::string salt1d = GetSalt(StorageKey1());
  std::string salt2d = GetSalt(StorageKey2());
  std::string salt3d = GetSalt(StorageKey3());
  EXPECT_NE(salt1d, salt1c);
  EXPECT_NE(salt2d, salt2c);
  EXPECT_EQ(salt3d, salt3c);

  // All salts deleted
  DeleteSaltsInTimeRange(base::Time::Min(), base::Time::Max());
  std::string salt1e = GetSalt(StorageKey1());
  std::string salt2e = GetSalt(StorageKey2());
  std::string salt3e = GetSalt(StorageKey3());
  EXPECT_NE(salt1e, salt1d);
  EXPECT_NE(salt2e, salt2d);
  EXPECT_NE(salt3e, salt3d);
}

TEST_P(MediaDeviceSaltServiceTest, GetAllStorageKeys) {
  // Deletion of individual salts is not supported when using the global salt.
  std::string salt1 = GetSalt(StorageKey1());
  std::string salt2 = GetSalt(StorageKey2());
  std::string salt3 = GetSalt(StorageKey3());
  EXPECT_THAT(
      GetAllStorageKeys(),
      UnorderedElementsAre(StorageKey1(), StorageKey2(), StorageKey3()));

  DeleteSingleSalt(StorageKey2());
  EXPECT_THAT(GetAllStorageKeys(),
              UnorderedElementsAre(StorageKey1(), StorageKey3()));

  DeleteSaltsInTimeRange(base::Time::Min(), base::Time::Max());
  EXPECT_THAT(GetAllStorageKeys(), IsEmpty());
}

TEST_P(MediaDeviceSaltServiceTest, OpaqueKey) {
  // Storage keys with opaque origin use an ephemeral global salt.
  std::string salt1 = GetSalt(blink::StorageKey());

  // The fallback salt is reset when all salts are deleted.
  DeleteSaltsInTimeRange(base::Time::Min(), base::Time::Max());
  std::string salt2 = GetSalt(blink::StorageKey());
  EXPECT_NE(salt1, salt2);

  // The fallback salt is not deleted in a matching deletion.
  DeleteSaltUsingMatcher(blink::StorageKey());
  std::string salt3 = GetSalt(blink::StorageKey());
  EXPECT_EQ(salt2, salt3);
}

TEST_P(MediaDeviceSaltServiceTest, ManyGetSalts) {
  const size_t n = 100;
  std::vector<std::string> salts;
  base::RunLoop run_loop;
  for (size_t i = 0; i < n; i++) {
    service()->GetSalt(StorageKey1(),
                       base::BindLambdaForTesting([&](const std::string& salt) {
                         salts.push_back(salt);
                         if (salts.size() == n) {
                           run_loop.Quit();
                         }
                       }));
  }
  run_loop.Run();
  ASSERT_EQ(salts.size(), n);
  EXPECT_FALSE(salts[0].empty());
  for (size_t i = 1; i < n; i++) {
    EXPECT_EQ(salts[i], salts[0]);
  }
}

TEST_P(MediaDeviceSaltServiceTest, DeviceChangeEvent) {
  base::SystemMonitor monitor;
  ASSERT_EQ(base::SystemMonitor::Get(), &monitor);
  testing::StrictMock<base::MockDevicesChangedObserver> observer;
  monitor.AddDevicesChangedObserver(&observer);
  GetSalt(StorageKey1());
  GetSalt(StorageKey2());

  EXPECT_CALL(observer, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO));
  EXPECT_CALL(observer,
              OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE));
  DeleteSaltUsingMatcher(StorageKey1());
  task_environment().RunUntilIdle();

  EXPECT_CALL(observer, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO));
  EXPECT_CALL(observer,
              OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE));
  DeleteSaltsInTimeRange(base::Time::Min(), base::Time::Max());
  task_environment().RunUntilIdle();

  monitor.RemoveDevicesChangedObserver(&observer);
}

INSTANTIATE_TEST_SUITE_P(All, MediaDeviceSaltServiceTest, testing::Bool());
}  // namespace media_device_salt
