// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

constexpr char kPageWithAnchorURL[] = "chrome://internals/user-education";
constexpr char16_t kBubbleBodyText[] = u"bubble body";

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

}  // namespace

using ShowPromoInPageBrowserTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(ShowPromoInPageBrowserTest, FocusesBrowserTabAndAnchor) {
  auto params = GetDefaultParams();
  params.target_url = GURL(kPageWithAnchorURL);
  params.overwrite_active_tab = true;
  // Set the alt text here and then check that aria-label matches.
  params.close_button_alt_text_id = IDS_CLOSE_PROMO;

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

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
      WaitForWebContentsNavigation(kTabId, GURL(kPageWithAnchorURL)),
      WaitForStateChange(kTabId, bubble_is_visible),
      Log("If the CheckViewProperty() step below fails intermittently, then  "
          "there is a race condition and we should change it to a "
          "StateObserver."),

      CheckViewProperty(ContentsWebView::kContentsWebViewElementId,
                        &views::View::HasFocus, true),
      CheckJsResultAt(kTabId, kPathToAnchor, kExpectActiveJs, ""));
}
