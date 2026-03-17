// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"

VerticalTabStripScrollBar::VerticalTabStripScrollBar(
    tabs::VerticalTabStripStateController* state_controller)
    : tab_strip_collapsed_(state_controller ? state_controller->IsCollapsed()
                                            : false) {
  if (state_controller) {
    collapsed_state_changed_subscription_ =
        state_controller->RegisterOnCollapseChanged(base::BindRepeating(
            &VerticalTabStripScrollBar::OnCollapsedStateChanged,
            base::Unretained(this)));
  }
}

VerticalTabStripScrollBar::~VerticalTabStripScrollBar() = default;

bool VerticalTabStripScrollBar::ShouldHaveRightMargin() const {
  return !tab_strip_collapsed_;
}

void VerticalTabStripScrollBar::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* state_controller) {
  if (tab_strip_collapsed_ != state_controller->IsCollapsed()) {
    tab_strip_collapsed_ = state_controller->IsCollapsed();
    InvalidateLayout();
  }
}

BEGIN_METADATA(VerticalTabStripScrollBar)
END_METADATA
