// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_prefs.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

namespace {

// Helper function that invokes `MaybeShowInfoBarForBrowser` and waits for it to
// run a "done" callback. Returns the value passed to the callback.
bool MaybeShowInfoBarForBrowserAndWait(
    base::WeakPtr<BrowserWindowInterface> browser,
    bool another_infobar_shown) {
  bool did_show_infobar = false;
  base::RunLoop run_loop;
  auto done_callback = base::BindLambdaForTesting([&](bool value) {
    did_show_infobar = value;
    run_loop.Quit();
  });
  PinInfoBarController::MaybeShowInfoBarForBrowser(
      browser, std::move(done_callback), another_infobar_shown);
  run_loop.Run();
  return did_show_infobar;
}

// Helper function that calls `OnShouldOfferToPinResult`, waits for it to run,
// and returns the result.
bool OnShouldOfferToPinResultAndWait(PinInfoBarController& controller,
                                     bool should_offer_to_pin) {
  bool did_show_infobar = false;
  base::RunLoop run_loop;
  auto done_callback = base::BindLambdaForTesting([&](bool value) {
    did_show_infobar = value;
    run_loop.Quit();
  });
  controller.OnShouldOfferToPinResult(std::move(done_callback),
                                      should_offer_to_pin);
  run_loop.Run();
  return did_show_infobar;
}

}  // namespace

class PinInfoBarControllerTest : public testing::Test {
 protected:
  PinInfoBarControllerTest()
      : profile_(std::make_unique<TestingProfile>()),
        delegate_(std::make_unique<TestTabStripModelDelegate>()),
        tab_strip_model_(
            std::make_unique<TabStripModel>(delegate_.get(), profile())),
        browser_window_interface_(
            std::make_unique<MockBrowserWindowInterface>()) {
    feature_list_.InitAndEnableFeature(features::kOfferPinToTaskbarInfoBar);

    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile()));
    delegate_->SetBrowserWindowInterface(browser_window_interface());
  }

  ~PinInfoBarControllerTest() override {
    // Break loop so we can deconstruct without dangling pointers.
    delegate_->SetBrowserWindowInterface(nullptr);
  }

  void SetUp() override {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    tab_strip_model_->AppendWebContents(std::move(web_contents), true);
    infobars::ContentInfoBarManager::CreateForWebContents(
        tab_strip_model_->GetActiveWebContents());
  }

  void SetBrowserType(BrowserWindowInterface::Type type) {
    ON_CALL(*browser_window_interface_, GetType)
        .WillByDefault(::testing::Return(type));
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  TestingProfile* profile() { return profile_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList feature_list_;

  // `ChromeLayoutProvider::Get()` is called when an infobar is created.
  ChromeLayoutProvider layout_provider_;

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
  const std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

// Don't show the infobar if another infobar was already shown.
TEST_F(PinInfoBarControllerTest, DontShowIfAnotherInfoBarShown) {
  EXPECT_FALSE(MaybeShowInfoBarForBrowserAndWait(
      browser_window_interface()->GetWeakPtr(),
      /*another_infobar_shown=*/true));
}

// `MaybeShowInfoBarForBrowser()` should handle a null browser.
TEST_F(PinInfoBarControllerTest, DontCrashIfBrowserNull) {
  EXPECT_FALSE(
      MaybeShowInfoBarForBrowserAndWait(nullptr,
                                        /*another_infobar_shown=*/false));
}

// Don't show the infobar if the browser type is not normal.
TEST_F(PinInfoBarControllerTest, DontShowIfBrowserNotNormal) {
  SetBrowserType(BrowserWindowInterface::TYPE_APP);
  PinInfoBarController controller(browser_window_interface());
  EXPECT_FALSE(OnShouldOfferToPinResultAndWait(controller,
                                               /*should_offer_to_pin=*/true));
}

// Don't show the infobar if the browser is incognito.
TEST_F(PinInfoBarControllerTest, DontShowIfIncognito) {
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(
          profile()->GetOffTheRecordProfile(Profile::OTRProfileID::PrimaryID(),
                                            /*create_if_needed=*/true)));
  PinInfoBarController controller(browser_window_interface());
  EXPECT_FALSE(OnShouldOfferToPinResultAndWait(controller,
                                               /*should_offer_to_pin=*/true));
}

// Don't show the infobar if this session shouldn't offer to pin to taskbar.
TEST_F(PinInfoBarControllerTest, DontShowIfCantPin) {
  SetBrowserType(BrowserWindowInterface::TYPE_NORMAL);
  PinInfoBarController controller(browser_window_interface());
  EXPECT_FALSE(OnShouldOfferToPinResultAndWait(controller,
                                               /*should_offer_to_pin=*/false));
}

// Don't show the infobar if it was shown recently.
TEST_F(PinInfoBarControllerTest, DontShowIfShownRecently) {
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(infobar_manager);
  ASSERT_TRUE(infobar_manager->infobars().empty());
  SetBrowserType(BrowserWindowInterface::Type::TYPE_NORMAL);

  SetInfoBarShownRecently();

  PinInfoBarController controller{browser_window_interface()};
  EXPECT_FALSE(OnShouldOfferToPinResultAndWait(controller,
                                               /*should_offer_to_pin=*/true));
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

// Don't show the infobar if it was shown the maximum number of times.
//
// Disabled because it's broken on multiple bots and very flaky on every other
// bot it runs on; see https://crbug.com/435215855 for links and more info.
TEST_F(PinInfoBarControllerTest, DISABLED_DontShowIfShownMaxTimes) {
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(infobar_manager);
  ASSERT_TRUE(infobar_manager->infobars().empty());
  SetBrowserType(BrowserWindowInterface::Type::TYPE_NORMAL);

  for (int i = 0; i < kPinInfoBarMaxPromptCount; i++) {
    SetInfoBarShownRecently();
  }

  task_environment().FastForwardBy(base::Days(60));

  PinInfoBarController controller{browser_window_interface()};
  EXPECT_FALSE(OnShouldOfferToPinResultAndWait(controller,
                                               /*should_offer_to_pin=*/true));
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

// Show the infobar if the browser type is normal.
TEST_F(PinInfoBarControllerTest, ShowInfoBar) {
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(infobar_manager);
  ASSERT_TRUE(infobar_manager->infobars().empty());
  SetBrowserType(BrowserWindowInterface::Type::TYPE_NORMAL);

  PinInfoBarController controller{browser_window_interface()};
  EXPECT_TRUE(OnShouldOfferToPinResultAndWait(controller,
                                              /*should_offer_to_pin=*/true));
  EXPECT_FALSE(infobar_manager->infobars().empty());

  // Clear the infobar to ensure `controller` stops observing
  // `infobar_manager` before being destroyed.
  infobar_manager->RemoveAllInfoBars(false);
}

}  // namespace default_browser
