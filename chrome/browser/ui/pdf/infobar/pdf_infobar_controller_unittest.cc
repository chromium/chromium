// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class PdfInfoBarControllerTest : public testing::Test {
 protected:
  PdfInfoBarControllerTest()
      : profile_(std::make_unique<TestingProfile>()),
        delegate_(std::make_unique<TestTabStripModelDelegate>()),
        tab_strip_model_(
            std::make_unique<TabStripModel>(delegate(), profile())),
        browser_window_interface_(
            std::make_unique<MockBrowserWindowInterface>()) {
    ON_CALL(*browser_window_interface_, GetTabStripModel)
        .WillByDefault(::testing::Return(tab_strip_model()));
    ON_CALL(*browser_window_interface_, GetProfile)
        .WillByDefault(::testing::Return(profile()));
    delegate_->SetBrowserWindowInterface(browser_window_interface());
  }

  ~PdfInfoBarControllerTest() override {
    // Break loop so we can deconstruct without dangling pointers.
    delegate_->SetBrowserWindowInterface(nullptr);
  }

  void AddTab() {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    tab_strip_model_->AppendWebContents(std::move(web_contents), true);
  }

  void SetUp() override {
    AddTab();
    infobars::ContentInfoBarManager::CreateForWebContents(
        tab_strip_model_->GetActiveWebContents());
  }

  void SetBrowserType(BrowserWindowInterface::Type type) {
    ON_CALL(*browser_window_interface_, GetType)
        .WillByDefault(::testing::Return(type));
  }

  // Calls `MaybeShowInfoBarCallback()` with the given `default_state` and
  // `browser_type`. Returns true if the infobar was shown, false if not.
  bool DidShowInfoBar(BrowserWindowInterface::Type browser_type,
                      shell_integration::DefaultWebClientState default_state) {
    auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
        tab_strip_model()->GetActiveWebContents());
    CHECK(infobar_manager);
    CHECK(infobar_manager->infobars().empty());
    SetBrowserType(browser_type);

    PdfInfoBarController controller{browser_window_interface()};
    controller.MaybeShowInfoBarCallback(default_state);

    // Return true if an infobar was created.
    bool did_show_infobar = !infobar_manager->infobars().empty();
    if (did_show_infobar) {
      // Clear the infobar to ensure `controller` stops observing
      // `infobar_manager` before being destroyed.
      infobar_manager->RemoveAllInfoBars(false);
    }
    return did_show_infobar;
  }

  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }
  TestingProfile* profile() { return profile_.get(); }
  TestTabStripModelDelegate* delegate() { return delegate_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;

  // Must be before `profile_`.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

  // `ChromeLayoutProvider::Get()` is called when an infobar is created.
  ChromeLayoutProvider layout_provider_;

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
  const std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  tabs::PreventTabFeatureInitialization prevent_;
};

class PdfInfoBarControllerPdfLoadTest : public PdfInfoBarControllerTest {
  void SetUp() override {
    feature_list().InitAndEnableFeatureWithParameters(
        features::kPdfInfoBar, {{"trigger", "pdf-load"}});
    PdfInfoBarControllerTest::SetUp();
  }
};

// Register to be called when the active tab changes if the browser is normal.
TEST_F(PdfInfoBarControllerPdfLoadTest, RegisterActiveTabDidChangeNormal) {
  SetBrowserType(BrowserWindowInterface::Type::TYPE_NORMAL);
  EXPECT_CALL(*(browser_window_interface()), RegisterActiveTabDidChange);
  PdfInfoBarController controller{browser_window_interface()};
}

// Don't register for active tab changes if the browser is not a normal type.
TEST_F(PdfInfoBarControllerPdfLoadTest, RegisterActiveTabDidChangeApp) {
  SetBrowserType(BrowserWindowInterface::Type::TYPE_APP);
  EXPECT_CALL(*(browser_window_interface()), RegisterActiveTabDidChange)
      .Times(0);
  PdfInfoBarController controller{browser_window_interface()};
}

class PdfInfoBarControllerStartupTest : public PdfInfoBarControllerTest {
  void SetUp() override {
    feature_list().InitAndEnableFeatureWithParameters(features::kPdfInfoBar,
                                                      {{"trigger", "startup"}});
    PdfInfoBarController::SetDefaultBrowserPromptShownForTesting(false);
    PdfInfoBarControllerTest::SetUp();
  }
};

// Show the infobar if the feature is enabled, the browser type is normal, and
// Chrome is not the default PDF viewer.
TEST_F(PdfInfoBarControllerStartupTest, MaybeShowInfoBarCallback) {
  EXPECT_TRUE(
      DidShowInfoBar(BrowserWindowInterface::Type::TYPE_NORMAL,
                     shell_integration::DefaultWebClientState::NOT_DEFAULT));
}

// Don't show the infobar if Chrome is already the default PDF viewer.
TEST_F(PdfInfoBarControllerStartupTest, DontShowInfoBarIfChromeIsDefault) {
  EXPECT_FALSE(
      DidShowInfoBar(BrowserWindowInterface::Type::TYPE_NORMAL,
                     shell_integration::DefaultWebClientState::IS_DEFAULT));
}

// Don't show the infobar if another Chrome channel is the default PDF viewer.
TEST_F(PdfInfoBarControllerStartupTest,
       DontShowInfoBarIfAnotherChromeIsDefault) {
  EXPECT_FALSE(DidShowInfoBar(
      BrowserWindowInterface::Type::TYPE_NORMAL,
      shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT));
}

// Don't show the infobar if Chrome's PDF viewer is disabled.
TEST_F(PdfInfoBarControllerStartupTest, DontShowInfoBarIfPdfViewerDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    true);
  EXPECT_FALSE(
      DidShowInfoBar(BrowserWindowInterface::Type::TYPE_NORMAL,
                     shell_integration::DefaultWebClientState::NOT_DEFAULT));
}

// Don't show the infobar if default apps are managed by a policy.
TEST_F(PdfInfoBarControllerStartupTest,
       DontShowInfoBarIfDefaultIsPolicyControlled) {
  local_state()->SetManagedPref(prefs::kDefaultBrowserSettingEnabled,
                                base::Value(true));
  EXPECT_FALSE(
      DidShowInfoBar(BrowserWindowInterface::Type::TYPE_NORMAL,
                     shell_integration::DefaultWebClientState::NOT_DEFAULT));
}

TEST_F(PdfInfoBarControllerStartupTest,
       DontShowInfoBarIfDefaultBrowserPromptShown) {
  PdfInfoBarController::SetDefaultBrowserPromptShownForTesting(true);
  EXPECT_FALSE(
      DidShowInfoBar(BrowserWindowInterface::Type::TYPE_NORMAL,
                     shell_integration::DefaultWebClientState::NOT_DEFAULT));
}
