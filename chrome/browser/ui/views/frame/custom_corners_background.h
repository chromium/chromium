// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_

#include <variant>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/color/color_variant.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class BrowserView;

// Represents a background with corners that can either be square, rounded, or
// rounded with a background color behind them.
//
// Used by a number of top-level browser elements to prevent overdrawing the
// browser's corners or providing nice curved interfaces between elements.
class CustomCornersBackground : public views::Background,
                                public views::ViewObserver {
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

  // Designates that the current frame color (either active or inactive) should
  // be used. If you want to force e.g. active color, use `kColorFrameActive`
  // instead.
  using FrameColor = base::StrongAlias<class FrameColorTag, std::monostate>;

  // Designates that the top container theme background should be used.
  using TopContainerTheme =
      base::StrongAlias<class TopContainerThemeTag, std::monostate>;

  // Specifies which color to be used for the background.
  using ColorChoice = std::variant<FrameColor, TopContainerTheme, ui::ColorId>;

  CustomCornersBackground(views::View& view,
                          BrowserView& browser_view,
                          ColorChoice primary_color,
                          ColorChoice corner_color);
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

  // Returns an appropriate window corner for the current platform.
  static Corner GetWindowCorner();

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;
  std::optional<gfx::RoundedCornersF> GetRoundedCornerRadii() const override;

 private:
  // Hide this as it should not be used directly.
  using Background::SetColor;

  // Paints the given `path` on `canvas` using `color_choice`.
  void PaintPath(gfx::Canvas* canvas,
                 const SkPath& path,
                 ColorChoice color_choice,
                 bool anti_alias) const;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View*) override;

  // Called when the browser's paint-as-active state changes.
  void OnBrowserPaintAsActiveChanged();

  bool visible_ = true;
  ColorChoice primary_color_;
  ColorChoice corner_color_;
  Corners corners_;
  const raw_ref<views::View> view_;
  const raw_ref<const BrowserView> browser_view_;
  base::CallbackListSubscription browser_frame_active_subscription_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_BACKGROUND_H_
