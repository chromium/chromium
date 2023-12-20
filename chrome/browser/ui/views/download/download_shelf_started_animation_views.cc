// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_started_animation_views.h"

#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/download/download_started_animation_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"

DownloadShelfStartedAnimationViews::DownloadShelfStartedAnimationViews(
    content::WebContents* web_contents)
    : DownloadStartedAnimationViews(
          web_contents,
          base::Milliseconds(600),
          ui::ImageModel::FromVectorIcon(
              kFileDownloadShelfIcon,
              kColorDownloadStartedAnimationForeground,
              72)) {}

int DownloadShelfStartedAnimationViews::GetX() const {
  // Align the image with the bottom left of the web contents (so that it
  // points to the newly created download).
  return base::i18n::IsRTL()
             ? web_contents_bounds().right() - GetPreferredSize().width()
             : web_contents_bounds().x();
}

int DownloadShelfStartedAnimationViews::GetY() const {
  int height = GetPreferredSize().height();
  return static_cast<int>(web_contents_bounds().bottom() - height -
                          height * (1 - GetCurrentValue()));
}

float DownloadShelfStartedAnimationViews::GetOpacity() const {
  // Start at zero, peak halfway and end at zero.
  return static_cast<float>(
      std::min(1.0 - pow(GetCurrentValue() - 0.5, 2) * 4.0, 1.0));
}

BEGIN_METADATA(DownloadShelfStartedAnimationViews)
END_METADATA
