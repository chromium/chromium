// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_CHOOSER_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_CHOOSER_BUBBLE_UI_H_

#include <memory>

#include "base/macros.h"
#include "components/bubble/bubble_ui.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class BubbleDialogDelegateView;
}

class Browser;
class ChooserController;
class ChooserBubbleUiViewDelegate;

// ChooserBubbleUi implements a chooser-based permission model,
// it uses table view to show a list of items (such as usb devices, etc.)
// for user to grant permission. It can be used by the WebUSB or WebBluetooth
// APIs. It is owned by the BubbleController, which is owned by the
// BubbleManager.
class ChooserBubbleUi : public BubbleUi, public views::WidgetObserver {
 public:
  ChooserBubbleUi(Browser* browser,
                  std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleUi() override;

  // BubbleUi
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  void CreateAndShow(views::BubbleDialogDelegateView* delegate);

  Browser* const browser_;  // Weak.
  // Weak. Owned by its parent view.
  ChooserBubbleUiViewDelegate* chooser_bubble_ui_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUi);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_CHOOSER_BUBBLE_UI_H_
