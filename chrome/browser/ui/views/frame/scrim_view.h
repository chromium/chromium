// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_H_

#include "ui/color/color_variant.h"
#include "ui/views/view.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

// A view that covers whatever is behind it with a scrim that darkens the
// background.
class ScrimView : public views::View {
  METADATA_HEADER(ScrimView, views::View)

 public:
  // Maybe consider using a different color for the scrim?
  // kColorSysStateScrim is a semi-transparent black which has no effect on a
  // pure black background. In contrast, macOS sheet uses a semi-transparent
  // grey scrim which lightens a dark background.
  explicit ScrimView(ui::ColorVariant scrim_color = ui::kColorSysStateScrim);

  ScrimView(const ScrimView&) = delete;
  ScrimView& operator=(const ScrimView&) = delete;

  ~ScrimView() override = default;

  void SetRoundedCorners(const gfx::RoundedCornersF& radii);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_H_
