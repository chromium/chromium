// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

namespace chromeos {

namespace {
constexpr SkColor kMultitaskMenuBackgroundColor =
    SkColorSetARGB(255, 255, 255, 255);
constexpr int kMultitaskMenuBubbleCornerRadius = 8;
constexpr int KMultitaskMenuWidth = 270;
constexpr int kMultitaskMenuHeight = 247;
}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor, aura::Window* parent_window) {
  DCHECK(parent_window);
  set_color(kMultitaskMenuBackgroundColor);
  SetAnchorView(anchor);
  SetPaintToLayer();
  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  // TODO(shidi): Confirm with UX/UI for additional arrow choices when parent
  // window has no space for `MultitaskMenu` to arrow at `TOP_CENTER`.
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetPreferredSize(gfx::Size(KMultitaskMenuWidth, kMultitaskMenuHeight));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(parent_window);
  set_close_on_deactivate(true);
}

MultitaskMenu::~MultitaskMenu() {
  if (bubble_widget_)
    HideBubble();
}

void MultitaskMenu::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_observer_.Reset();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::ShowBubble() {
  DCHECK(this->parent_window());
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
  bubble_widget_->Show();
  bubble_widget_observer_.Observe(bubble_widget_);
  bubble_widget_->Activate();
}

void MultitaskMenu::HideBubble() {
  DCHECK(bubble_widget_);
  bubble_widget_->Close();
  bubble_widget_observer_.Reset();
  bubble_widget_ = nullptr;
}
}  // namespace chromeos
