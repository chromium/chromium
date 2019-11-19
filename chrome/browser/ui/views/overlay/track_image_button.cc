// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/track_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kTrackImageSize = 24;

constexpr SkColor kTrackIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

TrackImageButton::TrackImageButton(ButtonListener* listener,
                                   const gfx::VectorIcon& icon,
                                   base::string16 label)
    : ImageButton(listener),
      image_(gfx::CreateVectorIcon(icon, kTrackImageSize, kTrackIconColor)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetImage(views::Button::STATE_NORMAL, image_);

  // Accessibility.
  SetFocusForPlatform();
  SetAccessibleName(label);
  SetTooltipText(label);
  SetInstallFocusRingOnFocus(true);
}

gfx::Size TrackImageButton::GetLastVisibleSize() const {
  return size().IsEmpty() ? last_visible_size_ : size();
}

void TrackImageButton::OnBoundsChanged(const gfx::Rect&) {
  if (!size().IsEmpty())
    last_visible_size_ = size();
}

void TrackImageButton::ToggleVisibility(bool is_visible) {
  SetVisible(is_visible);
  SetEnabled(is_visible);
  SetSize(is_visible ? GetLastVisibleSize() : gfx::Size());
}

}  // namespace views
