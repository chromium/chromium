// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"

#include "components/viz/service/display/direct_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

DelegatedInkPointPixelTestHelper::DelegatedInkPointPixelTestHelper() = default;
DelegatedInkPointPixelTestHelper::~DelegatedInkPointPixelTestHelper() = default;

DelegatedInkPointPixelTestHelper::DelegatedInkPointPixelTestHelper(
    DirectRenderer* renderer)
    : renderer_(renderer) {
  renderer_->CreateDelegatedInkPointRenderer();
}

void DelegatedInkPointPixelTestHelper::SetRendererAndCreateInkRenderer(
    DirectRenderer* renderer) {
  renderer_ = renderer;
  renderer_->CreateDelegatedInkPointRenderer();
}

DelegatedInkPointRendererBase*
DelegatedInkPointPixelTestHelper::GetInkRenderer() {
  return renderer_->GetDelegatedInkPointRenderer();
}

void DelegatedInkPointPixelTestHelper::CreateAndSendMetadata(
    const gfx::PointF& point,
    float diameter,
    SkColor color,
    const gfx::RectF& presentation_area) {
  DCHECK(renderer_);
  metadata_ =
      DelegatedInkMetadata(point, diameter, color, base::TimeTicks::Now(),
                           presentation_area, base::TimeTicks::Now());
  GetInkRenderer()->SetDelegatedInkMetadata(
      std::make_unique<DelegatedInkMetadata>(metadata_));
}

void DelegatedInkPointPixelTestHelper::CreateAndSendMetadataFromLastPoint() {
  DCHECK(renderer_);
  metadata_ = DelegatedInkMetadata(
      ink_points_.back().point(), metadata_.diameter(), metadata_.color(),
      ink_points_.back().timestamp(), metadata_.presentation_area(),
      metadata_.frame_time());
  GetInkRenderer()->SetDelegatedInkMetadata(
      std::make_unique<DelegatedInkMetadata>(metadata_));
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPoint(
    const gfx::PointF& point,
    base::TimeTicks timestamp) {
  DCHECK(renderer_);
  ink_points_.emplace_back(point, timestamp);
  GetInkRenderer()->StoreDelegatedInkPoint(ink_points_.back());
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPointFromMetadata() {
  CreateAndSendPoint(metadata().point(), metadata().timestamp());
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPointFromLastPoint(
    const gfx::PointF& point) {
  EXPECT_GT(static_cast<int>(ink_points_.size()), 0);
  CreateAndSendPoint(point, ink_points_.back().timestamp() +
                                base::TimeDelta::FromMicroseconds(10));
}

gfx::Rect DelegatedInkPointPixelTestHelper::GetDelegatedInkDamageRect() {
  EXPECT_GT(static_cast<int>(ink_points_.size()), 0);
  gfx::RectF ink_damage_rect_f =
      gfx::RectF(ink_points_[0].point(), gfx::SizeF(1, 1));
  for (uint64_t i = 1; i < ink_points_.size(); ++i) {
    ink_damage_rect_f.Union(
        gfx::RectF(ink_points_[i].point(), gfx::SizeF(1, 1)));
  }
  ink_damage_rect_f.Inset(-metadata().diameter() / 2.f,
                          -metadata().diameter() / 2.f);

  return gfx::ToEnclosingRect(ink_damage_rect_f);
}

}  // namespace viz
