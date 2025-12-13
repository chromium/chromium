// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_ROUNDED_CORNER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_ROUNDED_CORNER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

class BrowserView;

namespace gfx {
class Canvas;
}  // namespace gfx

// Draws a rounded corner on the web contents.
class ContentsRoundedCorner : public views::View {
  METADATA_HEADER(ContentsRoundedCorner, views::View)

 public:
  explicit ContentsRoundedCorner(
      BrowserView* browser_view,
      views::ShapeContextTokens corner_radius_token,
      base::RepeatingCallback<bool()> is_right_aligned_callback);
  ~ContentsRoundedCorner() override;

  // views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const views::ShapeContextTokens corner_radius_token_;
  const base::RepeatingCallback<bool()> is_right_aligned_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_ROUNDED_CORNER_H_
