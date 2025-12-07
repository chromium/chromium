// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_live_caption_button.h"

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"

OverlayWindowLiveCaptionButton::OverlayWindowLiveCaptionButton(
    PressedCallback callback)
    : SimpleOverlayWindowImageButton(
          std::move(callback),
          vector_icons::kLiveCaptionOnIcon,
          l10n_util::GetStringUTF16(
              IDS_PICTURE_IN_PICTURE_LIVE_CAPTION_CONTROL_TEXT)) {}

OverlayWindowLiveCaptionButton::~OverlayWindowLiveCaptionButton() = default;

void OverlayWindowLiveCaptionButton::SetIsLiveCaptionDialogOpen(bool is_open) {
  if (is_open) {
    // While the live caption dialog is open, this button should remain
    // highlighted.
    views::InkDrop::Get(this)->SetMode(
        views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
    views::InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTIVATED,
                                              nullptr);

    // Set the expanded state so that screen readers inform the user of the
    // dialog state.
    GetViewAccessibility().SetIsExpanded();
  } else {
    // While the live caption dialog is closed, the highlight should act like a
    // normal button.
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

    // Set the collapsed state so that screen readers inform the user of the
    // dialog state.
    GetViewAccessibility().SetIsCollapsed();
  }
}

BEGIN_METADATA(OverlayWindowLiveCaptionButton)
END_METADATA
