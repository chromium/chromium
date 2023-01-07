// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_ARROW_BORDER_H_
#define CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_ARROW_BORDER_H_

#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"

namespace gfx {
class Canvas;
struct VectorIcon;
}  // namespace gfx
namespace views {
class View;
}

// A Border that paints a border around a view with an arrow pointing outward to
// some location. For example:
// ___________________________
// |      View contents      |
// |_________    ____________|
//           \  /
//            \/
// The owner must provide the bounding rectangle of of the arrow in screen
// coordinates (pixels) via set_arrow_bounds().
class ArrowBorder : public views::Border {
 public:
  // The amount by which the arrow icon must be rotated prior to painting.
  enum class ArrowRotation { kNone, k90Degrees, k180Degrees, k270Degrees };

  // Properties for an orientation-specific border with a arrow.
  struct Properties {
    // The amount by which the arrow is inset on a single side of the border.
    gfx::Insets insets;

    // The insets into the bounding rectangle of the arrow into which the
    // popup's border should extend.
    gfx::Insets arrow_border_insets;

    // The rotation to be applied to the arrow icon to orient it properly.
    ArrowRotation arrow_rotation;
  };

  // Creates a border of |thickness| (DIP) and |color| for a view with
  // |background_color| using |arrow_icon| to paint the arrow itself.
  // |properties| indicates details for positioning the arrow.
  ArrowBorder(int thickness,
              ui::ColorId color,
              ui::ColorId background_color,
              const gfx::VectorIcon& arrow_icon,
              const Properties* properties);

  ArrowBorder(const ArrowBorder&) = delete;
  ArrowBorder& operator=(const ArrowBorder&) = delete;

  // Sets the bounds of the arrow in pixels relative to the containing widget.
  void set_arrow_bounds(const gfx::Rect& arrow_bounds) {
    arrow_bounds_ = arrow_bounds;
  }

 private:
  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

  // The region occupied by the border.
  const gfx::Insets insets_;

  const ui::ColorId color_;

  // The insets into the bounding rectangle of the arrow into which the popup's
  // border should extend.
  const gfx::Insets arrow_border_insets_;

  const ArrowRotation arrow_rotation_;

  // The arrow image to be painted in the border.
  ui::ImageModel arrow_;

  // The bounding rectangle of the arrow, in pixels, relative to the window's
  // client area. This rectangle may extend into the contents of the popup
  // (including its border).
  gfx::Rect arrow_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_ARROW_BORDER_H_
