// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_util.h"

#include "base/check.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace favicon {
namespace {

// Desaturate favicon HSL shift values.
const double kDesaturateHue = -1.0;
const double kDesaturateSaturation = 0.0;
const double kDesaturateLightness = 0.6;

}  // namespace

gfx::Image GetTabFaviconMaybeDesaturatedOnError(
    content::WebContents* contents) {
  DCHECK(contents);

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(contents);
  // TODO(crbug.com/40190724): Investigate why some WebContents do not have
  // an attached ContentFaviconDriver.
  if (!favicon_driver) {
    return gfx::Image();
  }

  gfx::Image favicon = favicon_driver->GetFavicon();

  // Favicons are only desaturated for committed network-error pages.
  if (contents->ShouldShowLoadingUI()) {
    return favicon;
  }

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_ERROR) {
    return favicon;
  }

  color_utils::HSL shift = {kDesaturateHue, kDesaturateSaturation,
                            kDesaturateLightness};
  return gfx::Image(gfx::ImageSkiaOperations::CreateHSLShiftedImage(
      *favicon.ToImageSkia(), shift));
}

}  // namespace favicon
