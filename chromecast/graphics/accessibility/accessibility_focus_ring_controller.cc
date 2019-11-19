// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#include "chromecast/graphics/accessibility/accessibility_focus_ring_controller.h"

#include <cstddef>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "chromecast/graphics/accessibility/accessibility_cursor_ring_layer.h"
#include "chromecast/graphics/accessibility/accessibility_focus_ring_layer.h"
#include "chromecast/graphics/accessibility/accessibility_highlight_layer.h"
#include "chromecast/graphics/accessibility/focus_ring_layer.h"

namespace chromecast {

namespace {

// The number of pixels the focus ring is outset from the object it outlines,
// which also determines the border radius of the rounded corners.
// TODO(dmazzoni): take display resolution into account.
constexpr int kAccessibilityFocusRingMargin = 7;

// Time to transition between one location and the next.
constexpr int kTransitionTimeMilliseconds = 300;

// Focus constants.
constexpr int kFocusFadeInTimeMilliseconds = 100;
constexpr int kFocusFadeOutTimeMilliseconds = 1600;

// Caret constants.
constexpr int kCaretFadeInTimeMilliseconds = 100;
constexpr int kCaretFadeOutTimeMilliseconds = 1600;
constexpr int kCaretRingColorRed = 51;
constexpr int kCaretRingColorGreen = 51;
constexpr int kCaretRingColorBlue = 255;

// Highlight constants.
constexpr float kHighlightOpacity = 0.3f;

// A Region is an unordered collection of Rects that maintains its
// bounding box. Used in the middle of an algorithm that groups
// adjacent and overlapping rects.
struct Region {
  explicit Region(gfx::Rect initial_rect) {
    bounds = initial_rect;
    rects.push_back(initial_rect);
  }
  gfx::Rect bounds;
  std::vector<gfx::Rect> rects;
};

}  // namespace

AccessibilityFocusRingController::AccessibilityFocusRingController(
    aura::Window* root_window)
    : root_window_(root_window) {
  DCHECK(root_window);
  focus_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kFocusFadeInTimeMilliseconds);
  focus_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kFocusFadeOutTimeMilliseconds);
  caret_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeInTimeMilliseconds);
  caret_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeOutTimeMilliseconds);
}

AccessibilityFocusRingController::~AccessibilityFocusRingController() = default;

void AccessibilityFocusRingController::SetFocusRingColor(SkColor color) {
  focus_ring_color_ = color;
  UpdateFocusRingsFromFocusRects();
}

void AccessibilityFocusRingController::ResetFocusRingColor() {
  focus_ring_color_.reset();
  UpdateFocusRingsFromFocusRects();
}

void AccessibilityFocusRingController::SetFocusRing(
    const std::vector<gfx::Rect>& rects,
    FocusRingBehavior focus_ring_behavior) {
  std::vector<gfx::Rect> clean_rects(rects);
  // Remove duplicates
  if (rects.size() > 1) {
    std::set<gfx::Rect> rects_set(rects.begin(), rects.end());
    clean_rects.assign(rects_set.begin(), rects_set.end());
  }
  // If there is no change, don't do any work.
  if (focus_ring_behavior_ == focus_ring_behavior &&
      clean_rects == focus_rects_)
    return;
  focus_ring_behavior_ = focus_ring_behavior;
  OnLayerChange(&focus_animation_info_);
  focus_rects_ = clean_rects;
  UpdateFocusRingsFromFocusRects();
}

void AccessibilityFocusRingController::HideFocusRing() {
  focus_rects_.clear();
  UpdateFocusRingsFromFocusRects();
}

