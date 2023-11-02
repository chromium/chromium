// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/frame/window_frame_util.h"

#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#endif  // BUILDFLAG(IS_WIN)

// static
SkAlpha WindowFrameUtil::CalculateWindows10GlassCaptionButtonBackgroundAlpha(
    SkAlpha theme_alpha) {
  return theme_alpha == SK_AlphaOPAQUE ? 0xCC : theme_alpha;
}

// static
gfx::Size WindowFrameUtil::GetWindows10GlassCaptionButtonAreaSize() {
  // TODO(crbug.com/1257470): Fix uses of this to dynamically compute the size
  // of the glass caption button area.
  constexpr int kNumButtons = 3;

  return gfx::Size(
      (kNumButtons * kWindows10GlassCaptionButtonWidth) +
          ((kNumButtons - 1) * kWindows10GlassCaptionButtonVisualSpacing),
      kWindows10GlassCaptionButtonHeightRestored);
}

// static
bool WindowFrameUtil::IsWin10TabSearchCaptionButtonEnabled(
    const Browser* browser) {
#if BUILDFLAG(IS_WIN)
  return browser->is_type_normal() &&
         base::win::GetVersion() >= base::win::Version::WIN10 &&
         base::FeatureList::IsEnabled(features::kWin10TabSearchCaptionButton);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}
