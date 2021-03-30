// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_UI_LOCK_BUBBLE_H_
#define COMPONENTS_EXO_UI_LOCK_BUBBLE_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace exo {

// This view displays a Bubble within the always on top container instructing a
// user on how to exit non-immersive fullscreen or 'gaming mode'.
// TODO(b/161952658): Use  ShellSurfaceBase::AddOverlay() rather than
// BubbleDialogDelegateView.
class UILockBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(UILockBubbleView);
  ~UILockBubbleView() override;
  UILockBubbleView(const UILockBubbleView&) = delete;
  UILockBubbleView& operator=(const UILockBubbleView&) = delete;

  static views::Widget* DisplayBubble(views::View* anchor_view);

 private:
  UILockBubbleView(views::View* anchor_view);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_UI_LOCK_BUBBLE_H_
