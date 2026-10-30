// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/utils/known_resource_images.h"

#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace webui_toolbar {

std::optional<std::string_view> GetKnownResourceImageUrl(
    const ui::ImageModel& image) {
  if (!image.IsImage()) {
    return std::nullopt;
  }

  struct KnownResourceImage {
    int resource_id;
    std::string_view url;
  };
  static constexpr KnownResourceImage kKnownResourceImages[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {IDR_GOOGLE_G_GRADIENT_16_ALT,
       "chrome://theme/IDR_GOOGLE_G_GRADIENT_16_ALT"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };

  gfx::ImageSkia image_skia = image.GetImage().AsImageSkia();
  if (image_skia.isNull()) {
    return std::nullopt;
  }
  for (const auto& known : kKnownResourceImages) {
    // ResourceBundle caches its images, so images obtained from it for the
    // same resource ID share the same backing storage.
    const gfx::ImageSkia* target =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            known.resource_id);
    if (target && image_skia.BackedBySameObjectAs(*target)) {
      return known.url;
    }
  }
  return std::nullopt;
}

}  // namespace webui_toolbar
