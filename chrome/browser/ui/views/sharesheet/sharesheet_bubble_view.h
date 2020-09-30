// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_

#include <vector>

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class GridLayout;
}

namespace sharesheet {
class SharesheetServiceDelegate;
}

namespace content {
class WebContents;
}

class SharesheetExpandButton;

class SharesheetBubbleView : public views::BubbleDialogDelegateView,
                             public views::ButtonListener {
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
  // views::ButtonListener overrides
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetDelegate override
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::BubbleDialogDelegateView overrides
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  void CreateBubble();
  std::unique_ptr<views::View> MakeScrollableTargetView();
  void PopulateLayoutsWithTargets(views::GridLayout* default_layout,
                                  views::GridLayout* expanded_layout);
  void OnDialogClosed();
  void UpdateAnchorPosition();
  void SetToDefaultBubbleSizing();

  // Owns this class.
  sharesheet::SharesheetServiceDelegate* delegate_;
  std::vector<TargetInfo> targets_;
  base::string16 active_target_;
  apps::mojom::IntentPtr intent_;
  sharesheet::CloseCallback close_callback_;

  int width_ = 0;
  int height_ = 0;
  bool user_cancelled_ = true;
  bool show_expanded_view_ = false;

  views::View* root_view_ = nullptr;
  views::View* main_view_ = nullptr;
  views::View* expanded_view_ = nullptr;
  views::View* share_action_view_ = nullptr;
  views::View* parent_view_ = nullptr;
  SharesheetExpandButton* expand_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
