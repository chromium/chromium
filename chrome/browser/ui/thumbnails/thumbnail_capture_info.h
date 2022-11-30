// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_INFO_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_INFO_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Describes how a thumbnail bitmap should be generated from a target surface.
// All sizes are in pixels, not DIPs.
struct ThumbnailCaptureInfo {
  // The total source size (including scrollbars).
  gfx::Size source_size;

  // Insets for scrollbars in the source image that should probably be
  // ignored for thumbnailing purposes.
  gfx::Insets scrollbar_insets;

  // Cropping rectangle for the source canvas, in pixels.
  gfx::Rect copy_rect;

  // Size of the target bitmap in pixels.
  gfx::Size target_size;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_INFO_H_
