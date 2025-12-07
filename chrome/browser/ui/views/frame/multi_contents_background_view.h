// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_BACKGROUND_VIEW_H_

#include "ui/views/view.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

// MultiContentsBackgroundView prioritizes using a `ui::LAYER_SOLID_COLOR` for
// background painting whenever possible (or `ui::LAYER_TEXTURED`). This method
// is more efficient than painting directly onto the widget's texture layer.
class MultiContentsBackgroundView : public views::View {
  METADATA_HEADER(MultiContentsBackgroundView, views::View)
 public:
  explicit MultiContentsBackgroundView(BrowserView* browser_view);

  MultiContentsBackgroundView(const MultiContentsBackgroundView&) = delete;
  MultiContentsBackgroundView& operator=(const MultiContentsBackgroundView&) =
      delete;

  ~MultiContentsBackgroundView() override;

  void SetRoundedCorners(const gfx::RoundedCornersF& radii);
  const gfx::RoundedCornersF& GetRoundedCorners() const;

  // views::View:
  void OnThemeChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  ui::LayerType CalculateLayerType() const;

  void UpdateSolidLayerColor();

  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_BACKGROUND_VIEW_H_
