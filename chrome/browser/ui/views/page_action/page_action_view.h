// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "ui/actions/actions.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace ui {
class MouseEvent;
}  // namespace ui

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
                 base::RepeatingCallback<void(actions::ActionId, bool)>
                     chip_state_changed_callback);
  PageActionView(const PageActionView&) = delete;
  PageActionView& operator=(const PageActionView&) = delete;
  ~PageActionView() override;

  // Sets the controller for this view, and attaches this view in the
  // controller.
  void OnNewActiveController(PageActionController* controller);

  // As an alternative to OnNewActiveController(), just set the observed model.
  // TODO(crbug.com/388524315): Merge OnNewActiveController and this method.
  void SetModel(PageActionModelInterface* model);

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
  bool ShouldShowSeparator() const override;
  bool ShouldUpdateInkDropOnClickCanceled() const override;
  void NotifyClick(const ui::Event& event) override;
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnClickCanceled(const ui::Event& event) override;

  actions::ActionId GetActionId() const;

  views::View* GetLabelForTesting();

 private:
  // The image associated with the `action_item_` size may be different from the
  // size needed for the location bar page action icon. Therefore, we should to
  // update the image size if needed.
  void UpdateIconImage();

  // The page action can be in icon mode and suggestion chip mode. This helper
  // ensures that the correct styling is applied based on the current mode.
  void UpdateStyle(bool is_suggestion_chip);

  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      observation_{this};

  // The view creates and holds the current controller's subscription to
  // ActionItem updates. This ensures that updates aren't unnecessarily
  // propagated to every tab's controller.
  base::CallbackListSubscription action_item_controller_subscription_;

  const int icon_size_;
  const gfx::Insets icon_insets_;

  // Helps to notify to the parent container that this child chip state has
  // changed.
  const base::RepeatingCallback<void(actions::ActionId, bool)>
      chip_state_changed_callback_;

  // Indicates that the current page action is showing as a suggestion chip.
  bool showing_suggestion_chip_ = false;

  // Used to track whether the mouse was pressed when associated ephemeral UI
  // (eg. a bubble that closes on focus loss) was showing, to avoid
  // re-triggering the action if so. This is necessary because the bubble will
  // have closed by the time the view invokes the action on button click.
  bool skip_action_invocation_ = false;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
