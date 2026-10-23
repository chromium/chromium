// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/show_promo_in_page.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/user_education/user_education_types.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

constexpr base::TimeDelta kShowPromoInPageTimeout = base::Seconds(30);

// The implementation for ShowPromoInPage.
//
// This object does the following:
//  - (optionally) opens the page
//  - finds the anchor element at the appropriate time
//  - displays the bubble
//
// On any of these events, the object deletes itself:
//  - the timeout is reached before the bubble is shown
//  - the browser is destroyed
//  - the bubble fails to be created
//  - the bubble is closed
//
// Furthermore the callback is called when:
//  - the bubble is successfully shown
//  - any of the failure modes above is reached
//
class ShowPromoInPageImpl : public ShowPromoInPage {
 public:
  explicit ShowPromoInPageImpl(BrowserWindowInterface* browser, Params params)
      : browser_(browser->GetWeakPtr()),
        target_url_(params.target_url),
        page_open_mode_(params.page_open_mode),
        bubble_anchor_id_(params.bubble_anchor_id),
        timeout_time_(params.timeout_override_for_testing.value_or(
            kShowPromoInPageTimeout)),
        callback_(std::move(params.callback)) {
    CHECK(browser_, base::NotFatalUntil::M162);
    CHECK(bubble_anchor_id_, base::NotFatalUntil::M162);
    CHECK(timeout_time_.is_positive(), base::NotFatalUntil::M162);
    CHECK(callback_, base::NotFatalUntil::M162);
    CHECK(!params.bubble_text.empty(), base::NotFatalUntil::M162);

    bubble_params_.body_text = params.bubble_text;
    bubble_params_.arrow = params.bubble_arrow;
    bubble_params_.focus_on_show_hint = false;
    if (params.close_button_alt_text_id) {
      bubble_params_.close_button_alt_text =
          l10n_util::GetStringUTF16(params.close_button_alt_text_id.value());
    }
  }

  ~ShowPromoInPageImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  base::WeakPtr<ShowPromoInPageImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  user_education::HelpBubble* GetHelpBubbleForTesting() override {
    return help_bubble_.get();
  }

  // Shows the page or immediately starts the promo if there's no URL and the
  // element is already present.
  void Show() {
    CHECK(browser_);

    if (target_url_.has_value()) {
      StartWaitingForAnchor();

      // Callbacks during Navigate() could conceivably delete `this`, so guard
      // whenever calling out to those methods.
      const auto weak_this = GetWeakPtr();
      NavigateParams navigate_params(browser_.get(), target_url_.value(),
                                     ui::PAGE_TRANSITION_LINK);
      navigate_params.disposition =
          user_education::GetWindowOpenDisposition(page_open_mode_);
      navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
      auto navigate_handle = Navigate(&navigate_params);
      if (weak_this) {
        navigate_handle_ = navigate_handle;
      }
      return;
    }

    // Maybe the element is already visible in the current page?
    if (auto* const visible_element =
            ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
                bubble_anchor_id_)) {
      OnAnchorShown(visible_element);
    } else {
      StartWaitingForAnchor();
    }
  }

 private:
  // Start watching the anchor and set the timeout.
  void StartWaitingForAnchor() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    anchor_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementShownInAnyContextCallback(
                bubble_anchor_id_,
                base::BindRepeating(&ShowPromoInPageImpl::OnAnchorShown,
                                    base::Unretained(this)));

    timeout_timer_.Start(FROM_HERE, timeout_time_,
                         base::BindOnce(&ShowPromoInPageImpl::OnTimeout,
                                        base::Unretained(this)));
  }

  void OnAnchorShown(ui::TrackedElement* anchor_element) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    anchor_subscription_ = base::CallbackListSubscription();
    navigate_handle_.reset();
    timeout_timer_.Stop();

    // It's possible that the browser window was closed and somehow the tab
    // opened in another window. It's an edge case but an important one since a
    // HelpBubbleFactoryRegistry is needed to create the help bubble.
    if (!browser_) {
      return OnFailureDeletesThis();
    }

    auto& factory = UserEducationServiceFactory::GetForBrowserContext(
                        browser_->GetProfile())
                        ->help_bubble_factory_registry();
    help_bubble_ =
        factory.CreateHelpBubble(anchor_element, std::move(bubble_params_));
    DUMP_WILL_BE_CHECK(help_bubble_)
        << "A help bubble should always be able to be created, so the anchor "
           "is likely not a supported type or something else has gone wrong "
           "with the UI.";
    if (!help_bubble_) {
      return OnFailureDeletesThis();
    }

    // When the bubble is closed, this object self-terminates.
    help_bubble_closed_subscription_ = help_bubble_->AddOnClosedCallback(
        base::BindOnce(&ShowPromoInPageImpl::OnBubbleClosed, GetWeakPtr()));

    // Call the success callback. While it's extremely unlikely, it's possible
    // the callback could cause the help bubble to close, which would delete
    // `this`.
    const auto weak_this = GetWeakPtr();
    std::move(callback_).Run(this, true);
    if (!weak_this) {
      return;
    }

    // Maybe focus the web contents containing the bubble (if it's the main
    // contents).
    if (auto* const webui_bubble =
            help_bubble_->AsA<user_education::HelpBubbleWebUI>()) {
      if (browser_->GetActiveTabInterface() &&
          browser_->GetActiveTabInterface()->GetContents() ==
              webui_bubble->GetWebContents()) {
        BrowserFocusController::From(browser_.get())->FocusWebContentsPane();
      }
    }
  }

  void OnBubbleClosed(user_education::HelpBubble::CloseReason) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    delete this;
  }

  void OnTimeout() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    anchor_subscription_ = base::CallbackListSubscription();
    navigate_handle_.reset();
    return OnFailureDeletesThis();
  }

  // Calls the callback and deletes `this`. Should always exit the current
  // method after calling.
  void OnFailureDeletesThis() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (callback_) {
      std::move(callback_).Run(this, false);
    }
    delete this;
  }

  const base::WeakPtr<BrowserWindowInterface> browser_;
  const std::optional<GURL> target_url_;
  const user_education::PageOpenMode page_open_mode_;
  const ui::ElementIdentifier bubble_anchor_id_;
  const base::TimeDelta timeout_time_;
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  base::CallbackListSubscription help_bubble_closed_subscription_;
  user_education::HelpBubbleParams bubble_params_;
  Callback callback_;
  base::WeakPtr<content::NavigationHandle> navigate_handle_;
  base::CallbackListSubscription anchor_subscription_;
  base::OneShotTimer timeout_timer_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ShowPromoInPageImpl> weak_ptr_factory_{this};
};

}  // namespace

ShowPromoInPage::Params::Params() = default;
ShowPromoInPage::Params::Params(Params&& other) noexcept = default;
ShowPromoInPage::Params& ShowPromoInPage::Params::operator=(
    Params&& other) noexcept = default;
ShowPromoInPage::Params::~Params() = default;

ShowPromoInPage::ShowPromoInPage() = default;
ShowPromoInPage::~ShowPromoInPage() = default;

base::WeakPtr<ShowPromoInPage> ShowPromoInPage::Start(
    BrowserWindowInterface* browser,
    Params params) {
  auto result =
      (new ShowPromoInPageImpl(browser, std::move(params)))->GetWeakPtr();
  result->Show();
  return result;
}
