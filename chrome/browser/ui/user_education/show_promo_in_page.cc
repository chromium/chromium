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
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

constexpr base::TimeDelta kShowPromoInPageTimeout = base::Seconds(30);

class ShowPromoInPageImpl : public ShowPromoInPage {
 public:
  explicit ShowPromoInPageImpl(Browser* browser, Params params)
      : browser_(browser->AsWeakPtr()), callback_(std::move(params.callback)) {
    DCHECK(callback_);
    DCHECK(browser_);
    DCHECK(!params.bubble_text.empty());

    base::TimeDelta timeout =
        params.timeout_override_for_testing.value_or(kShowPromoInPageTimeout);
    DCHECK(timeout.is_positive());

    bubble_params_.body_text = params.bubble_text;
    bubble_params_.arrow = params.bubble_arrow;
    bubble_params_.focus_on_show_hint = false;

    if (params.close_button_alt_text_id) {
      bubble_params_.close_button_alt_text =
          l10n_util::GetStringUTF16(params.close_button_alt_text_id.value());
    }

    anchor_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementShownInAnyContextCallback(
                params.bubble_anchor_id,
                base::BindRepeating(&ShowPromoInPageImpl::OnAnchorShown,
                                    base::Unretained(this)));

    if (params.target_url.has_value()) {
      NavigateParams navigate_params(browser, params.target_url.value(),
                                     ui::PAGE_TRANSITION_LINK);
      navigate_params.disposition =
          params.overwrite_active_tab
              ? WindowOpenDisposition::CURRENT_TAB
              : WindowOpenDisposition::NEW_FOREGROUND_TAB;
      navigate_handle_ = Navigate(&navigate_params);
    } else {
      auto* visible_element =
          ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
              params.bubble_anchor_id);

      if (visible_element) {
        OnAnchorShown(visible_element);
      }
    }

    timeout_.Start(FROM_HERE, timeout,
                   base::BindOnce(&ShowPromoInPageImpl::OnTimeout,
                                  base::Unretained(this)));
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
      auto& factory =
          UserEducationServiceFactory::GetForBrowserContext(browser_->profile())
              ->help_bubble_factory_registry();
      help_bubble_ =
          factory.CreateHelpBubble(anchor_element, std::move(bubble_params_));
      DCHECK(help_bubble_);

      // Maybe focus the web contents containing the bubble (if it's the main
      // contents).
      if (help_bubble_) {
        if (auto* const bubble =
                help_bubble_->AsA<user_education::HelpBubbleWebUI>()) {
          if (browser_->tab_strip_model()->GetActiveWebContents() ==
              bubble->GetWebContents()) {
            browser_->window()->FocusWebContentsPane();
          }
        }
      }
    }

    if (!help_bubble_) {
      std::move(callback_).Run(this, false);
      delete this;
      return;
    }
    help_bubble_closed_subscription_ = help_bubble_->AddOnCloseCallback(
        base::BindOnce(&ShowPromoInPageImpl::OnBubbleClosed, GetWeakPtr()));
    std::move(callback_).Run(this, true);
  }

  void OnBubbleClosed(user_education::HelpBubble*,
                      user_education::HelpBubble::CloseReason) {
    delete this;
  }

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

base::WeakPtr<ShowPromoInPage> ShowPromoInPage::Start(Browser* browser,
                                                      Params params) {
  return (new ShowPromoInPageImpl(browser, std::move(params)))->GetWeakPtr();
}
