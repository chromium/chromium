// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

class InitialWebUIManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(browser_window_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(testing::Const(browser_window_), GetProfile())
        .WillRepeatedly(testing::Return(profile()));
  }

  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(InitialWebUIManagerTest, RequestDeferShow_ReloadButtonEnabledWithoutDeferral) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kInitialWebUI, {}},
       {features::kWebUIReloadButton,
        {{"WebUIReloadButtonDeferBrowserViewShow", "false"}}}},
      {});

  InitialWebUIManager manager(&browser_window_);
  EXPECT_EQ(manager.RequestDeferShow(base::DoNothing()), false);
}

TEST_F(InitialWebUIManagerTest, RequestDeferShow_ReloadButtonEnabledWithDeferral) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kInitialWebUI, {}},
       {features::kWebUIReloadButton,
        {{"WebUIReloadButtonDeferBrowserViewShow", "true"}}}},
      {});

  InitialWebUIManager manager(&browser_window_);
  EXPECT_EQ(manager.RequestDeferShow(base::DoNothing()), true);
}

TEST_F(InitialWebUIManagerTest, RequestDeferShow_ReloadButtonDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kInitialWebUI},
                                       {features::kWebUIReloadButton});

  InitialWebUIManager manager(&browser_window_);
  EXPECT_EQ(manager.RequestDeferShow(base::DoNothing()), false);
}

void InitFeaturesForRefill(base::test::ScopedFeatureList& feature_list,
                           bool refill_enabled) {
  std::vector<base::test::FeatureRefAndParams> enabled_features = {
      {features::kInitialWebUI, {}},
      {features::kWebUIReloadButton,
       {{"WebUIReloadButtonPrewarmWebUI", "true"},
        {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
        {"WebUIReloadButtonProfilePrewarming", "true"}}}};
  std::vector<base::test::FeatureRef> disabled_features;
  if (refill_enabled) {
    enabled_features.emplace_back(features::kTopChromeWebUIPrerendering,
                                  base::FieldTrialParams());
  } else {
    disabled_features.push_back(features::kTopChromeWebUIPrerendering);
  }
  feature_list.InitWithFeaturesAndParameters(enabled_features,
                                             disabled_features);
}

class InitialWebUIManagerRefillTest : public InitialWebUIManagerTest {
 public:
  explicit InitialWebUIManagerRefillTest(bool refill_enabled) {
    InitFeaturesForRefill(scoped_feature_list_, refill_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class InitialWebUIManagerRefillOnTest : public InitialWebUIManagerRefillTest {
 public:
  InitialWebUIManagerRefillOnTest() : InitialWebUIManagerRefillTest(true) {}
};

class InitialWebUIManagerRefillOffTest : public InitialWebUIManagerRefillTest {
 public:
  InitialWebUIManagerRefillOffTest() : InitialWebUIManagerRefillTest(false) {}
};

TEST_F(InitialWebUIManagerRefillOnTest,
       RefillOnNoSpare_HoldsNoPrecreatedContents) {
  auto* profile_service =
      InitialWebUIProfileServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(profile_service);
  ASSERT_FALSE(profile_service->has_toolbar_contents_for_testing());

  InitialWebUIManager manager(&browser_window_);
  EXPECT_FALSE(manager.GetToolbarContentsForTesting());
  EXPECT_FALSE(manager.TakeToolbarContents());
}

TEST_F(InitialWebUIManagerRefillOffTest,
       RefillOffNoSpare_PerWindowPrewarmPrecreates) {
  auto* profile_service =
      InitialWebUIProfileServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(profile_service);
  // Consume the profile spare created in the constructor so none is available.
  EXPECT_TRUE(profile_service->TakeToolbarContents());
  ASSERT_FALSE(profile_service->has_toolbar_contents_for_testing());

  InitialWebUIManager manager(&browser_window_);
  EXPECT_TRUE(manager.GetToolbarContentsForTesting());
  std::unique_ptr<content::WebContents> contents =
      manager.TakeToolbarContents();
  EXPECT_TRUE(contents);
}

}  // namespace
