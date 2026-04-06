// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"

VerticalTabStripScrollBar::VerticalTabStripScrollBar(
    tabs::VerticalTabStripStateController* state_controller)
    : tab_strip_collapsed_(
          state_controller ? state_controller->GetCollapseState() !=
                                 tabs::VerticalTabStripCollapseState::kExpanded
                           : false) {
  if (state_controller) {
    collapsed_state_changed_subscription_ =
        state_controller->RegisterOnCollapseChanged(base::BindRepeating(
            &VerticalTabStripScrollBar::OnCollapseStateChanged,
            base::Unretained(this)));
  }
}

VerticalTabStripScrollBar::~VerticalTabStripScrollBar() = default;

bool VerticalTabStripScrollBar::ShouldHaveRightMargin() const {
  return !tab_strip_collapsed_;
}

void VerticalTabStripScrollBar::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  // Apply the margins immediately at the start of the animation by including
  // the collapsing state.
  const bool collapsed =
      state != tabs::VerticalTabStripCollapseState::kExpanded;
  if (tab_strip_collapsed_ != collapsed) {
    tab_strip_collapsed_ = collapsed;
    InvalidateLayout();
  }
}

BEGIN_METADATA(VerticalTabStripScrollBar)
END_METADATA
