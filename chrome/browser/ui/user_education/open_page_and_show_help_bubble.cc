// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/open_page_and_show_help_bubble.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

constexpr base::TimeDelta kOpenPageAndShowHelpBubbleTimeout = base::Seconds(30);

class OpenPageAndShowHelpBubbleImpl : public OpenPageAndShowHelpBubble {
 public:
  explicit OpenPageAndShowHelpBubbleImpl(Browser* browser, Params params)
      : browser_(browser->AsWeakPtr()), callback_(std::move(params.callback)) {
    DCHECK(callback_);
    DCHECK(browser_);
    DCHECK(!params.bubble_text.empty());

    base::TimeDelta timeout = params.timeout_override_for_testing.value_or(
        kOpenPageAndShowHelpBubbleTimeout);
    DCHECK(timeout.is_positive());

    bubble_params_.body_text = params.bubble_text;
    bubble_params_.arrow = params.bubble_arrow;

    anchor_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementShownInAnyContextCallback(
                params.bubble_anchor_id,
                base::BindRepeating(
                    &OpenPageAndShowHelpBubbleImpl::OnAnchorShown,
                    base::Unretained(this)));

    NavigateParams navigate_params(browser, params.target_url,
                                   ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition =
        params.overwrite_active_tab ? WindowOpenDisposition::CURRENT_TAB
                                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    navigate_handle_ = Navigate(&navigate_params);

    timeout_.Start(FROM_HERE, timeout,
                   base::BindOnce(&OpenPageAndShowHelpBubbleImpl::OnTimeout,
                                  base::Unretained(this)));
  }

  ~OpenPageAndShowHelpBubbleImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  base::WeakPtr<OpenPageAndShowHelpBubbleImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  user_education::HelpBubble* GetHelpBubbleForTesting() override {
    return help_bubble_.get();
  }

 private:
  void OnAnchorShown(ui::TrackedElement* anchor_element) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    anchor_subscription_ = base::CallbackListSubscription();
    navigate_handle_.reset();
    timeout_.AbandonAndStop();

    // It's possible that the browser window was closed and somehow the tab
    // opened in another window. It's an edge case but an important one since a
    // HelpBubbleFactoryRegistry is needed to create the help bubble.
    if (browser_) {
      auto* const factory =
          static_cast<user_education::FeaturePromoControllerCommon*>(
              browser_->window()->GetFeaturePromoController())
              ->bubble_factory_registry();
      help_bubble_ =
          factory->CreateHelpBubble(anchor_element, std::move(bubble_params_));
      DCHECK(help_bubble_);
    }

    if (!help_bubble_) {
      std::move(callback_).Run(this, false);
      delete this;
      return;
    }
    help_bubble_closed_subscription_ =
        help_bubble_->AddOnCloseCallback(base::BindOnce(
            &OpenPageAndShowHelpBubbleImpl::OnBubbleClosed, GetWeakPtr()));
    std::move(callback_).Run(this, true);
  }

  void OnBubbleClosed(user_education::HelpBubble* help_bubble) { delete this; }

  void OnTimeout() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    anchor_subscription_ = base::CallbackListSubscription();
    navigate_handle_.reset();
    std::move(callback_).Run(this, false);
    delete this;
  }

  const base::WeakPtr<Browser> browser_;
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  base::CallbackListSubscription help_bubble_closed_subscription_;
  user_education::HelpBubbleParams bubble_params_;
  Callback callback_;
  base::WeakPtr<content::NavigationHandle> navigate_handle_;
  base::CallbackListSubscription anchor_subscription_;
  base::OneShotTimer timeout_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<OpenPageAndShowHelpBubbleImpl> weak_ptr_factory_{this};
};

}  // namespace

OpenPageAndShowHelpBubble::Params::Params() = default;
OpenPageAndShowHelpBubble::Params::~Params() = default;
OpenPageAndShowHelpBubble::Params::Params(Params&& other) = default;
OpenPageAndShowHelpBubble::Params& OpenPageAndShowHelpBubble::Params::operator=(
    Params&& other) = default;

OpenPageAndShowHelpBubble::OpenPageAndShowHelpBubble() = default;
OpenPageAndShowHelpBubble::~OpenPageAndShowHelpBubble() = default;

base::WeakPtr<OpenPageAndShowHelpBubble> OpenPageAndShowHelpBubble::Start(
    Browser* browser,
    Params params) {
  return (new OpenPageAndShowHelpBubbleImpl(browser, std::move(params)))
      ->GetWeakPtr();
}
