// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_BACKGROUND_BLUR_BACKDROP_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_BACKGROUND_BLUR_BACKDROP_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class VerticalTabStripRegionView;

// A view that appears behind the Vertical Tab Strip when in expand-on-hover
// in Glass mode only. Provides additional effects that are blurred into the
// expand-on-hover background.
class VerticalTabStripBackgroundBlurBackdrop : public views::View {
  METADATA_HEADER(VerticalTabStripBackgroundBlurBackdrop, views::View)
 public:
  VerticalTabStripBackgroundBlurBackdrop();
  ~VerticalTabStripBackgroundBlurBackdrop() override;

  // Call after the vertical tab strip background is configured.
  void UpdateGeometry(const VerticalTabStripRegionView* from, float alpha);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  float alpha_ = 1.0f;
  SkPath border_path_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_BACKGROUND_BLUR_BACKDROP_H_
