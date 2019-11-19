// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/front_eliding_title_label.h"

#include "ui/views/accessibility/view_accessibility.h"

std::unique_ptr<views::Label> CreateFrontElidingTitleLabel(
    const base::string16& text) {
  auto label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_DIALOG_TITLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);
  label->GetViewAccessibility().OverrideRole(ax::mojom::Role::kIgnored);

  // Elide from head in order to keep the most significant part of the origin
  // and avoid spoofing. Note that in English, GetWindowTitle() returns a
  // string
  // "$ORIGIN wants to", so the "wants to" will not be elided. In other
  // languages, the non-origin part may appear fully or partly before the
  // origin (e.g., in Filipino, "Gusto ng $ORIGIN na"), which means it may be
  // elided. This is not optimal, but it is necessary to avoid origin
  // spoofing. See crbug.com/774438.
  label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, which would mean a very long origin gets
  // truncated from the least significant side. Explicitly disable multiline.
  label->SetMultiLine(false);

  return label;
}
