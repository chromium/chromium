// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_STARTED_ANIMATION_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_STARTED_ANIMATION_VIEWS_H_

#include "chrome/browser/ui/views/download/download_started_animation_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

// An animation that starts halfway down the screen and moves upwards towards
// the download bubble toolbar icon. The animation is piecewise linear, composed
// of 2 phases. During phase one, the icon moves upwards and fades in. During
// phase two, the icon continues moving upwards and fades out.
// TODO(crbug.com/40255939): Investigate writing this using more modern
// frameworks like layers and views animation builder.
class DownloadBubbleStartedAnimationViews
    : public DownloadStartedAnimationViews {
  METADATA_HEADER(DownloadBubbleStartedAnimationViews,
                  DownloadStartedAnimationViews)

 public:
  DownloadBubbleStartedAnimationViews(content::WebContents* web_contents,
                                      const gfx::Rect& toolbar_icon_bounds,
                                      SkColor image_foreground_color,
                                      SkColor image_background_color);
  DownloadBubbleStartedAnimationViews(
      const DownloadBubbleStartedAnimationViews&) = delete;
  DownloadBubbleStartedAnimationViews& operator=(
      const DownloadBubbleStartedAnimationViews&) = delete;
  ~DownloadBubbleStartedAnimationViews() override;

 private:
  // DownloadStartedAnimationViews
  int GetX() const override;
  int GetY() const override;
  float GetOpacity() const override;
  bool WebContentsTooSmall(const gfx::Size& image_size) const override;

  // Bounds of the download bubble icon in the screen coordinate system.
  gfx::Rect toolbar_icon_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_STARTED_ANIMATION_VIEWS_H_
