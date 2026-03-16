// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

class PinInfoBarControllerCrashTest : public testing::Test {
 protected:
  PinInfoBarControllerCrashTest()
      : profile_(std::make_unique<TestingProfile>()),
        delegate_(std::make_unique<TestTabStripModelDelegate>()),
        tab_strip_model_(
            std::make_unique<TabStripModel>(delegate_.get(), profile_.get())),
        browser_window_interface_(
            std::make_unique<MockBrowserWindowInterface>()) {
    feature_list_.InitAndEnableFeature(features::kOfferPinToTaskbarInfoBar);

    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, GetType())
        .WillByDefault(
            ::testing::Return(BrowserWindowInterface::Type::TYPE_NORMAL));
    delegate_->SetBrowserWindowInterface(browser_window_interface_.get());
  }

  ~PinInfoBarControllerCrashTest() override {
    delegate_->SetBrowserWindowInterface(nullptr);
  }

  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ChromeLayoutProvider layout_provider_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
  const std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

// Reproduce the crash in b/491643039: OnShouldOfferToPinResult called when
// there are no active web contents.
TEST_F(PinInfoBarControllerCrashTest, ReproduceCrashWithNoWebContents) {
  PinInfoBarController controller(browser_window_interface());

  base::RunLoop run_loop;
  bool callback_called = false;
  controller.OnShouldOfferToPinResult(
      base::BindLambdaForTesting([&](bool result) {
        callback_called = true;
        EXPECT_FALSE(result);
        run_loop.Quit();
      }),
      /*should_offer_to_pin=*/true);
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

}  // namespace default_browser
