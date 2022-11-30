// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"

#include "chrome/browser/ui/browser.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

CloseBubbleOnTabActivationHelper::CloseBubbleOnTabActivationHelper(
    views::BubbleDialogDelegateView* owner_bubble,
    Browser* browser)
    : owner_bubble_(owner_bubble), browser_(browser) {
  DCHECK(owner_bubble_);
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
}

CloseBubbleOnTabActivationHelper::~CloseBubbleOnTabActivationHelper() {
  if (browser_)
    browser_->tab_strip_model()->RemoveObserver(this);
}

void CloseBubbleOnTabActivationHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  if (owner_bubble_) {
    views::Widget* bubble_widget = owner_bubble_->GetWidget();
    if (bubble_widget)
      bubble_widget->Close();
    owner_bubble_ = nullptr;
  }
}

void CloseBubbleOnTabActivationHelper::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  DCHECK(browser_);
  browser_->tab_strip_model()->RemoveObserver(this);
  browser_ = nullptr;
}
