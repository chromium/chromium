// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

namespace omnibox_everywhere {

namespace {

gfx::ImageSkia GetBaseIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_GOOGLE_G_GRADIENT_16_ALT);
#else
#if BUILDFLAG(IS_WIN)
  constexpr int kIconDipSize = 32;
#else
  constexpr int kIconDipSize = 16;
#endif
  return gfx::CreateVectorIcon(vector_icons::kSearchIcon, kIconDipSize,
                               SK_ColorBLACK);
#endif
}

}  // namespace

gfx::ImageSkia GetOmniboxEverywhereIcon() {
  gfx::ImageSkia icon = GetBaseIcon();

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return gfx::ImageSkia::CreateFrom1xBitmap(
      icon.GetRepresentation(2.0f).GetBitmap());
#else
  return icon;
#endif
}

}  // namespace omnibox_everywhere
