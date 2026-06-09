// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/user_education/user_education_types.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Javascript that verifies that `el` is the active element in its document or
// shadow DOM. Returns the empty string on success, or an error message on
// failure.
constexpr char kExpectActiveJs[] = R"(
  (el) => {
    // Find the containing document or shadow DOM.
    let root = null;
    for (let parent = el; parent; parent = parent.parentNode) {
      if (parent instanceof Document || parent instanceof ShadowRoot) {
        root = parent;
        break;
      }
    }
    if (!root) {
      return 'Root not found.';
    }
    if (!root.activeElement) {
      return 'No active element.';
    }
    if (root.activeElement !== el) {
      return 'Active element is ' + root.activeElement.tagName + '#' +
          root.activeElement.id;
    }
    return '';
  }
)";

constexpr char kPageWithoutAnchorURL[] = "chrome://settings";
constexpr char16_t kBubbleBodyText[] = u"bubble body";

// This should be short enough that tests that *expect* the operation to time
// out should fail. Standard test timeout will be used for tests expected to
// succeed.
constexpr base::TimeDelta kShortTimeoutForTesting = base::Seconds(3);

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleShownEvent);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

}  // namespace

class ShowPromoInPageUiTest : public InteractiveBrowserTest {
 public:
  ShowPromoInPageUiTest() = default;
  ~ShowPromoInPageUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

  // Gets a partially-filled params block with default values. You will still
  // need to specify `target_url` and `callback`.
  ShowPromoInPage::Params GetDefaultParams() {
    ShowPromoInPage::Params params;
    params.bubble_anchor_id = kWebUIIPHDemoElementIdentifier;
    params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
    params.bubble_text = kBubbleBodyText;
    params.timeout_override_for_testing = base::Seconds(90);
    params.callback = base::BindLambdaForTesting(
        [this](ShowPromoInPage* source, bool success) {
          bubble_shown_ = success;
          BrowserElements::From(browser())->NotifyEvent(kBrowserViewElementId,
                                                        kBubbleShownEvent);
        });
    return params;
  }

  auto ShowPromo(
      std::optional<GURL> url,
      std::optional<user_education::PageOpenMode> open_mode = std::nullopt,
      std::optional<base::TimeDelta> timeout = std::nullopt) {
    return WithView(kBrowserViewElementId,
                    [=, this](BrowserView* browser_view) {
                      auto params = GetDefaultParams();
                      params.target_url = url;
                      if (open_mode) {
                        params.page_open_mode = *open_mode;
                      }
                      if (timeout) {
                        params.timeout_override_for_testing = *timeout;
                      }
                      handle_ = ShowPromoInPage::Start(browser_view->browser(),
                                                       std::move(params));
                    })
        .SetDescription("ShowPromo()");
  }

  auto WaitForBubble(bool expected_success) {
    auto steps =
        Steps(WaitForEvent(kBrowserViewElementId, kBubbleShownEvent),
              CheckResult([this] { return *bubble_shown_; }, expected_success,
                          "Callback indicates success or failure."),
              CheckResult([this] { return !!handle_.get(); }, expected_success,
                          "Handle validity tracks success or failure."));

    if (expected_success) {
      steps += Check(
          [this] {
            return handle_->GetHelpBubbleForTesting() &&
                   handle_->GetHelpBubbleForTesting()->is_open();
          },
          "Bubble exists and is open.");
    }

    AddDescriptionPrefix(steps, "WaitForBubbleShown()");
    return steps;
  }

  auto ExpectTabCount(int count) {
    return CheckResult([this] { return browser()->tab_strip_model()->count(); },
                       count, "Check tab strip count.");
  }

  auto ExpectWindowCount(int count) {
    return CheckResult([] { return GetAllBrowserWindowInterfaces().size(); },
                       count, "ExpectWindowCount()");
  }

  auto CheckHandleIsValid(bool expected) {
    return CheckResult([this] { return !!handle_; }, expected,
                       "Check handle is valid or not.");
  }

