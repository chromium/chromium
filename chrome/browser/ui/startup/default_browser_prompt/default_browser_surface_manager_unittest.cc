// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

#include <memory>

#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestDefaultBrowserSurfaceManager : public DefaultBrowserSurfaceManager {
 public:
  TestDefaultBrowserSurfaceManager() = default;
  ~TestDefaultBrowserSurfaceManager() override = default;

  using DefaultBrowserSurfaceManager::IsBrowserValidForShowing;

  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override {
    return default_browser::DefaultBrowserEntrypointType::kStartupInfobar;
  }

  void ShowForBrowser(BrowserWindowInterface* browser) override {}
  void CloseForBrowser(BrowserWindowInterface* browser) override {}
  void CloseAllPromptInstances() override {}
};

class DefaultBrowserSurfaceManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    browser_window_interface_ =
        std::make_unique<::testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, GetType())
        .WillByDefault(::testing::Return(BrowserWindowInterface::TYPE_NORMAL));
  }

  TestingProfile* profile() { return profile_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
};

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForNormalProfile) {
  TestDefaultBrowserSurfaceManager manager;
  EXPECT_TRUE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForIncognitoProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(otr_profile));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest,
       IsBrowserValidForEnterpriseIsolatedMode) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile_metrics::SetBrowserProfileType(
      otr_profile, profile_metrics::BrowserProfileType::kEnterpriseIsolated);
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(otr_profile));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForGuestSession) {
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(guest_profile.get()));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForNonNormalWindow) {
  ON_CALL(*browser_window_interface(), GetType())
      .WillByDefault(::testing::Return(BrowserWindowInterface::TYPE_POPUP));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}
