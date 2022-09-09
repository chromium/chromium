// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/track_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kTrackImageSize = 24;

}  // namespace

TrackImageButton::TrackImageButton(PressedCallback callback,
                                   const gfx::VectorIcon& icon,
                                   std::u16string label)
    : OverlayWindowImageButton(std::move(callback)),
      image_(ui::ImageModel::FromVectorIcon(icon,
                                            kColorPipWindowForeground,
                                            kTrackImageSize)) {
  SetImageModel(views::Button::STATE_NORMAL, image_);

  // Accessibility.
  SetAccessibleName(label);
  SetTooltipText(label);
}

void TrackImageButton::SetVisible(bool visible) {
  // We need to do more than the usual visibility change because otherwise the
  // overlay window cannot be dragged when grabbing within the button area.
  views::ImageButton::SetVisible(visible);
  SetSize(visible ? last_visible_size_ : gfx::Size());
}

void TrackImageButton::OnBoundsChanged(const gfx::Rect&) {
  if (!size().IsEmpty())
    last_visible_size_ = size();
}

BEGIN_METADATA(TrackImageButton, OverlayWindowImageButton)
END_METADATA
