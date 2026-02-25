// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_FLOATING_CORNER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_FLOATING_CORNER_H_

#include <optional>
#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/views/frame/custom_corners.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

class BrowserView;

// Represents a single, floating, curved corner with an optional stroke.
class CustomFloatingCorner : public views::View, public CustomCorners {
  METADATA_HEADER(CustomFloatingCorner, views::View)

 public:
  // Specifies which corner something refers to.
  // Currently only top corners are fully supported; bottom corners do not
  // support strokes (as they are not yet needed).
  enum class CornerOrientation {
    kTopLeading,
    kTopTrailing,
    kBottomLeading,
    kBottomTrailing,
  };

  CustomFloatingCorner(BrowserView& browser_view,
                       CornerOrientation orientation,
                       views::ShapeContextTokens corner_radius_token,
                       ColorChoice color,
                       std::optional<ui::ColorId> stroke_color = std::nullopt,
                       bool is_vertical_window_edge = false);
  ~CustomFloatingCorner() override;

  // Sets the color to paint the corner.
  void SetColor(ColorChoice color);

  // Sets the corner orientation.
  void SetOrientation(CornerOrientation orientation);

  // Set the corner radius.
  void SetCornerRadius(views::ShapeContextTokens corner_radius_token);

  // Sets a stroke, or no stroke (std::nullopt). If `is_vertical_window_edge` is
  // true, the stroke ends outside the vertical bounds of the corner.
  void SetStroke(std::optional<ui::ColorId> stroke_color,
                 bool is_vertical_window_edge);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  CornerOrientation orientation_for_testing() const { return orientation_; }

 private:
  // CustomCorners:
  const views::View& GetView() const override;
  void OnBrowserPaintAsActiveChanged() override;
  void SchedulePaintHost() override;

  CornerOrientation orientation_;
  views::ShapeContextTokens corner_radius_token_;
  ColorChoice color_;
  std::optional<ui::ColorId> stroke_color_;
  bool is_vertical_window_edge_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_FLOATING_CORNER_H_
