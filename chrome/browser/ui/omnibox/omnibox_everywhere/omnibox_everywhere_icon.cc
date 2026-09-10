// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"

#include "build/branding_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

namespace omnibox_everywhere {

gfx::ImageSkia GetOmniboxEverywhereIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_GOOGLE_G_GRADIENT_16_ALT);
#else
  return gfx::CreateVectorIcon(vector_icons::kSearchIcon, 16, SK_ColorBLACK);
#endif
}

}  // namespace omnibox_everywhere
