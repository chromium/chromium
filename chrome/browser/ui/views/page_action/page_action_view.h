// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "ui/actions/actions.h"

namespace page_actions {

class PageActionController;

// PageActionView is the view displaying the page action. There is one per
// browser, per page action.
class PageActionView : public IconLabelBubbleView,
                       public PageActionModelObserver {
  METADATA_HEADER(PageActionView, IconLabelBubbleView)
 public:
  PageActionView(actions::ActionItem* action_item,
                 IconLabelBubbleView::Delegate* parent_delegate);
  PageActionView(const PageActionView&) = delete;
  PageActionView& operator=(const PageActionView&) = delete;
  ~PageActionView() override;

  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

  // Sets the controller for this view, and attaches this view in the
  // controller.
  void OnNewActiveController(PageActionController* controller);

  // PageActionModelObserver
  void OnPageActionModelChanged(PageActionModel* model) override;
  void OnPageActionModelWillBeDeleted(PageActionModel* model) override;

  actions::ActionId GetActionId() const;

 private:
  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  base::ScopedObservation<PageActionModel, PageActionModelObserver>
      observation_{this};
};

class PageActionViewInterface : public views::LabelButtonActionViewInterface {
 public:
  explicit PageActionViewInterface(PageActionView* action_view,
                                   PageActionModel* model);
  PageActionViewInterface(const PageActionViewInterface&) = delete;
  PageActionViewInterface& operator=(const PageActionViewInterface&) = delete;
  ~PageActionViewInterface() override;

  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<PageActionView> action_view_;
  raw_ptr<PageActionModel> model_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_H_
