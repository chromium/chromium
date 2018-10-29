// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuButton;

// This bubble view is displayed when a Happiness tracking survey is triggered.
// It displays a WebUI that hosts the survey.
class HatsBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Returns a pointer to the Hats Bubble being shown. For testing only.
  static views::BubbleDialogDelegateView* GetHatsBubble();
  // Shows the bubble if one is not already showing.
  static views::BubbleDialogDelegateView* CreateHatsBubble(
      AppMenuButton* anchor_button,
      Browser* browser,
      gfx::NativeView parent_view);

 protected:
  // views::BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  bool ShouldShowCloseButton() const override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  HatsBubbleView(AppMenuButton* anchor_button,
                 Browser* browser,
                 gfx::NativeView parent_view);
  ~HatsBubbleView() override;

  static HatsBubbleView* instance_;
  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  DISALLOW_COPY_AND_ASSIGN(HatsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
