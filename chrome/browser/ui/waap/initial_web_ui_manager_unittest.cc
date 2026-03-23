// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

class InitialWebUIManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

  content::BrowserTaskEnvironment task_environment_;
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

}  // namespace
