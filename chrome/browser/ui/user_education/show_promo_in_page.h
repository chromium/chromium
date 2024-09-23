// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_SHOW_PROMO_IN_PAGE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_SHOW_PROMO_IN_PAGE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace user_education {
class HelpBubble;
}  // namespace user_education

class Browser;

// Utility for opening a page (optionally) and showing a help bubble on a
// predetermined element. The object exists only as long as the operation
// is in progress or the help bubble is visible. If a `target_url` is
// defined in the parameters, this page will be opened. If `target_url`
// is omitted, the current page will be used to look for the anchor.
//
// Example:
//
//    // Open security settings page and show a help bubble promoting the
//    // "Enhanced Protection" user setting.
//    ShowPromoInPage::Params params;
//
//    // This WebUI page must be instrumented for help bubbles; see
//    // components/user_education/webui/README.md
//    params.target_url = "chrome://settings/security";
//
//    // This specifies what will be shown in the help bubble and how and where
//    // it will be positioned.
//    params.bubble_anchor_id = kEnhancedSecuritySettingElementId;
//    params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
//    params.bubble_text =
//        l10n_util::GetStringUTF16(IDS_ENABLE_ENHANCED_PROTECTION_PROMO);
//
//    // Log whenever a user actually sees the promo.
//    params.callback = base::BindOnce(&::MaybeLogEnhancedSecurityPromoShown);
//
//    // Shows the page and the bubble. Because we did not specify
//    // |params.overwrite_active_tab|, the settings page will be opened in a
//    // new tab in |browser_| rather than the current tab.
//    ShowPromoInPage::Start(browser_, std::move(params));
//
class ShowPromoInPage {
 public:
  // Called when the help bubble is shown or if it fails/times out. On failure,
  // `source` is destroyed immediately after this call.
  using Callback =
      base::OnceCallback<void(ShowPromoInPage* source, bool success)>;

  // Specifies how a page should be open to show a help bubble.
  struct Params {
    Params();
    Params(Params&& other) noexcept;
    Params& operator=(Params&& other) noexcept;
    ~Params();

    // The page to open. If not specified, the current page will be used.
    std::optional<GURL> target_url = std::nullopt;

    // Whether the page should open in the current active tab. Default is false
    // and should not be set to true unless this action is triggered from the
    // active tab (lest the user lose whatever page they had open).
    //
    // For example, clicking on a dialog offering to take the user to a settings
    // page should not overwrite the current active tab (which could be e.g.
    // their email!) But a button on one settings page that will take the user
    // to a different settings page could set this to true so the navigation
    // happens in the settings page itself.
    bool overwrite_active_tab = false;

    // The element to anchor to. See //components/user_education/webui/README.md
    // for an explanation on how to instrument a WebUI page for Help Bubbles and
    // how to associated a ui::ElementIdentifier with an HTML element.
    ui::ElementIdentifier bubble_anchor_id;

    // How the help bubble should anchor to the anchor element.
    user_education::HelpBubbleArrow bubble_arrow;

    // The text of the help bubble.
    std::u16string bubble_text;

    // The id of the text that should be used for the accessibility label of the
    // help bubble's close button. The localized string will be looked up using
    // this identifier.
    std::optional<int> close_button_alt_text_id;

    // Callback that notifies whether the page loaded and the bubble was
    // displayed successfully. Typically used for testing or metrics collection.
    // If the page load is aborted or the anchor not found within a reasonable
    // amount of time, is called with `success` = false.
    Callback callback = base::DoNothing();

    // Overrides the default timeout for the bubble to be shown; only use for
    // integration testing.
    std::optional<base::TimeDelta> timeout_override_for_testing;
  };

  ShowPromoInPage(const ShowPromoInPage&) = delete;
  virtual ~ShowPromoInPage();
  void operator=(const ShowPromoInPage&) = delete;

  // Opens the page in `browser` and displays a Help Bubble as described by
  // `params`. This method must be called on the UI thread.
  static base::WeakPtr<ShowPromoInPage> Start(Browser* browser, Params params);

  // Returns the help bubble if one was created.
  virtual user_education::HelpBubble* GetHelpBubbleForTesting() = 0;

 protected:
  ShowPromoInPage();
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_SHOW_PROMO_IN_PAGE_H_
