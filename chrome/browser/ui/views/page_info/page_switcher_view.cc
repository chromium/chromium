// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_switcher_view.h"

#include "ui/views/layout/fill_layout.h"

PageSwitcherView::PageSwitcherView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

PageSwitcherView::~PageSwitcherView() = default;

void PageSwitcherView::SwitchToPage(std::unique_ptr<views::View> page) {
  if (current_page_)
    RemoveChildViewT(current_page_);
  current_page_ = AddChildView(std::move(page));
  PreferredSizeChanged();
}

void PageSwitcherView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}
