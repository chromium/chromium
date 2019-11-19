// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

class ToolbarActionsBarBubbleViewsTest;

namespace views {
class ImageButton;
class Label;
}

class ToolbarActionsBarBubbleViews : public views::BubbleDialogDelegateView,
                                     public views::ButtonListener {
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

  void Show();
  std::string GetAnchorActionId();

  const views::Label* body_text() const { return body_text_; }
  const views::Label* item_list() const { return item_list_; }
  views::ImageButton* learn_more_button() const { return learn_more_button_; }

 private:
  friend class ToolbarActionsBarBubbleViewsTest;

  std::unique_ptr<views::View> CreateExtraInfoView();

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  void Init() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate_;
  bool delegate_notified_of_close_ = false;
  views::Label* body_text_ = nullptr;
  views::Label* item_list_ = nullptr;
  views::ImageButton* learn_more_button_ = nullptr;
  const bool anchored_to_action_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_VIEWS_H_
