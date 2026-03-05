// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hover_card_anchor_target.h"
#include "ui/views/view.h"

HoverCardAnchorTarget::HoverCardAnchorTarget(views::View* anchor_view)
    : anchor_view_(anchor_view) {
  CHECK(anchor_view_);
}

views::View* HoverCardAnchorTarget::GetAnchorView() {
  return const_cast<views::View*>(
      static_cast<const HoverCardAnchorTarget*>(this)->GetAnchorView());
}

const views::View* HoverCardAnchorTarget::GetAnchorView() const {
  return anchor_view_;
}
