// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service_factory.h"

#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserManagerServiceFactoryTest : public testing::Test {
 public:
  BrowserManagerServiceFactoryTest() = default;
  ~BrowserManagerServiceFactoryTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserManagerServiceFactoryTest,
       ValidServiceWhenBrowserWindowsDisallowed) {
  TestingProfile::Builder builder;
  builder.DisallowBrowserWindows();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_FALSE(profile->AllowsBrowserWindows());

  BrowserManagerService* service =
      BrowserManagerServiceFactory::GetForProfile(profile.get());
  EXPECT_TRUE(service);
}

TEST_F(BrowserManagerServiceFactoryTest, ValidServiceForRegularProfile) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  BrowserManagerService* service =
      BrowserManagerServiceFactory::GetForProfile(profile.get());
  EXPECT_TRUE(service);
}

TEST_F(BrowserManagerServiceFactoryTest, ValidServiceForIncognitoProfile) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  TestingProfile::Builder incognito_builder;
  TestingProfile* incognito_profile =
      incognito_builder.BuildIncognito(profile.get());

  BrowserManagerService* service =
      BrowserManagerServiceFactory::GetForProfile(incognito_profile);
  EXPECT_TRUE(service);
}

TEST_F(BrowserManagerServiceFactoryTest, ValidServiceForGuestProfile) {
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  BrowserManagerService* service =
      BrowserManagerServiceFactory::GetForProfile(profile.get());
  EXPECT_TRUE(service);
}

TEST_F(BrowserManagerServiceFactoryTest, ValidServiceForNonPrimaryOTRProfile) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  TestingProfile::Builder otr_builder;
  Profile::OTRProfileID otr_profile_id =
      Profile::OTRProfileID::CreateUniqueForTesting();
  TestingProfile* otr_profile =
      otr_builder.BuildOffTheRecord(profile.get(), otr_profile_id);

  BrowserManagerService* service =
      BrowserManagerServiceFactory::GetForProfile(otr_profile);
  EXPECT_TRUE(service);
}
