// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/offset_tag.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/outsets_f.h"

namespace viz {

OffsetTag OffsetTag::CreateRandom() {
  return OffsetTag(base::Token::CreateRandom());
}

std::string OffsetTag::ToString() const {
  return token_.ToString().substr(0, 8) + "...";
}

bool OffsetTagValue::IsValid() const {
  return !!tag;
}

std::string OffsetTagValue::ToString() const {
  return base::StringPrintf("%s=[%g %g]", tag.ToString().c_str(), offset.x(),
                            offset.y());
}

OffsetTagConstraints::OffsetTagConstraints() = default;

OffsetTagConstraints::OffsetTagConstraints(float min_x,
                                           float max_x,
                                           float min_y,
                                           float max_y)
    : min_offset(min_x, min_y), max_offset(max_x, max_y) {
  DCHECK(IsValid());
}

gfx::Vector2dF OffsetTagConstraints::Clamp(gfx::Vector2dF value) const {
  DCHECK(IsValid());
  return gfx::Vector2dF(std::clamp(value.x(), min_offset.x(), max_offset.x()),
                        std::clamp(value.y(), min_offset.y(), max_offset.y()));
}

void OffsetTagConstraints::ExpandVisibleRect(
    gfx::RectF& visible_rect_in_target) const {
  DCHECK(IsValid());
  visible_rect_in_target.Outset(gfx::OutsetsF::TLBR(
      max_offset.y(), max_offset.x(), -min_offset.y(), -min_offset.x()));
}

bool OffsetTagConstraints::IsValid() const {
  return min_offset.x() <= 0 && min_offset.y() <= 0 && max_offset.x() >= 0 &&
         max_offset.y() >= 0 && min_offset.x() <= max_offset.x() &&
         min_offset.y() <= max_offset.y();
}

std::string OffsetTagConstraints::ToString() const {
  return base::StringPrintf("[%g-%g %g-%g]", min_offset.x(), max_offset.x(),
                            min_offset.y(), max_offset.y());
}

OffsetTagDefinition::OffsetTagDefinition() = default;

OffsetTagDefinition::OffsetTagDefinition(
    const OffsetTag& tag,
    const SurfaceRange& provider,
    const OffsetTagConstraints& constraints)
    : tag(tag), provider(provider), constraints(constraints) {}

OffsetTagDefinition::OffsetTagDefinition(const OffsetTagDefinition& other) =
    default;

OffsetTagDefinition& OffsetTagDefinition::operator=(
    const OffsetTagDefinition& other) = default;

OffsetTagDefinition::~OffsetTagDefinition() = default;

bool OffsetTagDefinition::IsValid() const {
  return tag && provider.IsValid() && constraints.IsValid();
}

}  // namespace viz
