// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

class ToolbarActionsBarBubbleViewsTest;

namespace views {
class ImageButton;
class Label;
}

class ToolbarActionsBarBubbleViews : public views::BubbleDialogDelegateView,
                                     public views::WidgetObserver {
  METADATA_HEADER(ToolbarActionsBarBubbleViews, views::BubbleDialogDelegateView)

 public:
  // Creates the bubble anchored to |anchor_view|, which may not be nullptr.
  ToolbarActionsBarBubbleViews(
      views::View* anchor_view,
      bool anchored_to_action,
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate);
  ToolbarActionsBarBubbleViews(const ToolbarActionsBarBubbleViews&) = delete;
  ToolbarActionsBarBubbleViews& operator=(const ToolbarActionsBarBubbleViews&) =
      delete;
  ~ToolbarActionsBarBubbleViews() override;

  std::string GetAnchorActionId() const;

  const views::Label* body_text() const { return body_text_; }
  const views::Label* item_list() const { return item_list_; }
  views::ImageButton* learn_more_button() const { return learn_more_button_; }

 private:
  friend class ToolbarActionsBarBubbleViewsTest;

  std::unique_ptr<views::View> CreateExtraInfoView();

  void ButtonPressed();

  void NotifyDelegateOfClose(
      ToolbarActionsBarBubbleDelegate::CloseAction action);

  // views::BubbleDialogDelegateView:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Init() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate_;
  bool delegate_notified_of_close_ = false;
  bool observer_notified_of_show_ = false;
  raw_ptr<views::Label> body_text_ = nullptr;
  raw_ptr<views::Label> item_list_ = nullptr;
  raw_ptr<views::ImageButton> learn_more_button_ = nullptr;
  const bool anchored_to_action_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
