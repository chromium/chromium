// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_service.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

class TabDeclutterServiceTest : public BrowserWithTestWindowTest {
 public:
  TabDeclutterServiceTest() = default;
  TabDeclutterServiceTest(const TabDeclutterServiceTest&) = delete;
  TabDeclutterServiceTest& operator=(const TabDeclutterServiceTest&) = delete;

  TestingProfile* profile() { return profile_.get(); }
  TabDeclutterService* service() { return service_.get(); }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<TabDeclutterService>();
  }
  void TearDown() override {}

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TabDeclutterService> service_;
};

// Service Factory tests.

TEST_F(TabDeclutterServiceTest, DifferentSessionPerProfile) {
  std::unique_ptr<TestingProfile> profile_1 =
      std::make_unique<TestingProfile>();
  std::unique_ptr<TestingProfile> profile_2 =
      std::make_unique<TestingProfile>();

  TabDeclutterService* service_1 =
      TabDeclutterServiceFactory::GetForProfile(profile_1.get());
  TabDeclutterService* service_2 =
      TabDeclutterServiceFactory::GetForProfile(profile_2.get());

  EXPECT_NE(service_1, service_2);
}

TEST_F(TabDeclutterServiceTest, NoIncognito) {
  TestingProfile::Builder incognito_builder;
  TestingProfile* incognito_profile =
      incognito_builder.BuildIncognito(profile());
  TabDeclutterService* incognito_service =
      TabDeclutterServiceFactory::GetForProfile(incognito_profile);

  EXPECT_EQ(incognito_service, nullptr);
}
