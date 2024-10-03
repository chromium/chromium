// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"

#include <memory>
#include <utility>

#include "components/viz/service/display/direct_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

DelegatedInkPointPixelTestHelper::DelegatedInkPointPixelTestHelper() = default;
DelegatedInkPointPixelTestHelper::~DelegatedInkPointPixelTestHelper() = default;

DelegatedInkPointPixelTestHelper::DelegatedInkPointPixelTestHelper(
    DirectRenderer* renderer)
    : renderer_(renderer) {
  CreateInkRenderer();
}

void DelegatedInkPointPixelTestHelper::SetRendererAndCreateInkRenderer(
    DirectRenderer* renderer) {
  renderer_ = renderer;
  CreateInkRenderer();
}

void DelegatedInkPointPixelTestHelper::DropRenderer() {
  ink_renderer_ = nullptr;
  renderer_ = nullptr;
}

void DelegatedInkPointPixelTestHelper::CreateInkRenderer() {
  auto ink_renderer = std::make_unique<DelegatedInkPointRendererSkia>();
  ink_renderer_ = ink_renderer.get();
  renderer_->SetDelegatedInkPointRendererSkiaForTest(std::move(ink_renderer));
}

void DelegatedInkPointPixelTestHelper::CreateAndSendMetadata(
    const gfx::PointF& point,
    float diameter,
    SkColor4f color,
    base::TimeTicks timestamp,
    const gfx::RectF& presentation_area,
    const std::uint64_t render_pass_id) {
  DCHECK(renderer_);
  // TODO(crbug.com/40219248): Make this function use SkColor4f
  metadata_ = gfx::DelegatedInkMetadata(
      point, diameter, color.toSkColor(), timestamp, presentation_area,
      base::TimeTicks::Now(), /*hovering*/ false, render_pass_id);
  renderer_->SetDelegatedInkMetadata(
      std::make_unique<gfx::DelegatedInkMetadata>(metadata_));
}

void DelegatedInkPointPixelTestHelper::CreateAndSendMetadataFromLastPoint() {
  DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
  CreateAndSendMetadataFromLastPoint(ink_points_.begin()->first);
}

void DelegatedInkPointPixelTestHelper::CreateAndSendMetadataFromLastPoint(
    int32_t pointer_id) {
  DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
  // TODO(crbug.com/40219248): Make this function use SkColor4f
  CreateAndSendMetadata(
      ink_points_[pointer_id].back().point(), metadata_.diameter(),
      SkColor4f::FromColor(metadata_.color()),
      ink_points_[pointer_id].back().timestamp(), metadata_.presentation_area(),
      metadata_.render_pass_id());
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPoint(
    const gfx::PointF& point,
    base::TimeTicks timestamp) {
  CreateAndSendPoint(point, timestamp, /*pointer_id*/ 1);
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPoint(
    const gfx::PointF& point,
    base::TimeTicks timestamp,
    int32_t pointer_id) {
  DCHECK(renderer_);
  ink_points_[pointer_id].emplace_back(point, timestamp, pointer_id);
  ink_renderer_->StoreDelegatedInkPoint(ink_points_[pointer_id].back());
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPointFromLastPoint(
    const gfx::PointF& point) {
  DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
  CreateAndSendPointFromLastPoint(ink_points_.begin()->first, point);
}

void DelegatedInkPointPixelTestHelper::CreateAndSendPointFromLastPoint(
    int32_t pointer_id,
    const gfx::PointF& point) {
  DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
  EXPECT_GT(static_cast<int>(ink_points_[pointer_id].size()), 0);
  CreateAndSendPoint(
      point,
      ink_points_[pointer_id].back().timestamp() + base::Microseconds(10),
      pointer_id);
}

gfx::Rect DelegatedInkPointPixelTestHelper::GetDelegatedInkDamageRect() {
  DCHECK_EQ(static_cast<int>(ink_points_.size()), 1);
  return GetDelegatedInkDamageRect(ink_points_.begin()->first);
}

gfx::Rect DelegatedInkPointPixelTestHelper::GetDelegatedInkDamageRect(
    int32_t pointer_id) {
  DCHECK(ink_points_.find(pointer_id) != ink_points_.end());
  EXPECT_GT(static_cast<int>(ink_points_[pointer_id].size()), 0);
  gfx::RectF ink_damage_rect_f =
      gfx::RectF(ink_points_[pointer_id][0].point(), gfx::SizeF(1, 1));
  for (uint64_t i = 1; i < ink_points_[pointer_id].size(); ++i) {
    ink_damage_rect_f.Union(
        gfx::RectF(ink_points_[pointer_id][i].point(), gfx::SizeF(1, 1)));
  }
  ink_damage_rect_f.Inset(-metadata().diameter() / 2.f);

  return gfx::ToEnclosingRect(ink_damage_rect_f);
}

}  // namespace viz
