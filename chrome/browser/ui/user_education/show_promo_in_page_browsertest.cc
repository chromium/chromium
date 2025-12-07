// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/show_promo_in_page.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kPageWithoutAnchorURL[] = "chrome://settings";
constexpr char16_t kBubbleBodyText[] = u"bubble body";

// This should be short enough that tests that *expect* the operation to time
// out should fail. Standard test timeout will be used for tests expected to
// succeed.
constexpr base::TimeDelta kShortTimeoutForTesting = base::Seconds(3);

// Gets a partially-filled params block with default values. You will still
// need to specify `target_url` and `callback`.
ShowPromoInPage::Params GetDefaultParams() {
  ShowPromoInPage::Params params;
  params.bubble_anchor_id = kWebUIIPHDemoElementIdentifier;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
  params.bubble_text = kBubbleBodyText;
  params.timeout_override_for_testing = base::Seconds(90);
  return params;
}

}  // namespace

class ShowPromoInPageBrowserTest : public InteractiveBrowserTest {
 public:
  ShowPromoInPageBrowserTest() = default;
  ~ShowPromoInPageBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, ShowPromoInNewPage) {
  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.callback = bubble_shown.Get();

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // The operation should have opened a second tab in the browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  ASSERT_NE(nullptr, handle->GetHelpBubbleForTesting());
  ASSERT_TRUE(handle->GetHelpBubbleForTesting()->is_open());

  // Closing the help bubble should destroy the object.
  handle->GetHelpBubbleForTesting()->Close();
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, ShowPromoInNewWindow) {
  Profile* profile = browser()->profile();
  CloseAllBrowsers();

  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.callback = bubble_shown.Get();

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ASSERT_FALSE(browser->window()->IsActive());
  handle = ShowPromoInPage::Start(browser, std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // The operation should have activated and opened a single tab in the browser.
#if !BUILDFLAG(IS_LINUX)
  // On Linux, programmatic activation may not be supported.
  EXPECT_TRUE(browser->window()->IsActive());
#endif
  EXPECT_EQ(1, browser->tab_strip_model()->count());

  ASSERT_NE(nullptr, handle->GetHelpBubbleForTesting());
  ASSERT_TRUE(handle->GetHelpBubbleForTesting()->is_open());

  // Closing the help bubble should destroy the object.
  handle->GetHelpBubbleForTesting()->Close();
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, ShowPromoInSameTab) {
  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.callback = bubble_shown.Get();
  params.page_open_mode = user_education::PageOpenMode::kOverwriteActiveTab;

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // The operation should have re-used the active tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ASSERT_NE(nullptr, handle->GetHelpBubbleForTesting());
  ASSERT_TRUE(handle->GetHelpBubbleForTesting()->is_open());

  // Closing the help bubble should destroy the object.
  handle->GetHelpBubbleForTesting()->Close();
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, ShowPromoInSamePage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIUserEducationInternalsURL)));

  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.callback = bubble_shown.Get();

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // The operation should not have changed the active tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ASSERT_NE(nullptr, handle->GetHelpBubbleForTesting());
  ASSERT_TRUE(handle->GetHelpBubbleForTesting()->is_open());

  // Closing the help bubble should destroy the object.
  handle->GetHelpBubbleForTesting()->Close();
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest,
                       ShowPromoInPageFailureCallbackOnTimeout) {
  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithoutAnchorURL);
  params.callback = bubble_shown.Get();
  params.timeout_override_for_testing = kShortTimeoutForTesting;

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_FALSE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // On failure the object is destroyed immediately.
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest,
                       DestroyBrowserBeforeComplete) {
  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithoutAnchorURL);
  params.callback = bubble_shown.Get();
  params.timeout_override_for_testing = kShortTimeoutForTesting;

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_FALSE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  CloseAllBrowsers();

  run_loop.Run();

  // On failure the object is destroyed immediately.
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest,
                       HelpBubbleParamsCanConfigureCloseButtonAltText) {
  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.page_open_mode = user_education::PageOpenMode::kOverwriteActiveTab;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  auto help_bubble_start_callback =
      base::BindOnce(base::IgnoreResult(&ShowPromoInPage::Start), browser(),
                     std::move(params));
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_is_visible;
  bubble_is_visible.event = kBubbleIsVisible;
  bubble_is_visible.where = kPathToHelpBubbleCloseButton;
  bubble_is_visible.type = StateChange::Type::kExists;
  RunTestSequence(InstrumentTab(kTabId),
                  Do(std::move(help_bubble_start_callback)),
                  WaitForWebContentsNavigation(
                      kTabId, GURL(chrome::kChromeUIUserEducationInternalsURL)),
                  WaitForStateChange(kTabId, bubble_is_visible),
                  CheckJsResultAt(kTabId, kPathToHelpBubbleCloseButton,
                                  "(el) => el.getAttribute('aria-label')",
                                  l10n_util::GetStringUTF8(IDS_CLOSE_PROMO)));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, ShowPromoInSingletonTab) {
  // Open 2 tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIUserEducationInternalsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Open a promo in a singleton tab.
  base::MockCallback<ShowPromoInPage::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.callback = bubble_shown.Get();
  params.page_open_mode = user_education::PageOpenMode::kSingletonTab;

  base::WeakPtr<ShowPromoInPage> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](ShowPromoInPage* source, bool success) {
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = ShowPromoInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // No new tabs should have been opened.
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  // The promo should have opened in the already-open tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  ASSERT_NE(nullptr, handle->GetHelpBubbleForTesting());
  ASSERT_TRUE(handle->GetHelpBubbleForTesting()->is_open());

  // Closing the help bubble should destroy the object.
  handle->GetHelpBubbleForTesting()->Close();
  ASSERT_FALSE(handle);
}
