// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_ARTWORK_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_ARTWORK_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace media_message_center {

// An artwork view with a rounded rectangle vignette
class MediaArtworkView : public views::View {
 public:
  METADATA_HEADER(MediaArtworkView);
  MediaArtworkView(float corner_radius,
                   const gfx::Size& artwork_size,
                   const gfx::Size& favicon_size);
  ~MediaArtworkView() override = default;

  void SetVignetteColor(const SkColor& vignette_color);
  SkColor GetVignetteColor() const;

  void SetBackgroundColor(const SkColor& background_color);

  void SetImage(const gfx::ImageSkia& image);
  void SetFavicon(const gfx::ImageSkia& favicon);

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  SkColor vignette_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;

  gfx::ImageSkia image_;
  gfx::ImageSkia favicon_;

  const float corner_radius_;
  const gfx::Size artwork_size_;
  const gfx::Size favicon_size_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_ARTWORK_VIEW_H_
