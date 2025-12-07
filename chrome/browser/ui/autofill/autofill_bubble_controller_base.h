// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/actions/action_id.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillBubbleBase;

// Enum for the current showing state of the bubble.
// TODO(crbug.com/445901842): Investigate if this can be removed.
enum class BubbleState {
  // The bubble and the omnibox icon should be hidden.
  kHidden = 0,
  // Only the omnibox icon should be visible.
  kShowingIcon = 1,
  // The bubble and the omnibox icon should be both visible.
  kShowingIconAndBubble = 2,
};

// Interface that exposes controller functionality to all autofill bubbles.
class AutofillBubbleControllerBase : public BubbleControllerBase,
                                     public content::WebContentsObserver {
 public:
  explicit AutofillBubbleControllerBase(content::WebContents* web_contents);
  ~AutofillBubbleControllerBase() override;

  // Calls the bubble manager to show the bubble if bubble manager is enabled.
  // Otherwise just shows the bubble.
  // `force_show` indicates to the bubble manager to show this bubble
  // irrespective of its priority.
  void QueueOrShowBubble(bool force_show = false);

  // BubbleControllerBase:
  void ShowBubble() override;
  void HideBubble(bool initiated_by_bubble_manager) override;
  bool CanBeReshown() const override;
  bool IsShowingBubble() const override;
  bool IsMouseHovered() const override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 protected:
  // RAII-type guard that, while in scope, instructs the `BubbleManager` not
  // to show the next queued bubble when this controller's bubble is hidden.
  // Must not outlive the controller.
  // TODO(crbug.com/432429605): Look into ways to move this lock to
  // BubbleManager to allow suppressing other controllers' bubbles while a
  // multi-step flow is ongoing (e.g., credit card upload).
  class DoNotShowNextQueuedBubbleGuard final {
   public:
    DoNotShowNextQueuedBubbleGuard(AutofillBubbleControllerBase* controller,
                                   base::PassKey<AutofillBubbleControllerBase>)
        : controller_(CHECK_DEREF(controller)) {
      CHECK(controller_->allow_bubble_manager_to_show_next_);
      controller_->allow_bubble_manager_to_show_next_ = false;
    }
    DoNotShowNextQueuedBubbleGuard(const DoNotShowNextQueuedBubbleGuard&) =
        delete;
    DoNotShowNextQueuedBubbleGuard& operator=(
        const DoNotShowNextQueuedBubbleGuard&) = delete;
    DoNotShowNextQueuedBubbleGuard(DoNotShowNextQueuedBubbleGuard&&) = delete;
    DoNotShowNextQueuedBubbleGuard& operator=(
        DoNotShowNextQueuedBubbleGuard&&) = delete;
    ~DoNotShowNextQueuedBubbleGuard() {
      controller_->allow_bubble_manager_to_show_next_ = true;
    }

   private:
    const raw_ref<AutofillBubbleControllerBase> controller_;
  };

  DoNotShowNextQueuedBubbleGuard DoNotShowNextQueuedBubble() {
    return DoNotShowNextQueuedBubbleGuard(this, {});
  }

  bool IsBubbleManagerEnabled() const {
#if !BUILDFLAG(IS_ANDROID)
    return base::FeatureList::IsEnabled(
        features::kAutofillShowBubblesBasedOnPriorities);
#else
    return false;
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // Migrated page action bubble controller subclasses should override this
  // method to supply their page action's `ActionId`.
  virtual std::optional<actions::ActionId> GetActionIdForPageAction();

  // This method should only be overridden if `GetActionIdForPageAction` is not
  // overridden (page action not migrated yet) or to override the
  // `ActionId`->`PageActionIconType` mapping defined in
  // `page_action_properties_provider.cc`.
  virtual std::optional<PageActionIconType> GetPageActionIconType();

  // Subclasses should implement this method to actually show the bubble and
  // potentially log metrics.
  virtual void DoShowBubble() = 0;

  // Updates page action visibility.
  virtual void UpdatePageActionIcon();

  // Subclasses can override this method to provide custom page action
  // visibility logic with the new page action framework.
  virtual bool ShouldShowPageAction();

  // For migrated page actions, subclasses can override this method to supply
  // custom tooltip text.
  virtual std::optional<std::u16string> GetPageActionTooltipText();

  // If the BubbleManager feature is enabled, this returns `false` if a bubble
  // is already queued to be shown.
  [[nodiscard]] bool MaySetUpBubble();

  // Setter for `bubble_view`.
  void SetBubbleView(AutofillBubbleBase& bubble_view);

  // Resets the `bubble_view` and informs the bubble manager about it.
  void ResetBubbleViewAndInformBubbleManager();

  AutofillBubbleBase* bubble_view() const { return bubble_view_; }

  // True if anytime, the bubble was shown to the user in the lifecycle of the
  // bubble.
  // TODO(crbug.com/432429605): Remove this state and log a separate enum for
  // the cases where bubble is discarded by bubble manager.
  bool was_bubble_shown_ = false;

  // True when the hide bubble is requested by the BubbleManager.
  bool bubble_hide_initiated_by_bubble_manager_ = false;

 private:
  // This indicates to the `BubbleManager` whether it should show the next
  // bubble.
  bool allow_bubble_manager_to_show_next_ = true;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<AutofillBubbleBase> bubble_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
