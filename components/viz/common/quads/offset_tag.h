// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_OFFSET_TAG_H_
#define COMPONENTS_VIZ_COMMON_QUADS_OFFSET_TAG_H_

#include <string>

#include "base/token.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
namespace mojom {
class OffsetTagDataView;
}

// OffsetTag is used to tag layer/quads that can be moved by the display
// compositor at draw time. The quads will be moved based on a corresponding
// OffsetTagValue provided by another viz client. The viz client that defines
// the OffsetTag will specify what surface provides the OffsetTagValue and
// OffsetTagConstraints to limit the position of quads.
class VIZ_COMMON_EXPORT OffsetTag {
 public:
  // Creates a non-empty tag.
  static OffsetTag CreateRandom();

  // Creates an empty tag. The empty tag is used to indicate no tag on
  // layers/quads.
  constexpr OffsetTag() = default;

  // For interop with Java created tokens.
  constexpr explicit OffsetTag(const base::Token& token) : token_(token) {}

  constexpr OffsetTag(const OffsetTag& other) = default;
  constexpr OffsetTag& operator=(const OffsetTag& other) = default;

  std::string ToString() const;
  constexpr bool IsEmpty() const { return token_.is_zero(); }

  constexpr explicit operator bool() const { return !IsEmpty(); }

  friend constexpr bool operator==(const OffsetTag& lhs,
                                   const OffsetTag& rhs) = default;
  friend constexpr auto operator<=>(const OffsetTag& lhs,
                                    const OffsetTag& rhs) = default;

 private:
  friend struct mojo::StructTraits<mojom::OffsetTagDataView, OffsetTag>;

  // base::Token / a 128 bits token allow randomly generated tokens to be used
  // without risk of collision but isn't intended to provide any security
  // guarantees. Collision free allocation avoids having to coordinate
  // allocation of tags. The embedder explicitly picks which viz client provides
  // the OffsetTagValue so it's not possible for a malicious viz client to
  // provide a different value.
  base::Token token_;
};

// Provides an offset value to translate tagged quads.
struct VIZ_COMMON_EXPORT OffsetTagValue {
  // Validates tag is non-empty.
  bool IsValid() const;
  std::string ToString() const;

  OffsetTag tag;
  gfx::Vector2dF offset;
};

// Provides constraints on where the OffsetTagValue can move quads. The
// constraints must include the default 0,0 offset to be valid.
struct VIZ_COMMON_EXPORT OffsetTagConstraints {
  OffsetTagConstraints();
  OffsetTagConstraints(float min_x, float max_x, float min_y, float max_y);

  // Clamps `offset` so it satisfies constraints.
  gfx::Vector2dF Clamp(gfx::Vector2dF offset) const;

  // This function takes a rect that contains visible parts of content with no
  // offset, in the target render pass coordinate space that an offset will be
  // applied in, and expands it based on max possible offsets in each direction.
  //
  // This expansion is "backwards" from what you might expect. For example if
  // constraints allow shifting the content down, that outsets the visible_rect
  // at the top, since content that was previously above the top of the visible
  // rect is moved lower and becomes visible.
  void ExpandVisibleRect(gfx::RectF& visible_rect_in_target) const;

  // Validates that constrains include 0,0 offset and that min is smaller max.
  bool IsValid() const;
  std::string ToString() const;

  gfx::Vector2dF min_offset;
  gfx::Vector2dF max_offset;
};

// Defines where to look for the value and constraints for an OffsetTag.
// `provider` will be resolved to a specific surface and the OffsetTagValue with
// matching `tag` will be used as the offset value. If no surface exists or the
// resolved surface has no matching OffsetTagValue then the default 0,0 offset
// will be used. If the offset value does not satisfy `constraints` then it will
// be clamped.
struct VIZ_COMMON_EXPORT OffsetTagDefinition {
  OffsetTagDefinition();
  OffsetTagDefinition(const OffsetTag& tag,
                      const SurfaceRange& provider,
                      const OffsetTagConstraints& constraints);

  OffsetTagDefinition(const OffsetTagDefinition& other);
  OffsetTagDefinition& operator=(const OffsetTagDefinition& other);
  ~OffsetTagDefinition();

  // Validates that tag is non-empty plus provider and constraints are valid.
  bool IsValid() const;

  OffsetTag tag;
  SurfaceRange provider;
  OffsetTagConstraints constraints;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_OFFSET_TAG_H_
