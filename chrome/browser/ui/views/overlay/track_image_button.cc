// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/track_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace {

const int kTrackImageSize = 24;

}  // namespace

namespace views {

TrackImageButton::TrackImageButton(PressedCallback callback,
                                   const gfx::VectorIcon& icon,
                                   std::u16string label)
    : ImageButton(std::move(callback)),
      image_(
          gfx::CreateVectorIcon(icon, kTrackImageSize, kPipWindowIconColor)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetImage(views::Button::STATE_NORMAL, image_);

  // Accessibility.
  SetAccessibleName(label);
  SetTooltipText(label);
  SetInstallFocusRingOnFocus(true);
}

void TrackImageButton::SetVisible(bool visible) {
  // We need to do more than the usual visibility change because otherwise the
  // overlay window cannot be dragged when grabbing within the button area.
  ImageButton::SetVisible(visible);
  SetSize(visible ? last_visible_size_ : gfx::Size());
}

void TrackImageButton::OnBoundsChanged(const gfx::Rect&) {
  if (!size().IsEmpty())
    last_visible_size_ = size();
}

BEGIN_METADATA(TrackImageButton, views::ImageButton)
END_METADATA

}  // namespace views
