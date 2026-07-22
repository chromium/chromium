// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace pdf::infobar {

class PdfInfoBarDelegateTest : public testing::Test {
 protected:
  PdfInfoBarDelegateTest() {
    feature_list_.InitAndDisableFeature(features::kPdfInfoBar);
  }

  void SetUp() override {
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents_.get());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  ChromeLayoutProvider layout_provider_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_{
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(&profile_))};
};

// Executes the code to ensure that creating the infobar doesn't crash. When the
// infobar is created, the "shown" histogram should be recorded.
TEST_F(PdfInfoBarDelegateTest, Create) {
  EXPECT_TRUE(PdfInfoBarDelegate::Create(infobar_manager()));
  histogram_tester().ExpectUniqueSample("PDF.InfoBar.Shown", true, 1);
}

// When the infobar is dismissed, the "dismissed" histogram should be recorded.
// "Accept" is not tested here because it opens an OS dialog.
TEST_F(PdfInfoBarDelegateTest, DismissedHistogram) {
  infobars::InfoBar* infobar = PdfInfoBarDelegate::Create(infobar_manager());
  static_cast<PdfInfoBarDelegate*>(infobar->delegate())->InfoBarDismissed();
  histogram_tester().ExpectUniqueSample(
      "PDF.InfoBar.UserInteraction", PdfInfoBarUserInteraction::kDismissed, 1);
}

// When the infobar is destroyed without being accepted or dismissed, the
// "ignored" histogram should be recorded.
TEST_F(PdfInfoBarDelegateTest, IgnoredHistogram) {
  infobars::InfoBar* infobar = PdfInfoBarDelegate::Create(infobar_manager());
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectUniqueSample("PDF.InfoBar.UserInteraction",
                                        PdfInfoBarUserInteraction::kIgnored, 1);
}

// When the infobar is destroyed after being dismissed, the "dismissed"
// histogram (not the "ignored" histogram) should be recorded.
TEST_F(PdfInfoBarDelegateTest, DismissedHistogramInfoBarDestroyed) {
  infobars::InfoBar* infobar = PdfInfoBarDelegate::Create(infobar_manager());
  static_cast<PdfInfoBarDelegate*>(infobar->delegate())->InfoBarDismissed();
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectUniqueSample(
      "PDF.InfoBar.UserInteraction", PdfInfoBarUserInteraction::kDismissed, 1);
}

