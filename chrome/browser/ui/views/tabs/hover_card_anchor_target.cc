// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hover_card_anchor_target.h"

#include "ui/base/class_property.h"
#include "ui/views/view.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(, HoverCardAnchorTarget*)
DEFINE_UI_CLASS_PROPERTY_KEY(HoverCardAnchorTarget*,
                             kHoverCardAnchorTarget,
                             nullptr)

HoverCardAnchorTarget::HoverCardAnchorTarget(views::View* anchor_view)
    : anchor_view_(anchor_view) {
  CHECK(anchor_view_);
  anchor_view_->SetProperty(kHoverCardAnchorTarget, this);
}

views::View* HoverCardAnchorTarget::GetAnchorView() {
  return const_cast<views::View*>(
      static_cast<const HoverCardAnchorTarget*>(this)->GetAnchorView());
}

const views::View* HoverCardAnchorTarget::GetAnchorView() const {
  return anchor_view_;
}

HoverCardAnchorTarget* HoverCardAnchorTarget::FromAnchorView(
    views::View* anchor_view) {
  return anchor_view->GetProperty(kHoverCardAnchorTarget);
}
