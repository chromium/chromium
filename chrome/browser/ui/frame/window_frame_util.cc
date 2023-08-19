// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/frame/window_frame_util.h"

#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/ui_base_features.h"
#endif  // BUILDFLAG(IS_WIN)

// static
SkAlpha WindowFrameUtil::CalculateWindowsCaptionButtonBackgroundAlpha(
    SkAlpha theme_alpha) {
  return theme_alpha == SK_AlphaOPAQUE ? 0xCC : theme_alpha;
}

// static
gfx::Size WindowFrameUtil::GetWindowsCaptionButtonAreaSize() {
  // TODO(crbug.com/1257470): Fix uses of this to dynamically compute the size
  // of the caption button area.
  constexpr int kNumButtons = 3;

  return gfx::Size((kNumButtons * kWindowsCaptionButtonWidth) +
                       ((kNumButtons - 1) * kWindowsCaptionButtonVisualSpacing),
                   kWindowsCaptionButtonHeightRestored);
}

// static
bool WindowFrameUtil::IsWindowsTabSearchCaptionButtonEnabled(
    const Browser* browser) {
#if BUILDFLAG(IS_WIN)
  return !features::IsChromeRefresh2023() && browser->is_type_normal();
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}
