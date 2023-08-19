// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_
#define COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkColor.h"

namespace favicon_base {

// Styling specifications of a fallback icon. The icon is composed of a solid
// rounded square containing a single letter. The specification excludes the
// icon URL and size, which are given when the icon is rendered.
struct FallbackIconStyle {
  FallbackIconStyle();
  ~FallbackIconStyle();

  // Icon background fill color.
  SkColor background_color;
  bool is_default_background_color;

  // Icon text color.
  SkColor text_color;

  bool operator==(const FallbackIconStyle& other) const;
};

// Set |style|'s background color to the dominant color of |bitmap_data|,
// clamping luminance down to a reasonable maximum value so that light text is
// readable.
void SetDominantColorAsBackground(base::span<const uint8_t> bitmap_data,
                                  FallbackIconStyle* style);

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_
