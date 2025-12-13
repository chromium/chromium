// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"

#include <ostream>

namespace {

// Shrinks an exclusion area by `width` and `height`. If these are larger than
// the content area, they are subtracted from the margins, to a minimum of zero.
void ShrinkBy(BrowserLayoutExclusionArea& area, float width, float height) {
  const float resulting_width = area.content.width() - width;
  const float resulting_height = area.content.height() - height;
  area.content.set_width(std::max(0.f, resulting_width));
  area.content.set_height(std::max(0.f, resulting_height));
  area.horizontal_padding =
      std::max(0.f, area.horizontal_padding + std::min(0.f, resulting_width));
  area.vertical_padding =
      std::max(0.f, area.vertical_padding + std::min(0.f, resulting_height));
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
  auto result = WithClientArea(rect);
  result.visual_client_area.set_origin(gfx::Point());
  return result;
}

BrowserLayoutParams BrowserLayoutParams::WithClientArea(
    const gfx::Rect& new_client_area) const {
  CHECK(visual_client_area.Contains(new_client_area))
      << "Expected " << visual_client_area.ToString() << " to contain "
      << new_client_area.ToString();
  const auto insets = visual_client_area.InsetsFrom(new_client_area);
  return WithInsets(insets);
}

BrowserLayoutParams BrowserLayoutParams::WithInsets(
    const gfx::Insets& insets) const {
  BrowserLayoutParams result = *this;
  result.Inset(insets);
  return result;
}

void BrowserLayoutParams::Inset(const gfx::Insets& insets) {
  visual_client_area.Inset(insets);
  if (visual_client_area.width() < 0) {
    visual_client_area.set_width(0);
  }
  if (visual_client_area.height() < 0) {
    visual_client_area.set_height(0);
  }
  if (!leading_exclusion.IsEmpty()) {
    ShrinkBy(leading_exclusion, insets.left(), insets.top());
  }
  if (!trailing_exclusion.IsEmpty()) {
    ShrinkBy(trailing_exclusion, insets.right(), insets.top());
  }
}

void BrowserLayoutParams::SetTop(int top) {
  Inset(gfx::Insets::TLBR(top - visual_client_area.y(), 0, 0, 0));
}

void BrowserLayoutParams::InsetHorizontal(int amount, bool leading) {
  amount = std::min(amount, visual_client_area.width());
  Inset(leading ? gfx::Insets::TLBR(0, amount, 0, 0)
                : gfx::Insets::TLBR(0, 0, 0, amount));
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
