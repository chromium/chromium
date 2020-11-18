// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_

#include <vector>

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class GridLayout;
class Separator;
}

namespace sharesheet {
class SharesheetServiceDelegate;
}

namespace content {
class WebContents;
}

class SharesheetExpandButton;

class SharesheetBubbleView : public views::BubbleDialogDelegateView {
 public:
  using TargetInfo = sharesheet::TargetInfo;

  SharesheetBubbleView(views::View* anchor_view,
                       sharesheet::SharesheetServiceDelegate* delegate);
  SharesheetBubbleView(content::WebContents* web_contents,
                       sharesheet::SharesheetServiceDelegate* delegate);
  SharesheetBubbleView(const SharesheetBubbleView&) = delete;
  SharesheetBubbleView& operator=(const SharesheetBubbleView&) = delete;
  ~SharesheetBubbleView() override;

  // |close_callback| is run when the bubble is closed.
  void ShowBubble(std::vector<TargetInfo> targets,
                  apps::mojom::IntentPtr intent,
                  sharesheet::CloseCallback close_callback);
  void ShowActionView();
  void ResizeBubble(const int& width, const int& height);
  void CloseBubble();

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // views::WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  void CreateBubble();
  std::unique_ptr<views::View> MakeScrollableTargetView(
      std::vector<TargetInfo> targets);
  void PopulateLayoutsWithTargets(std::vector<TargetInfo> targets,
                                  views::GridLayout* default_layout,
                                  views::GridLayout* expanded_layout);
  void ExpandButtonPressed();
  void TargetButtonPressed(TargetInfo target);
  void UpdateAnchorPosition();
  void SetToDefaultBubbleSizing();

  // Owns this class.
  sharesheet::SharesheetServiceDelegate* delegate_;
  base::string16 active_target_;
  apps::mojom::IntentPtr intent_;
  sharesheet::CloseCallback close_callback_;

  int width_ = 0;
  int height_ = 0;
  bool user_cancelled_ = true;
  bool show_expanded_view_ = false;

  size_t keyboard_highlighted_target_ = 0;

  views::View* main_view_ = nullptr;
  views::View* default_view_ = nullptr;
  views::View* expanded_view_ = nullptr;
  views::View* share_action_view_ = nullptr;
  // Separator that appears above the expand button.
  views::Separator* expand_button_separator_ = nullptr;
  // Separator between the default_view and the expanded_view.
  views::Separator* expanded_view_separator_ = nullptr;
  views::View* parent_view_ = nullptr;
  SharesheetExpandButton* expand_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