void AccessibilityFocusRingController::SetHighlights(
    const std::vector<gfx::Rect>& rects,
    SkColor color) {
  highlight_rects_ = rects;
  GetColorAndOpacityFromColor(color, kHighlightOpacity, &highlight_color_,
                              &highlight_opacity_);
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingController::HideHighlights() {
  highlight_rects_.clear();
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingController::UpdateFocusRingsFromFocusRects() {
  previous_focus_rings_.swap(focus_rings_);
  focus_rings_.clear();
  RectsToRings(focus_rects_, &focus_rings_);
  focus_layers_.resize(focus_rings_.size());
  if (focus_rings_.empty())
    return;

  for (size_t i = 0; i < focus_rings_.size(); ++i) {
    if (!focus_layers_[i])
      focus_layers_[i] =
          std::make_unique<AccessibilityFocusRingLayer>(root_window_, this);
  }

  if (focus_ring_behavior_ == FocusRingBehavior::PERSIST_FOCUS_RING &&
      focus_layers_[0]->CanAnimate()) {
    // In PERSIST mode, animate the first ring to its destination
    // location, then set the rest of the rings directly.
    for (size_t i = 1; i < focus_rings_.size(); ++i)
      focus_layers_[i]->Set(focus_rings_[i]);
  } else {
    // In FADE mode, set all focus rings to their destination location.
    for (size_t i = 0; i < focus_rings_.size(); ++i)
      focus_layers_[i]->Set(focus_rings_[i]);
  }

  for (size_t i = 0; i < focus_rings_.size(); ++i) {
    if (focus_ring_color_)
      focus_layers_[i]->SetColor(*focus_ring_color_);
    else
      focus_layers_[i]->ResetColor();
  }
}

void AccessibilityFocusRingController::UpdateHighlightFromHighlightRects() {
  if (!highlight_layer_)
    highlight_layer_ =
        std::make_unique<AccessibilityHighlightLayer>(root_window_, this);
  highlight_layer_->Set(highlight_rects_, highlight_color_);
  highlight_layer_->SetOpacity(highlight_opacity_);
}

void AccessibilityFocusRingController::OnLayerChange(
    AccessibilityFocusRingController::LayerAnimationInfo* animation_info) {
  animation_info->change_time = base::TimeTicks::Now();
  if (animation_info->opacity == 0)
    animation_info->start_time = animation_info->change_time;
}

void AccessibilityFocusRingController::SetCaretRing(
    const gfx::Point& location) {
  caret_location_ = location;

  if (!caret_layer_) {
    caret_layer_.reset(new AccessibilityCursorRingLayer(
        root_window_, this, kCaretRingColorRed, kCaretRingColorGreen,
        kCaretRingColorBlue));
  }

  caret_layer_->Set(location);
  OnLayerChange(&caret_animation_info_);
}

void AccessibilityFocusRingController::HideCaretRing() {
  caret_layer_.reset();
}

void AccessibilityFocusRingController::SetNoFadeForTesting() {
  focus_animation_info_.fade_in_time = base::TimeDelta();
  focus_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
  caret_animation_info_.fade_in_time = base::TimeDelta();
  caret_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
}

void AccessibilityFocusRingController::RectsToRings(
    const std::vector<gfx::Rect>& src_rects,
    std::vector<AccessibilityFocusRing>* rings) const {
  if (src_rects.empty())
    return;

  // Give all of the rects a margin.
  std::vector<gfx::Rect> rects;
  rects.resize(src_rects.size());
  for (size_t i = 0; i < src_rects.size(); ++i) {
    rects[i] = src_rects[i];
    rects[i].Inset(-GetMargin(), -GetMargin());
  }

  // Split the rects into contiguous regions.
  std::vector<Region> regions;
  regions.push_back(Region(rects[0]));
  for (size_t i = 1; i < rects.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < regions.size(); ++j) {
      if (Intersects(rects[i], regions[j].bounds)) {
        regions[j].rects.push_back(rects[i]);
        regions[j].bounds.Union(rects[i]);
        found = true;
      }
    }
    if (!found) {
      regions.push_back(Region(rects[i]));
    }
  }

  // Keep merging regions that intersect.
  // TODO(dmazzoni): reduce the worst-case complexity! This appears like
  // it could be O(n^3), make sure it's not in practice.
  bool merged;
  do {
    merged = false;
    for (size_t i = 0; i < regions.size() - 1 && !merged; ++i) {
      for (size_t j = i + 1; j < regions.size() && !merged; ++j) {
        if (Intersects(regions[i].bounds, regions[j].bounds)) {
          regions[i].rects.insert(regions[i].rects.end(),
                                  regions[j].rects.begin(),
                                  regions[j].rects.end());
          regions[i].bounds.Union(regions[j].bounds);
          regions.erase(regions.begin() + j);
          merged = true;
        }
      }
    }
  } while (merged);

  for (size_t i = 0; i < regions.size(); ++i) {
    std::sort(regions[i].rects.begin(), regions[i].rects.end());
    rings->push_back(RingFromSortedRects(regions[i].rects));
  }
}

int AccessibilityFocusRingController::GetMargin() const {
  return kAccessibilityFocusRingMargin;
}

void AccessibilityFocusRingController::GetColorAndOpacityFromColor(
    SkColor color,
    float default_opacity,
    SkColor* result_color,
    float* result_opacity) {
  int alpha = SkColorGetA(color);
  if (alpha == 0xFF) {
    *result_opacity = default_opacity;
  } else {
    *result_opacity = SkColor4f::FromColor(color).fA;
  }
  *result_color = SkColorSetA(color, 0xFF);
}

// Given a vector of rects that all overlap, already sorted from top to bottom
// and left to right, split them into three shapes covering the top, middle,
// and bottom of a "paragraph shape".
//
// Input:
//
//                       +---+---+
//                       | 1 | 2 |
// +---------------------+---+---+
// |             3               |
// +--------+---------------+----+
// |    4   |         5     |
// +--------+---------------+--+
// |             6             |
// +---------+-----------------+
// |    7    |
// +---------+
//
// Output:
//
//                       +-------+
//                       |  Top  |
// +---------------------+-------+
// |                             |
// |                             |
// |           Middle            |
// |                             |
// |                             |
// +---------+-------------------+
// | Bottom  |
// +---------+
//
// When there's no clear "top" or "bottom" segment, split the overall rect
// evenly so that some of the area still fits into the "top" and "bottom"
// segments.
void AccessibilityFocusRingController::SplitIntoParagraphShape(
    const std::vector<gfx::Rect>& rects,
    gfx::Rect* top,
    gfx::Rect* middle,
    gfx::Rect* bottom) const {
  size_t n = rects.size();

  // Figure out how many rects belong in the top portion.
  gfx::Rect top_rect = rects[0];
  int top_middle = (top_rect.y() + top_rect.bottom()) / 2;
  size_t top_count = 1;
  while (top_count < n && rects[top_count].y() < top_middle) {
    top_rect.Union(rects[top_count]);
    top_middle = (top_rect.y() + top_rect.bottom()) / 2;
    top_count++;
  }

  // Figure out how many rects belong in the bottom portion.
  gfx::Rect bottom_rect = rects[n - 1];
  int bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
  size_t bottom_count = std::min(static_cast<size_t>(1), n - top_count);
  while (bottom_count + top_count < n &&
         rects[n - bottom_count - 1].bottom() > bottom_middle) {
    bottom_rect.Union(rects[n - bottom_count - 1]);
    bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
    bottom_count++;
  }

  // Whatever's left goes to the middle rect, but if there's no middle or
  // bottom rect, split the existing rects evenly to make one.
  gfx::Rect middle_rect;
  if (top_count + bottom_count < n) {
    middle_rect = rects[top_count];
    for (size_t i = top_count + 1; i < n - bottom_count; i++)
      middle_rect.Union(rects[i]);
  } else if (bottom_count > 0) {
    gfx::Rect enclosing_rect = top_rect;
    enclosing_rect.Union(bottom_rect);
    int middle_top = (top_rect.y() + top_rect.bottom() * 2) / 3;
    int middle_bottom = (bottom_rect.y() * 2 + bottom_rect.bottom()) / 3;
    top_rect.set_height(middle_top - top_rect.y());
    bottom_rect.set_height(bottom_rect.bottom() - middle_bottom);
    bottom_rect.set_y(middle_bottom);
    middle_rect = gfx::Rect(enclosing_rect.x(), middle_top,
                            enclosing_rect.width(), middle_bottom - middle_top);
  } else {
    int middle_top = (top_rect.y() * 2 + top_rect.bottom()) / 3;
    int middle_bottom = (top_rect.y() + top_rect.bottom() * 2) / 3;
    middle_rect = gfx::Rect(top_rect.x(), middle_top, top_rect.width(),
                            middle_bottom - middle_top);
    bottom_rect = gfx::Rect(top_rect.x(), middle_bottom, top_rect.width(),
                            top_rect.bottom() - middle_bottom);
    top_rect.set_height(middle_top - top_rect.y());
  }

  if (middle_rect.y() > top_rect.bottom()) {
    middle_rect.set_height(middle_rect.height() + middle_rect.y() -
                           top_rect.bottom());
    middle_rect.set_y(top_rect.bottom());
  }

  if (middle_rect.bottom() < bottom_rect.y()) {
    middle_rect.set_height(bottom_rect.y() - middle_rect.y());
  }

  *top = top_rect;
  *middle = middle_rect;
  *bottom = bottom_rect;
}

AccessibilityFocusRing AccessibilityFocusRingController::RingFromSortedRects(
    const std::vector<gfx::Rect>& rects) const {
  if (rects.size() == 1)
    return AccessibilityFocusRing::CreateWithRect(rects[0], GetMargin());

  gfx::Rect top;
  gfx::Rect middle;
  gfx::Rect bottom;
  SplitIntoParagraphShape(rects, &top, &middle, &bottom);

  return AccessibilityFocusRing::CreateWithParagraphShape(top, middle, bottom,
                                                          GetMargin());
}

bool AccessibilityFocusRingController::Intersects(const gfx::Rect& r1,
                                                  const gfx::Rect& r2) const {
  int slop = GetMargin();
  return (r2.x() <= r1.right() + slop && r2.right() >= r1.x() - slop &&
          r2.y() <= r1.bottom() + slop && r2.bottom() >= r1.y() - slop);
}

void AccessibilityFocusRingController::OnDeviceScaleFactorChanged() {
  UpdateFocusRingsFromFocusRects();
}

void AccessibilityFocusRingController::OnAnimationStep(
    base::TimeTicks timestamp) {
  if (!focus_rings_.empty() && focus_layers_[0]->CanAnimate())
    AnimateFocusRings(timestamp);

  if (caret_layer_ && caret_layer_->CanAnimate())
    AnimateCaretRing(timestamp);
}

void AccessibilityFocusRingController::AnimateFocusRings(
    base::TimeTicks timestamp) {
  CHECK(!focus_rings_.empty());
  CHECK(!focus_layers_.empty());
  CHECK(focus_layers_[0]);

  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // focus change, so we just treat those as a delta of zero.
  if (timestamp < focus_animation_info_.change_time)
    timestamp = focus_animation_info_.change_time;

  if (focus_ring_behavior_ == FocusRingBehavior::PERSIST_FOCUS_RING) {
    base::TimeDelta delta = timestamp - focus_animation_info_.change_time;
    base::TimeDelta transition_time =
        base::TimeDelta::FromMilliseconds(kTransitionTimeMilliseconds);
    if (delta >= transition_time) {
      focus_layers_[0]->Set(focus_rings_[0]);
      return;
    }

    double fraction = delta.InSecondsF() / transition_time.InSecondsF();

    // Ease-in effect.
    fraction = pow(fraction, 0.3);

    // Handle corner case where we're animating but we don't have previous
    // rings.
    if (previous_focus_rings_.empty())
      previous_focus_rings_ = focus_rings_;

    focus_layers_[0]->Set(AccessibilityFocusRing::Interpolate(
        previous_focus_rings_[0], focus_rings_[0], fraction));
  } else {
    ComputeOpacity(&focus_animation_info_, timestamp);
    for (size_t i = 0; i < focus_layers_.size(); ++i)
      focus_layers_[i]->SetOpacity(focus_animation_info_.opacity);
  }
}

void AccessibilityFocusRingController::ComputeOpacity(
    AccessibilityFocusRingController::LayerAnimationInfo* animation_info,
    base::TimeTicks timestamp) {
  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // mouse movement, so we just treat those as a delta of zero.
  if (timestamp < animation_info->start_time)
    timestamp = animation_info->start_time;

  base::TimeDelta start_delta = timestamp - animation_info->start_time;
  base::TimeDelta change_delta = timestamp - animation_info->change_time;
  base::TimeDelta fade_in_time = animation_info->fade_in_time;
  base::TimeDelta fade_out_time = animation_info->fade_out_time;

  if (change_delta > fade_in_time + fade_out_time) {
    animation_info->opacity = 0.0;
    return;
  }

  float opacity;
  if (start_delta < fade_in_time) {
    opacity = start_delta.InSecondsF() / fade_in_time.InSecondsF();
  } else {
    opacity = 1.0 - (change_delta.InSecondsF() /
                     (fade_in_time + fade_out_time).InSecondsF());
  }

  // Layer::SetOpacity will throw an error if we're not within 0...1.
  opacity = base::ClampToRange(opacity, 0.0f, 1.0f);

  animation_info->opacity = opacity;
}

void AccessibilityFocusRingController::AnimateCaretRing(
    base::TimeTicks timestamp) {
  CHECK(caret_layer_);

  ComputeOpacity(&caret_animation_info_, timestamp);
  if (caret_animation_info_.opacity == 0.0) {
    caret_layer_.reset();
    return;
  }
  caret_layer_->SetOpacity(caret_animation_info_.opacity);
}

}  // namespace chromecast
