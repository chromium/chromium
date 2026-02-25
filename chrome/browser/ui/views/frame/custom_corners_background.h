// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_

#include <variant>

#include "base/types/strong_alias.h"
#include "chrome/browser/ui/views/frame/custom_corners.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

class BrowserView;

// Represents a background with corners that can either be square, rounded, or
// rounded with a background color behind them.
//
// Used by a number of top-level browser elements to prevent overdrawing the
// browser's corners or providing nice curved interfaces between elements.
class CustomCornersBackground : public views::Background, public CustomCorners {
 public:
  // Specifies how a corner should be painted.
  enum class CornerType {
    // Paint all the way to the corner.
    kSquare,
    // Paint only a rounded corner, leaving the actual corner transparent.
    kRounded,
    // Paint a rounded corner with a background color behind.
    kRoundedWithBackground
  };

  // Represents each corner. If `radius` is not specified it will default to
  // `TOOLBAR_CORNER_RADIUS`.
  struct Corner {
    CornerType type = CornerType::kSquare;
    std::optional<int> radius;

    bool operator==(const Corner&) const = default;
  };

  // Specifies how all corners should be rendered.
  struct Corners {
    Corner upper_leading;
    Corner upper_trailing;
    Corner lower_leading;
    Corner lower_trailing;

    bool operator==(const Corners&) const = default;
  };

  // Specifies whether outline strokes should be drawn.
  struct Outline {
    ui::ColorId color = ui::kColorSeparator;
    bool top = false;
    bool leading = false;
    bool bottom = false;
    bool trailing = false;

    bool has_strokes() const { return top || leading || bottom || trailing; }

    bool operator==(const Outline&) const = default;
  };

  CustomCornersBackground(views::View& view,
                          BrowserView& browser_view,
                          ColorChoice primary_color,
                          ColorChoice corner_color,
                          std::optional<int> default_radius = std::nullopt);
  ~CustomCornersBackground() override;

  // Sets whether the background should be painted.
  void SetVisible(bool visible);

  // Sets the color to paint the primary area of the view.
  void SetPrimaryColor(ColorChoice primary_color);

  // Sets the color to paint behind corners of type `kRoundedWithBackground`;
  // default is `FrameColor`.
  void SetCornerColor(ColorChoice corner_color);

  // Sets the corners to use.
  void SetCorners(const Corners& corners);

  // Sets the outline strokes to use.
  void SetOutline(const Outline& outline);

  // Returns an appropriate window corner for the current platform.
  // Specify `upper` to switch between upper (true) and lower (false) corners,
  // as they may be different on some platforms.
  Corner GetWindowCorner(bool upper) const;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;
  std::optional<gfx::RoundedCornersF> GetRoundedCornerRadii() const override;

  // CustomCorners:
  const views::View& GetView() const override;
  void OnBrowserPaintAsActiveChanged() override;
  void SchedulePaintHost() override;

  int default_radius() const { return default_radius_; }

 private:
  // Hide this as it should not be used directly.
  using Background::SetColor;

  // Possibly mirrors corners for RtL.
  Corners GetMirroredCorners() const;

  // Possibly mirrors outline for RtL.
  Outline GetMirroredOutline() const;

  bool visible_ = true;
  ColorChoice primary_color_;
  ColorChoice corner_color_;
  int default_radius_;
  Corners corners_;
  Outline outline_;
  const raw_ref<views::View> view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_