  auto CloseBubble() {
    auto steps = Steps(
        Check([this] { return handle_ && handle_->GetHelpBubbleForTesting(); },
              "Check bubble was open."),
        Do([this] {
          handle_->GetHelpBubbleForTesting()->Close(
              user_education::HelpBubble::CloseReason::kProgrammaticallyClosed);
        }),
        CheckHandleIsValid(false));
    AddDescriptionPrefix(steps, "CloseBubble()");
    return steps;
  }

  auto CheckActiveTabIs(const GURL& url,
                        std::optional<int> index = std::nullopt) {
    auto steps = Steps(CheckView(
                           kBrowserViewElementId,
                           [](BrowserView* browser_view) {
                             return browser_view->browser()
                                 ->tab_strip_model()
                                 ->GetActiveTab()
                                 ->GetContents()
                                 ->GetURL();
                           },
                           url)
                           .SetDescription("CheckActiveTabIs()"));
    if (index) {
      steps += CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            return browser_view->browser()->tab_strip_model()->active_index();
          },
          *index);
    }
    AddDescriptionPrefix(steps, "CheckActiveTabIs()");
    return steps;
  }

 private:
  base::WeakPtr<ShowPromoInPage> handle_;
  std::optional<bool> bubble_shown_;
};

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, ShowPromoInNewPage) {
  RunTestSequence(ShowPromo(GURL(chrome::kChromeUIUserEducationInternalsURL)),
                  WaitForBubble(true), ExpectTabCount(2), ExpectWindowCount(1),
                  CloseBubble());
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, ShowPromoInNewWindow) {
  const GURL url(chrome::kChromeUIUserEducationInternalsURL);
  Browser* const other = CreateBrowser(browser()->profile());
  RunTestSequence(SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Window activation is iffy on Linux with Wayland."),
                  InContext(BrowserElements::From(other)->GetContext(),
                            ActivateSurface(kBrowserViewElementId)),
                  ShowPromo(url), WaitForBubble(true),
                  // The operation should have activated and opened a single tab
                  // in the browser.
                  ExpectWindowCount(2), CheckActiveTabIs(url),
#if !BUILDFLAG(IS_LINUX)
                  // On Linux, programmatic activation may not be supported.
                  CheckView(kBrowserViewElementId,
                            [](BrowserView* browser_view) {
                              return browser_view->IsActive();
                            }),
#endif
                  CloseBubble());
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, ShowPromoInSameTab) {
  RunTestSequence(ShowPromo(GURL(chrome::kChromeUIUserEducationInternalsURL),
                            user_education::PageOpenMode::kOverwriteActiveTab),
                  WaitForBubble(true), ExpectTabCount(1), ExpectWindowCount(1),
                  CloseBubble());
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, ShowPromoInSamePage) {
  const GURL url(chrome::kChromeUIUserEducationInternalsURL);
  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
      ExpectTabCount(1), ShowPromo(std::nullopt), WaitForBubble(true),
      // The bubble should appear in the current tab without navigating it.
      ExpectTabCount(1), ExpectWindowCount(1), CheckActiveTabIs(url),
      CloseBubble());
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest,
                       ShowPromoInPageFailureCallbackOnTimeout) {
  RunTestSequence(ShowPromo(GURL(kPageWithoutAnchorURL), std::nullopt,
                            kShortTimeoutForTesting),
                  WaitForBubble(false), ExpectTabCount(2),
                  ExpectWindowCount(1));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, DestroyBrowserBeforeComplete) {
  RunTestSequence(
      ShowPromo(GURL(kPageWithoutAnchorURL), std::nullopt,
                kShortTimeoutForTesting),
      WaitForBubble(false), Do([this] { browser()->GetWindow()->Close(); }),
      WaitForHide(kBrowserViewElementId), CheckHandleIsValid(false));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest,
                       DestroyBrowserWhileBubbleVisible) {
  RunTestSequence(
      ShowPromo(GURL(chrome::kChromeUIUserEducationInternalsURL)),
      WaitForBubble(true), Do([this] { browser()->GetWindow()->Close(); }),
      WaitForHide(kBrowserViewElementId), CheckHandleIsValid(false));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest,
                       HelpBubbleParamsCanConfigureCloseButtonAltText) {
  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.page_open_mode = user_education::PageOpenMode::kOverwriteActiveTab;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);

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

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, ShowPromoInSingletonTab) {
  const GURL target_url(chrome::kChromeUIUserEducationInternalsURL);
  const GURL other_url(chrome::kChromeUISettingsURL);

  RunTestSequence(
      AddInstrumentedTab(kTabId, target_url),
      AddInstrumentedTab(kOtherTabId, other_url), ExpectTabCount(3),
      SelectTab(kTabStripElementId, 2), CheckActiveTabIs(other_url, 2),
      ShowPromo(target_url, user_education::PageOpenMode::kSingletonTab),
      WaitForBubble(true),
      // No new tabs should have been opened.
      ExpectTabCount(3),
      // The promo should have opened in the already-open tab.
      CheckActiveTabIs(target_url, 1));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest, FocusesBrowserTabAndAnchor) {
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.page_open_mode = user_education::PageOpenMode::kOverwriteActiveTab;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);

  auto help_bubble_start_callback =
      base::BindOnce(base::IgnoreResult(&ShowPromoInPage::Start), browser(),
                     std::move(params));
  static const DeepQuery kPathToAnchor = {"user-education-internals",
                                          "#IPH_WebUiHelpBubbleTest"};
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_is_visible;
  bubble_is_visible.event = kBubbleIsVisible;
  bubble_is_visible.where = kPathToHelpBubbleCloseButton;
  bubble_is_visible.type = StateChange::Type::kExists;
  RunTestSequence(
      InstrumentTab(kTabId),
      Do([this]() { browser()->window()->SetFocusToLocationBar(true); }),
      Do(std::move(help_bubble_start_callback)),
      WaitForWebContentsNavigation(
          kTabId, GURL(chrome::kChromeUIUserEducationInternalsURL)),
      WaitForStateChange(kTabId, bubble_is_visible),
      Log("If the CheckViewProperty() step below fails intermittently, then  "
          "there is a race condition and we should change it to a "
          "StateObserver."),

      CheckViewProperty(ContentsWebView::kContentsWebViewElementId,
                        &views::View::HasFocus, true),
      CheckJsResultAt(kTabId, kPathToAnchor, kExpectActiveJs, ""));
}

