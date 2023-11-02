// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

namespace vr {

namespace {

// NB: this function makes no attempt to account for rotated elements.
float GetExtent(const UiElement& element, bool horizontal) {
  gfx::Point3F p = horizontal ? gfx::Point3F(element.size().width(), 0, 0)
                              : gfx::Point3F(0, element.size().height(), 0);
  gfx::Point3F o;
  p = element.LocalTransform().MapPoint(p);
  o = element.LocalTransform().MapPoint(o);
  return (p - o).Length();
}

}  // namespace

LinearLayout::LinearLayout(Direction direction) : direction_(direction) {}
LinearLayout::~LinearLayout() {}

bool LinearLayout::SizeAndLayOutChildren() {
  bool changed = UiElement::SizeAndLayOutChildren();
  if (layout_length_ == 0.0f)
    return changed;

  // We need to adjust one of the elements' size to ensure a fixed total layout
  // width.  Find that element, set its size, and lay it out again.
  UiElement* element_to_resize = nullptr;
  for (auto& child : children()) {
    if (child->resizable_by_layout()) {
      element_to_resize = child.get();
      break;
    }
  }
  DCHECK_NE(element_to_resize, nullptr);
  changed |= AdjustResizableElement(element_to_resize);
  changed |= element_to_resize->SizeAndLayOut();
  return changed;
}

void LinearLayout::LayOutContributingChildren() {
  float x_factor = 0.f;
  float y_factor = 0.f;
  switch (direction_) {
    case kUp:
      y_factor = 1.f;
      break;
    case kDown:
      y_factor = -1.f;
      break;
    case kLeft:
      x_factor = -1.f;
      break;
    case kRight:
      x_factor = 1.f;
      break;
  }

  float total_extent;
  float minor_extent;
  GetTotalExtent(nullptr, &total_extent, &minor_extent);

  bool horizontal = Horizontal();
  float cumulative_offset = -0.5 * total_extent;
  for (auto& child : children()) {
    if (!child->IsVisible() || !child->requires_layout())
      continue;
    float child_extent = GetExtent(*child, horizontal);
    float child_minor_extent = GetExtent(*child, !horizontal);
    float offset = cumulative_offset + 0.5 * child_extent;
    float x_align_offset = 0.0f;
    float y_align_offset = 0.0f;
    if (Horizontal()) {
      DCHECK_NE(RIGHT, child->x_anchoring());
      DCHECK_NE(LEFT, child->x_anchoring());
      if (child->y_anchoring() == TOP || child->y_anchoring() == BOTTOM) {
        y_align_offset = 0.5f * (minor_extent - child_minor_extent);
        if (child->y_anchoring() == BOTTOM)
          y_align_offset *= -1.0f;
      }
    } else {
      DCHECK_NE(TOP, child->y_anchoring());
      DCHECK_NE(BOTTOM, child->y_anchoring());
      if (child->x_anchoring() == RIGHT || child->x_anchoring() == LEFT) {
        x_align_offset = 0.5f * (minor_extent - child_minor_extent);
        if (child->x_anchoring() == LEFT)
          x_align_offset *= -1.0f;
      }
    }
    child->SetLayoutOffset(offset * x_factor + x_align_offset,
                           offset * y_factor + y_align_offset);
    cumulative_offset += child_extent + margin_;
  }

  SetSize(horizontal ? total_extent : minor_extent,
          !horizontal ? total_extent : minor_extent);
}

bool LinearLayout::Horizontal() const {
  return direction_ == LinearLayout::kLeft ||
         direction_ == LinearLayout::kRight;
}

void LinearLayout::GetTotalExtent(const UiElement* element_to_exclude,
                                  float* major_extent,
                                  float* minor_extent) const {
  *major_extent = -margin_;
  *minor_extent = 0.f;
  bool horizontal = Horizontal();
  for (auto& child : children()) {
    if (child->IsVisible() && child->requires_layout()) {
      *major_extent += margin_;
      if (child.get() != element_to_exclude) {
        *major_extent += GetExtent(*child, horizontal);
        *minor_extent = std::max(*minor_extent, GetExtent(*child, !horizontal));
      }
    }
  }
}

bool LinearLayout::AdjustResizableElement(UiElement* element_to_resize) {
  // Figure out how much space is available for the variable element.
  float minimum_total = 0;
  float minor = 0;
  GetTotalExtent(element_to_resize, &minimum_total, &minor);

  float extent = layout_length_ - minimum_total;
  extent = std::max(extent, 0.f);

  auto new_size = element_to_resize->size();
  if (Horizontal())
    new_size.set_width(extent);
  else
    new_size.set_height(extent);
  if (element_to_resize->size() == new_size)
    return false;

  element_to_resize->SetSize(new_size.width(), new_size.height());
  return true;
}

}  // namespace vr
