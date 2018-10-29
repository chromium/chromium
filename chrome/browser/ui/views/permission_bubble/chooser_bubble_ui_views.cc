// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include <memory>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

// The Views browser implementation of ChooserBubbleUi's anchor methods.
// Views browsers have a native View to anchor the bubble to, which these
// functions provide.

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return std::make_unique<ChooserBubbleUi>(browser_,
                                           std::move(chooser_controller_));
}

void ChooserBubbleUi::CreateAndShow(views::BubbleDialogDelegateView* delegate) {
  // Set |parent_window_| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  gfx::NativeView parent = widget->GetNativeView();
  DCHECK(parent);
  delegate->set_parent_window(parent);
  if (browser_->window()->IsActive())
    views::BubbleDialogDelegateView::CreateBubble(delegate)->Show();
  else
    views::BubbleDialogDelegateView::CreateBubble(delegate)->ShowInactive();
}
