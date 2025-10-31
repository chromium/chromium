// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"

#include <ostream>

namespace {

// Returns an exclusion area shrunk by `width` and `height`.
// If these are larger than the content area, they are subtracted from the
// margins.
BrowserLayoutExclusionArea ShrinkBy(const BrowserLayoutExclusionArea& area,
                                    float width,
                                    float height) {
  BrowserLayoutExclusionArea result;
  const float resulting_width = area.content.width() - width;
  const float resulting_height = area.content.height() - height;
  result.content.set_width(std::max(0.f, resulting_width));
  result.content.set_height(std::max(0.f, resulting_height));
  result.horizontal_padding =
      std::max(0.f, area.horizontal_padding + std::min(0.f, resulting_width));
  result.vertical_padding =
      std::max(0.f, area.vertical_padding + std::min(0.f, resulting_height));
  return result;
}

}  // namespace

gfx::SizeF BrowserLayoutExclusionArea::ContentWithPaddingAndInsets(
    float horizontal_inset,
    float vertical_inset) const {
  return gfx::SizeF(
      content.width() + std::max(0.f, horizontal_padding - horizontal_inset),
      content.height() + std::max(0.f, vertical_padding - vertical_inset));
}

bool BrowserLayoutParams::IsEmpty() const {
  return visual_client_area.IsEmpty();
}

BrowserLayoutParams BrowserLayoutParams::InLocalCoordinates(
    const gfx::Rect& rect) const {
  CHECK(visual_client_area.Contains(rect))
      << "Expected " << visual_client_area.ToString() << " to contain "
      << rect.ToString();
  BrowserLayoutParams result;
  result.visual_client_area.set_size(rect.size());
  const auto insets = visual_client_area.InsetsFrom(rect);
  if (!leading_exclusion.IsEmpty()) {
    result.leading_exclusion =
        ShrinkBy(leading_exclusion, insets.left(), insets.top());
  }
  if (!trailing_exclusion.IsEmpty()) {
    result.trailing_exclusion =
        ShrinkBy(trailing_exclusion, insets.right(), insets.top());
  }
  return result;
}

std::ostream& operator<<(std::ostream& os,
                         const BrowserLayoutExclusionArea& exclusion) {
  os << exclusion.content.ToString() << " +h: " << exclusion.horizontal_padding
     << " +v: " << exclusion.vertical_padding;
  return os;
}

std::ostream& operator<<(std::ostream& os, const BrowserLayoutParams& params) {
  os << "client: " << params.visual_client_area.ToString() << " leading: { "
     << params.leading_exclusion << "} trailing: { "
     << params.trailing_exclusion << " }";
  return os;
}
