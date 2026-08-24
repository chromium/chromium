// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/translate/translate_language_search_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_features.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace translate {

views::BoxLayoutView* GetListView(views::View* search_view) {
  if (search_view->children().size() < 2) {
    return nullptr;
  }
  views::View* scroll_view = search_view->children()[1];
  if (views::IsViewClass<views::ScrollView>(scroll_view)) {
    return views::AsViewClass<views::BoxLayoutView>(
        static_cast<views::ScrollView*>(scroll_view)->contents());
  }
  return nullptr;
}

class TranslateBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  TranslateBubbleViewBrowserTest() = default;

  TranslateBubbleViewBrowserTest(const TranslateBubbleViewBrowserTest&) =
      delete;
  TranslateBubbleViewBrowserTest& operator=(
      const TranslateBubbleViewBrowserTest&) = delete;

  ~TranslateBubbleViewBrowserTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    TranslateManager::SetIgnoreMissingKeyForTesting(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
    // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
    // language detection.
    command_line->AppendSwitch(::switches::kOverrideLanguageDetection);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void NavigateAndWaitForLanguageDetection(const GURL& url,
                                           const std::string& expected_lang) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    while (expected_lang !=
           ChromeTranslateClient::FromWebContents(
               browser()->GetTabStripModel()->GetActiveWebContents())
               ->GetLanguageState()
               .source_language()) {
      CreateTranslateWaiter(
          browser()->GetTabStripModel()->GetActiveWebContents(),
          TranslateWaiter::WaitEvent::kLanguageDetermined)
          ->Wait();
    }
  }

  void SwitchViewForTesting(TranslateBubbleView* bubble,
                            TranslateBubbleModel::ViewState state) {
    bubble->SwitchView(state);
  }

  TranslateLanguageSearchView* GetTranslateLanguageSearchView(
      TranslateBubbleView* bubble) {
    return bubble->translate_language_search_view_;
  }
};

