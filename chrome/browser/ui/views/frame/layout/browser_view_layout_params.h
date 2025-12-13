// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_PARAMS_H_

#include <ostream>

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

// Represents an area in the upper left or right of the browser window that
// browser UI should be careful when rendering in. This might include caption
// buttons, control box, or app icon.
//
// This is an example of the leading exclusion area in LTR:
//
// ┏━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━┯━━━━━
// ┃ content        │ horizontal │
// ┠────────────────┘     ↔      │
// ┃    vertical ↕     padding   ┊
// ┠─────────────────┄┄┄┄┄┄┄┄┄┄┄┄┘
// ┃
//
struct BrowserLayoutExclusionArea {
  // This is the area which has visual elements managed by the frame. No drawing
  // should occur here.
  gfx::SizeF content;

  // Any additional area next to the content that should remain empty for visual
  // balance. It's okay for edges and borders to be drawn in this space.
  float horizontal_padding = 0.f;

  // Any additional area below the content that should remain empty for visual
  // balance. It's okay for edges and borders to be drawn in this space.
  float vertical_padding = 0.f;

  bool operator==(const BrowserLayoutExclusionArea&) const = default;

  // Returns the content area plus the padding, if any.
  gfx::SizeF ContentWithPadding() const {
    return gfx::SizeF(content.width() + horizontal_padding,
                      content.height() + vertical_padding);
  }

  // As ContentWithPadding(), but subtracts the insets `horizontal_inset` and
  // `vertical_inset` from the margins, with a minimum margin of zero.
  gfx::SizeF ContentWithPaddingAndInsets(float horizontal_inset,
                                         float vertical_inset) const;

  // Returns true if there is no exclusion area.
  bool IsEmpty() const { return ContentWithPadding().IsEmpty(); }
};

// Represents the parameters that the browser's layout requires in order to lay
// out the window contents.
//
// This is how the exclusion areas look in LTR:
// ┏━━━━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━━━━━┓
// ┃ leading_exclusion │              │ trailing_exclusion ┃
// ┠───────────────────┘              └────────────────────┨
// ┃                                                       ┃
//
// Note that in RTL UI, coordinates are reversed, so the leading exclusion is
// still at the lower X coordinate and the trailing exclusion at the higher.
//
// Also note that one or both exclusions may be empty, in which case there is
// no exclusion.
//
struct BrowserLayoutParams {
  // A rectangle in which it is generally safe to lay out browser view elements.
  // This is in window coordinates and may not align with the actual content
  // view. It is okay for the content view to paint outside this rectangle, but
  // that may overlap OS or frame elements.
  gfx::Rect visual_client_area;
  // The area in the leading (lowest X values; i.e. top-left in LTR and top-
  // right in RTL) corner occupied by frame-owned controls, from the edge of the
  // visual client area.
  //
  // It is sometimes okay for the content to draw through the edge of this area,
  // e.g. to draw the leading curve of the first tab. Use the difference between
  // `content` and `content_with_padding` to determine the area it is safe to
  // draw in.
  BrowserLayoutExclusionArea leading_exclusion;
  // The area in the trailing (highest X values; i.e. top-right in LTR and top-
  // left in RTL) corner occupied by frame-owned controls, from the edge of the
  // visual client area.
  //
  // It is sometimes okay for the content to draw through the edge of this area,
  // e.g. to draw the leading curve of the first tab. Use the difference between
  // `content` and `content_with_padding` to determine the area it is safe to
  // draw in.
  BrowserLayoutExclusionArea trailing_exclusion;

  bool operator==(const BrowserLayoutParams&) const = default;

  // Is the visual area empty?
  bool IsEmpty() const;

  // Applies `insets` to the contents area, in-place.
  void Inset(const gfx::Insets& insets);

  // Moves the top of the visual client area down to `top`.
  void SetTop(int top);

  // Insets by `amount` on either the `leading` or (if false) trailing edge, to
  // a minimum of zero width.
  void InsetHorizontal(int amount, bool leading);

  // Returns a new set of params after applying `insets` to the
  // `visual_client_area`; the coordinate system is not changed.
  [[nodiscard]] BrowserLayoutParams WithInsets(const gfx::Insets& insets) const;

  // Returns a new set of params after moving the `visual_client_area` to
  // `new_client_area`, which should be smaller. The coordinate system is not
  // changed.
  [[nodiscard]] BrowserLayoutParams WithClientArea(
      const gfx::Rect& new_client_area) const;

  // Converts this params object to local coordinates in `rect`.
  [[nodiscard]] BrowserLayoutParams InLocalCoordinates(
      const gfx::Rect& rect) const;
};

std::ostream& operator<<(std::ostream& os,
                         const BrowserLayoutExclusionArea& area);
std::ostream& operator<<(std::ostream& os, const BrowserLayoutParams& params);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_PARAMS_H_
