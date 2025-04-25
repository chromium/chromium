// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view.h"

namespace page_actions {

class PageActionController;
class PageActionModelInterface;
struct PageActionViewParams;

// PageActionView is the view displaying the page action. There is one per
// browser, per page action.
class PageActionView : public IconLabelBubbleView,
                       public PageActionModelObserver {
  METADATA_HEADER(PageActionView, IconLabelBubbleView)
 public:
  PageActionView(actions::ActionItem* action_item,
                 const PageActionViewParams& params,
                 ui::ElementIdentifier element_identifier);
  PageActionView(const PageActionView&) = delete;
  PageActionView& operator=(const PageActionView&) = delete;
  ~PageActionView() override;

  // Sets the controller for this view, and attaches this view in the
  // controller.
  void OnNewActiveController(PageActionController* controller);

  // As an alternative to OnNewActiveController(), just set the observed model.
  // TODO(crbug.com/388524315): Merge OnNewActiveController and this method.
  void SetModel(PageActionModelInterface* model);

  // Indicates whether this view is showing a suggestion chip.
  // A chip is considered showing even if it is mid-animation (i.e. while
  // expanding and collapsing).
  bool IsChipVisible() const;

  using ChipVisibilityChanged = base::RepeatingCallback<void(PageActionView*)>;
  base::CallbackListSubscription AddChipVisibilityChangedCallback(
      ChipVisibilityChanged callback);

  // PageActionModelObserver:
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

  // IconLabelBubbleView:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;
  void OnTouchUiChanged() override;
  void UpdateBorder() override;
  bool ShouldShowLabelAfterAnimation() const override;
  bool ShouldShowSeparator() const override;
  bool ShouldUpdateInkDropOnClickCanceled() const override;
  void NotifyClick(const ui::Event& event) override;
  gfx::Size GetMinimumSize() const override;
  bool IsBubbleShowing() const override;
  bool IsTriggerableEvent(const ui::Event& event) override;

  actions::ActionId GetActionId() const;

  views::View* GetLabelForTesting();
  gfx::SlideAnimation& GetSlideAnimationForTesting();

 private:
  // The image associated with the `action_item_` size may be different from the
  // size needed for the location bar page action icon. Therefore, we should to
  // update the image size if needed.
  void UpdateIconImage();

  // Changes to label visibility indicate that the chip state of this page
  // action changed. This handler ensures the view is updated accordingly.
  void OnLabelVisibilityChanged();

  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      observation_{this};

  // The view creates and holds the current controller's subscription to
  // ActionItem updates. This ensures that updates aren't unnecessarily
  // propagated to every tab's controller.
  base::CallbackListSubscription action_item_controller_subscription_;

  const int icon_size_;
  const gfx::Insets icon_insets_;

  // Subscription to changes in label visibility, used for updating properties
  // dependent on label visibility and notifying others of chip state changes.
  base::CallbackListSubscription label_visibility_changed_subscription_;

  // Client-provided callbacks for changes to chip state.
  base::RepeatingCallbackList<void(PageActionView*)>
      chip_visibility_changed_callbacks_;

  // Used to record click event histogram. It's initialized to base::DoNothing()
  // for testing purpose.
  base::RepeatingCallback<void(PageActionTrigger)> click_callback_ =
      base::DoNothing();
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