IN_PROC_BROWSER_TEST_F(ShowPromoInPageUiTest,
                       FocusesBrowserTabAndAnchorInSingletonTab) {
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  auto params = GetDefaultParams();
  params.target_url = GURL(chrome::kChromeUIUserEducationInternalsURL);
  params.page_open_mode = user_education::PageOpenMode::kSingletonTab;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);

  auto help_bubble_start_callback =
      base::BindOnce(base::IgnoreResult(&ShowPromoInPage::Start), browser(),
                     std::move(params));
  static const DeepQuery kPathToAnchor = {"user-education-internals",
                                          "#IPH_WebUiHelpBubbleTest"};
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_is_visible;
  bubble_is_visible.event = kBubbleIsVisible;
  bubble_is_visible.where = kPathToHelpBubbleCloseButton;
  bubble_is_visible.type = StateChange::Type::kExists;
  RunTestSequence(
      InstrumentNextTab(kTabId),
      Do([this]() { browser()->window()->SetFocusToLocationBar(true); }),
      Do(std::move(help_bubble_start_callback)),
      WaitForWebContentsNavigation(
          kTabId, GURL(chrome::kChromeUIUserEducationInternalsURL)),
      WaitForStateChange(kTabId, bubble_is_visible),
      Log("If the CheckViewProperty() step below fails intermittently, then  "
          "there is a race condition and we should change it to a "
          "StateObserver."),

      CheckViewProperty(ContentsWebView::kContentsWebViewElementId,
                        &views::View::HasFocus, true),
      CheckJsResultAt(kTabId, kPathToAnchor, kExpectActiveJs, ""));
}