class PdfInfoBarDelegateMigratedTest : public testing::Test {
 protected:
  PdfInfoBarDelegateMigratedTest()
      : profile_(std::make_unique<TestingProfile>()),
        delegate_(std::make_unique<TestTabStripModelDelegate>()),
        tab_strip_model_(std::make_unique<TabStripModel>(delegate_.get(),
                                                         profile_.get())),
        browser_window_interface_(
            std::make_unique<MockBrowserWindowInterface>()) {
    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(::testing::ReturnRef(unowned_user_data_host_));
    delegate_->SetBrowserWindowInterface(browser_window_interface_.get());

    // Enable kPdfInfoBar feature and centralized framework with migrated PDF.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPdfInfoBar, {}},
         {infobars::kCentralizedInfoBarFramework, {{"MigratedPdf", "true"}}}},
        {});
  }

  ~PdfInfoBarDelegateMigratedTest() override {
    // Break loop so we can deconstruct without dangling pointers.
    delegate_->SetBrowserWindowInterface(nullptr);
  }

  void AddTab() {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    tab_strip_model_->AppendWebContents(std::move(web_contents), true);
  }

  void SetUp() override {
    PdfInfoBarController::SetHigherPriorityInfoBarShownForTesting(false);
    AddTab();
    infobars::ContentInfoBarManager::CreateForWebContents(
        tab_strip_model_->GetActiveWebContents());

    // `BrowserInfoBarManager::From()` will return `nullptr` (false) if a global
    // `BrowserInfoBarManager` has not yet been instantiated for the testing
    // browser process (e.g., if this is the first test setup in the process, or
    // if it was previously cleared in `TearDown()`). In that case, we
    // instantiate and own one for the duration of the test.
    if (!infobars::BrowserInfoBarManager::From(
            TestingBrowserProcess::GetGlobal())) {
      owned_browser_infobar_manager_ =
          std::make_unique<infobars::BrowserInfoBarManager>(
              TestingBrowserProcess::GetGlobal());
    }
    // Controller must be created after the managers are ready.
    controller_ =
        std::make_unique<PdfInfoBarController>(browser_window_interface_.get());

    ASSERT_TRUE(infobar_manager());
    ShowInfoBar(*controller());
  }

  void TearDown() override {
    auto* manager = infobar_manager();
    if (manager) {
      while (!manager->infobars().empty()) {
        manager->RemoveInfoBar(manager->infobars()[0]);
      }
    }
    // Controller must be destroyed while managers and mock browser are still
    // alive to avoid raw_ptr check failures during destruction of the fixture.
    controller_.reset();
    owned_browser_infobar_manager_.reset();
    delegate_->SetBrowserWindowInterface(nullptr);
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

  MockBrowserWindowInterface* browser_window_interface() {

    return browser_window_interface_.get();

  }
  TestingProfile* profile() { return profile_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  PdfInfoBarController* controller() { return controller_.get(); }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        tab_strip_model()->GetActiveWebContents());
  }

  void ShowInfoBar(PdfInfoBarController& controller) {
    controller.MaybeShowInfoBarCallback(
        shell_integration::DefaultWebClientState::NOT_DEFAULT);
  }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  ChromeLayoutProvider layout_provider_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabStripModelDelegate> delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  std::unique_ptr<infobars::BrowserInfoBarManager>
      owned_browser_infobar_manager_;
  // Controller must be declared after browser_window_interface_ to ensure
  // correct destruction order.
  std::unique_ptr<PdfInfoBarController> controller_;
};

TEST_F(PdfInfoBarDelegateMigratedTest, Create) {
  histogram_tester().ExpectUniqueSample("PDF.InfoBar.Shown", true, 1);
}

// When the infobar is dismissed, the "dismissed" histogram should be recorded
// under the new path.
TEST_F(PdfInfoBarDelegateMigratedTest, DismissedHistogram) {
  ASSERT_EQ(1u, infobar_manager()->infobars().size());
  infobars::InfoBar* infobar = infobar_manager()->infobars()[0];

  infobar->delegate()->InfoBarDismissed();

  histogram_tester().ExpectUniqueSample(
      "PDF.InfoBar.UserInteraction", PdfInfoBarUserInteraction::kDismissed, 1);
}

// When the infobar is destroyed without being accepted or dismissed, the
// "ignored" histogram should be recorded under the new path.
TEST_F(PdfInfoBarDelegateMigratedTest, IgnoredHistogram) {
  ASSERT_EQ(1u, infobar_manager()->infobars().size());
  infobars::InfoBar* infobar = infobar_manager()->infobars()[0];

  infobar_manager()->RemoveInfoBar(infobar);

  histogram_tester().ExpectUniqueSample("PDF.InfoBar.UserInteraction",
                                        PdfInfoBarUserInteraction::kIgnored, 1);
}

// When the infobar is destroyed after being dismissed, the "dismissed"
// histogram (not the "ignored" histogram) should be recorded under the new
// path.
TEST_F(PdfInfoBarDelegateMigratedTest, DismissedHistogramInfoBarDestroyed) {
  ASSERT_EQ(1u, infobar_manager()->infobars().size());
  infobars::InfoBar* infobar = infobar_manager()->infobars()[0];

  infobar->delegate()->InfoBarDismissed();
  infobar_manager()->RemoveInfoBar(infobar);

  histogram_tester().ExpectUniqueSample(
      "PDF.InfoBar.UserInteraction", PdfInfoBarUserInteraction::kDismissed, 1);
}

}  // namespace pdf::infobar
