// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_service_factory.h"

#include "base/test/test_future.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_device_salt {

class MediaDeviceSaltServiceTest : public testing::Test {
 public:
  void SetUp() override {
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        &browser_context_);

    user_prefs::UserPrefs::Set(&browser_context_, &pref_service_);
    ASSERT_FALSE(pref_service_.FindPreference(
        media_device_salt::prefs::kMediaDeviceIdSalt));

    MediaDeviceIDSalt::RegisterProfilePrefs(pref_service_.registry());
    ASSERT_TRUE(pref_service_.FindPreference(
        media_device_salt::prefs::kMediaDeviceIdSalt));

    MediaDeviceSaltServiceFactory* factory =
        MediaDeviceSaltServiceFactory::GetInstance();
    ASSERT_TRUE(factory);

    service_ = factory->GetForBrowserContext(&browser_context_);
    ASSERT_TRUE(service_);
  }

  void TearDown() override {
    service_ = nullptr;
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&browser_context_);
  }

 protected:
  const sync_preferences::TestingPrefServiceSyncable& pref_service() const {
    return pref_service_;
  }
  MediaDeviceSaltService* service() const { return service_.get(); }

  std::string GetSalt() const {
    base::test::TestFuture<const std::string&> future;
    service_->GetSalt(future.GetCallback());
    return future.Get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  raw_ptr<MediaDeviceSaltService> service_;
};

TEST_F(MediaDeviceSaltServiceTest, GetAndResetSalt) {
  std::string salt1 = GetSalt();
  EXPECT_FALSE(salt1.empty());
  EXPECT_EQ(salt1, pref_service().GetString(
                       media_device_salt::prefs::kMediaDeviceIdSalt));

  service()->ResetSalt();

  std::string salt2 = GetSalt();
  EXPECT_NE(salt1, salt2);
  EXPECT_FALSE(salt2.empty());
  EXPECT_EQ(salt2, pref_service().GetString(
                       media_device_salt::prefs::kMediaDeviceIdSalt));
}

}  // namespace media_device_salt
