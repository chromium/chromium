// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/open_page_and_show_help_bubble.h"

#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

constexpr char kPageWithAnchorURL[] = "chrome://internals/user-education";
constexpr char kPageWithoutAnchorURL[] = "chrome://settings";
constexpr char16_t kBubbleBodyText[] = u"bubble body";
constexpr ui::ElementIdentifier kHelpBubbleAnchorId =
    kWebUIIPHDemoElementIdentifier;

// This should be short enough that tests that *expect* the operation to time
// out should fail. Standard test timeout will be used for tests expected to
// succeed.
constexpr base::TimeDelta kTimeoutForTesting = base::Seconds(3);

// Gets a partially-filled params block with default values. You will still
// need to specify `target_url` and `callback`.
OpenPageAndShowHelpBubble::Params GetDefaultParams() {
  OpenPageAndShowHelpBubble::Params params;
  params.bubble_anchor_id = kHelpBubbleAnchorId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
  params.bubble_text = kBubbleBodyText;
  return params;
}

}  // namespace

using OpenPageAndShowHelpBubbleBrowserTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(OpenPageAndShowHelpBubbleBrowserTest,
                       OpenPageAndDisplayHelpBubbleInNewPage) {
  base::MockCallback<OpenPageAndShowHelpBubble::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithAnchorURL);
  params.callback = bubble_shown.Get();

  base::WeakPtr<OpenPageAndShowHelpBubble> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](OpenPageAndShowHelpBubble* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = OpenPageAndShowHelpBubble::Start(browser(), std::move(params));

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

IN_PROC_BROWSER_TEST_F(OpenPageAndShowHelpBubbleBrowserTest,
                       OpenPageAndDisplayHelpBubbleInSameTab) {
  base::MockCallback<OpenPageAndShowHelpBubble::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithAnchorURL);
  params.callback = bubble_shown.Get();
  params.overwrite_active_tab = true;

  base::WeakPtr<OpenPageAndShowHelpBubble> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](OpenPageAndShowHelpBubble* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_TRUE(success);
        quit_closure.Run();
      });

  handle = OpenPageAndShowHelpBubble::Start(browser(), std::move(params));

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

IN_PROC_BROWSER_TEST_F(OpenPageAndShowHelpBubbleBrowserTest,
                       OpenPageAndDisplayHelpBubbleFailureCallbackOnTimeout) {
  base::MockCallback<OpenPageAndShowHelpBubble::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithoutAnchorURL);
  params.callback = bubble_shown.Get();
  params.timeout_override_for_testing = kTimeoutForTesting;

  base::WeakPtr<OpenPageAndShowHelpBubble> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](OpenPageAndShowHelpBubble* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_FALSE(success);
        quit_closure.Run();
      });

  handle = OpenPageAndShowHelpBubble::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  run_loop.Run();

  // On failure the object is destroyed immediately.
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(OpenPageAndShowHelpBubbleBrowserTest,
                       DestroyBrowserBeforeComplete) {
  base::MockCallback<OpenPageAndShowHelpBubble::Callback> bubble_shown;

  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithoutAnchorURL);
  params.callback = bubble_shown.Get();
  params.timeout_override_for_testing = kTimeoutForTesting;

  base::WeakPtr<OpenPageAndShowHelpBubble> handle;

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_CALL(bubble_shown, Run)
      .WillOnce([&](OpenPageAndShowHelpBubble* source, bool success) {
        EXPECT_EQ(handle.get(), source);
        EXPECT_FALSE(success);
        quit_closure.Run();
      });

  handle = OpenPageAndShowHelpBubble::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);

  CloseAllBrowsers();

  run_loop.Run();

  // On failure the object is destroyed immediately.
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(OpenPageAndShowHelpBubbleBrowserTest,
                       HelpBubbleParamsCanConfigureCloseButtonAltText) {
  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithAnchorURL);
  params.overwrite_active_tab = true;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  auto help_bubble_start_callback =
      base::BindOnce(base::IgnoreResult(&OpenPageAndShowHelpBubble::Start),
                     browser(), std::move(params));
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_is_visible;
  bubble_is_visible.event = kBubbleIsVisible;
  bubble_is_visible.where = kPathToHelpBubbleCloseButton;
  bubble_is_visible.type = StateChange::Type::kExists;
  RunTestSequence(
      InstrumentTab(kTabId), Do(std::move(help_bubble_start_callback)),
      WaitForWebContentsNavigation(kTabId, GURL(kPageWithAnchorURL)),
      WaitForStateChange(kTabId, bubble_is_visible),
      CheckJsResultAt(kTabId, kPathToHelpBubbleCloseButton,
                      "(el) => el.getAttribute('aria-label')",
                      l10n_util::GetStringUTF8(IDS_CLOSE_PROMO)));
}