class TranslateBubbleViewBrowserTest_SearchUIDisabled
    : public TranslateBubbleViewBrowserTest {
 public:
  TranslateBubbleViewBrowserTest_SearchUIDisabled() {
    feature_list_.InitAndDisableFeature(translate::kTranslateLanguageSearchUI);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class TranslateBubbleViewBrowserTest_SearchUIEnabled
    : public TranslateBubbleViewBrowserTest {
 public:
  TranslateBubbleViewBrowserTest_SearchUIEnabled() {
    feature_list_.InitAndEnableFeature(translate::kTranslateLanguageSearchUI);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseBrowserWithoutTranslating) {
  EXPECT_FALSE(
      TranslateBubbleController::From(browser())->GetTranslateBubble());

  // Show a French page and wait until the bubble is shown.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  EXPECT_TRUE(bubble);
  views::ViewTracker bubble_tracker(bubble);
  EXPECT_EQ(bubble, bubble_tracker.view());

  // Close the window without translating. Spin the runloop to allow
  // asynchronous window closure to happen.
  chrome::CloseWindow(browser());
  base::RunLoop().RunUntilIdle();

  // Closing the window should close the bubble.
  EXPECT_EQ(nullptr, bubble_tracker.view());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseLastTabWithoutTranslating) {
  EXPECT_FALSE(
      TranslateBubbleController::From(browser())->GetTranslateBubble());

  // Show a French page and wait until the bubble is shown.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  EXPECT_TRUE(bubble);
  views::ViewTracker bubble_tracker(bubble);
  EXPECT_EQ(bubble, bubble_tracker.view());

  // Close the tab without translating. Spin the runloop to allow asynchronous
  // window closure to happen.
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  chrome::CloseWebContents(
      browser(), browser()->GetTabStripModel()->GetActiveWebContents(), false);
  base::RunLoop().RunUntilIdle();

  // Closing the last tab should close the bubble.
  EXPECT_EQ(nullptr, bubble_tracker.view());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseAnotherTabWithoutTranslating) {
  EXPECT_FALSE(
      TranslateBubbleController::From(browser())->GetTranslateBubble());

  int active_index = browser()->GetTabStripModel()->active_index();

  // Open another tab to load a French page on background.
  int french_index = active_index + 1;
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  chrome::AddTabAt(browser(), french_url, french_index, false);
  EXPECT_EQ(active_index, browser()->GetTabStripModel()->active_index());
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetWebContentsAt(french_index);

  // The bubble is not shown because the tab is not activated.
  EXPECT_FALSE(
      TranslateBubbleController::From(browser())->GetTranslateBubble());

  // Close the French page tab immediately.
  chrome::CloseWebContents(browser(), web_contents, false);
  EXPECT_EQ(active_index, browser()->GetTabStripModel()->active_index());
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(
      TranslateBubbleController::From(browser())->GetTranslateBubble());

  // Close the last tab.
  chrome::CloseWebContents(
      browser(), browser()->GetTabStripModel()->GetActiveWebContents(), false);
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  // TODO(crbug.com/40691800): This should produce one event instead of two.
  EXPECT_LT(0, counter.GetCount(ax::mojom::Event::kAlert));
}

class TranslateBubbleVisualTest
    : public SupportsTestDialog<
          TranslateBubbleViewBrowserTest_SearchUIDisabled> {
 public:
  TranslateBubbleVisualTest(const TranslateBubbleVisualTest&) = delete;
  TranslateBubbleVisualTest& operator=(const TranslateBubbleVisualTest&) =
      delete;

 protected:
  TranslateBubbleVisualTest() = default;

  TranslateBubbleView* GetCurrentTranslateBubble() {
    return TranslateBubbleController::From(browser())->GetTranslateBubble();
  }

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override {
    GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
    NavigateAndWaitForLanguageDetection(french_url, "fr");
    DCHECK(GetCurrentTranslateBubble());
    GetCurrentTranslateBubble()->SwitchView(state_);
  }

  void set_state(TranslateBubbleModel::ViewState state) { state_ = state; }

 private:
  TranslateBubbleModel::ViewState state_;
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleVisualTest, InvokeUi_error) {
  set_state(TranslateBubbleModel::ViewState::VIEW_STATE_ERROR);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleVisualTest, InvokeUi_before_translate) {
  set_state(TranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleVisualTest, InvokeUi_select_source) {
  set_state(TranslateBubbleModel::ViewState::VIEW_STATE_SOURCE_LANGUAGE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       OpeningFindBarClosesTranslateBubble) {
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");
  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  EXPECT_TRUE(bubble);

  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  chrome::Find(browser());
  waiter.Wait();

  bubble = TranslateBubbleController::From(browser())->GetTranslateBubble();
  EXPECT_FALSE(bubble);
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest_SearchUIDisabled,
                       SelectTargetLanguageByDisplayName) {
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  ASSERT_TRUE(bubble);

  // Switch to advanced target view
  SwitchViewForTesting(bubble,
                       TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  // Select Spanish by display name
  translate::test_utils::SelectTargetLanguageByDisplayName(browser(),
                                                           u"Spanish");

  EXPECT_EQ(bubble->model()->GetTargetLanguageIndex(),
            static_cast<int>(
                bubble->model()->GetTargetLanguageIndexForCode("es").value()));
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest_SearchUIEnabled,
                       SelectTargetLanguageByDisplayName) {
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  ASSERT_TRUE(bubble);

  // Switch to advanced target view
  SwitchViewForTesting(bubble,
                       TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  // Select Spanish by display name
  translate::test_utils::SelectTargetLanguageByDisplayName(browser(),
                                                           u"Spanish");

  EXPECT_EQ(bubble->model()->GetTargetLanguageIndex(),
            static_cast<int>(
                bubble->model()->GetTargetLanguageIndexForCode("es").value()));
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest_SearchUIEnabled,
                       SelectLanguageClearsListView) {
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  TranslateBubbleView* bubble =
      TranslateBubbleController::From(browser())->GetTranslateBubble();
  ASSERT_TRUE(bubble);

  // Switch to advanced target view (the search view)
  SwitchViewForTesting(bubble,
                       TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  TranslateLanguageSearchView* search_view =
      GetTranslateLanguageSearchView(bubble);
  ASSERT_TRUE(search_view);

  views::BoxLayoutView* list_view = GetListView(search_view);
  ASSERT_TRUE(list_view);

  // Get the first target language in the list view.
  ASSERT_FALSE(list_view->children().empty());
  HoverButton* button_to_click =
      views::AsViewClass<HoverButton>(list_view->children().front());
  ASSERT_TRUE(button_to_click);

  // Click the button.
  views::test::ButtonTestApi(button_to_click)
      .NotifyClick(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                                ui::DomCode::ENTER, ui::EF_NONE));

  // Verify that the list view is now empty.
  EXPECT_TRUE(list_view->children().empty());
}
}  // namespace translate
