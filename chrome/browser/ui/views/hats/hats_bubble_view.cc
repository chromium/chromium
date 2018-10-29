// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_bubble_view.h"

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_features.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
HatsBubbleView* HatsBubbleView::instance_ = nullptr;

views::BubbleDialogDelegateView* HatsBubbleView::GetHatsBubble() {
  return instance_;
}

// static
views::BubbleDialogDelegateView* HatsBubbleView::CreateHatsBubble(
    AppMenuButton* anchor_button,
    Browser* browser,
    gfx::NativeView parent_view) {
  base::RecordAction(base::UserMetricsAction("HatsBubble.Show"));

  return new HatsBubbleView(anchor_button, browser, parent_view);
}

HatsBubbleView::HatsBubbleView(AppMenuButton* anchor_button,
                               Browser* browser,
                               gfx::NativeView parent_view)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_LEFT),
      close_bubble_helper_(this, browser) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HATS_BUBBLE);

  set_parent_window(parent_view);

  // TODO: this->AddChildView(WebUI)

  views::BubbleDialogDelegateView::CreateBubble(this);

  instance_ = this;
}

HatsBubbleView::~HatsBubbleView() {}

int HatsBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool HatsBubbleView::ShouldShowCloseButton() const {
  return true;
}

void HatsBubbleView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  instance_ = nullptr;
}
