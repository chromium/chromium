// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/actions/action_id.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class AutofillBubbleBase;
class AutofillBubbleHandler;

// Interface that exposes controller functionality to all autofill bubbles.
class AutofillBubbleControllerBase : public BubbleControllerBase,
                                     public content::WebContentsObserver {
 public:
  // Configures how `AutofillBubbleControllerBase` interacts with the browser
  // action (`action_id`) associated with this bubble.
  struct PageActionConfig {
    actions::ActionId action_id;

    // If true, `UpdatePageActionIcon()` shows and hides `action_id`'s omnibox
    // icon based on `ShouldShowPageAction()`. Set to false if omnibox icon
    // visibility is owned by a separate `*PageActionController` or if the
    // bubble never shows an omnibox icon.
    bool should_manage_visibility = false;

    // If true, marks `action_id`'s `ActionItem` as showing a bubble while this
    // bubble is open (`ActionItem::SetIsShowingBubble()`). This prevents
    // clicking the anchor icon from immediately re-triggering the action after
    // mouse-down closes the bubble, and lets `PageActionController` suppress
    // competing anchored messages.
    // TODO(crbug.com/582209796): Enable for pinnable toolbar actions once
    // clicking a `PinnedActionToolbarButton` dismisses inactive bubbles.
    bool should_sync_bubble_showing_state = false;
  };

  AutofillBubbleControllerBase(content::WebContents* web_contents,
                               PageActionConfig page_action_config);
  AutofillBubbleControllerBase(tabs::TabInterface* tab_interface,
                               PageActionConfig page_action_config);
  ~AutofillBubbleControllerBase() override;

  // Calls the bubble manager to show the bubble on desktop. On Android, just
  // shows the bubble.
  // `force_show` indicates to the bubble manager to show this bubble
  // irrespective of its priority.
  void QueueOrShowBubble(bool force_show = false);

  // BubbleControllerBase:
  void ShowBubble() override;
  void HideBubble(bool initiated_by_bubble_manager) override;
  bool CanBeReshown() const override;
  bool ShouldReshowOnTabVisible() const override;
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

  bool IsBubbleManagerEnabled();

  // Subclasses should implement this method to actually show the bubble and
  // potentially log metrics.
  virtual void DoShowBubble() = 0;

  // Updates page action visibility.
  virtual void UpdatePageActionIcon();

  // Subclasses can override this method to provide custom page action
  // visibility logic.
  virtual bool ShouldShowPageAction();

  // Subclasses can override this method to supply custom tooltip text for their
  // page action.
  virtual std::optional<std::u16string> GetPageActionTooltipText();

  // Returns `false` if a bubble is already queued to be shown.
  [[nodiscard]] bool MaySetUpBubble();

  // Setter for `bubble_view`.
  void SetBubbleView(AutofillBubbleBase& bubble_view);

  // Resets the `bubble_view` and informs the bubble manager about it.
  void ResetBubbleViewAndInformBubbleManager();

  AutofillBubbleBase* bubble_view() const { return bubble_view_; }

  tabs::TabInterface* GetTabInterface();

  AutofillBubbleHandler* GetAutofillBubbleHandler();

  // True if anytime, the bubble was shown to the user in the lifecycle of the
  // bubble.
  // TODO(crbug.com/432429605): Remove this state and log a separate enum for
  // the cases where bubble is discarded by bubble manager.
  bool was_bubble_shown_ = false;

  // True when the hide bubble is requested by the BubbleManager.
  bool bubble_hide_initiated_by_bubble_manager_ = false;

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  void SetActionItemIsShowingBubble(bool showing);

  // Non-null when constructed with `tabs::TabInterface*`.
  const raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  const PageActionConfig page_action_config_;

  base::CallbackListSubscription will_discard_contents_subscription_;

  // This indicates to the `BubbleManager` whether it should show the next
  // bubble.
  bool allow_bubble_manager_to_show_next_ = true;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<AutofillBubbleBase> bubble_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
