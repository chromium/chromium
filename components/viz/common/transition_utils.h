// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_TRANSITION_UTILS_H_
#define COMPONENTS_VIZ_COMMON_TRANSITION_UTILS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This class is a collection of utils used by view transition API.
class VIZ_COMMON_EXPORT TransitionUtils {
 public:
  // Creates a deep copy of |source_pass| retaining all state. |filter_callback|
  // is invoked for each render pass draw quad to let the caller modify the copy
  // of these quads. If the callback returns true the quad is skipped otherwise
  // it is copied as-is.
  using FilterCallback =
      base::RepeatingCallback<bool(const DrawQuad&, CompositorRenderPass&)>;
  static std::unique_ptr<CompositorRenderPass> CopyPassWithQuadFiltering(
      const CompositorRenderPass& source_pass,
      FilterCallback filter_callback);

  static CompositorRenderPassId NextRenderPassId(
      const CompositorRenderPassId& id) {
    return CompositorRenderPassId(id.GetUnsafeValue() + 1);
  }

  static std::string RenderPassListToString(
      const CompositorRenderPassList& render_passes);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_TRANSITION_UTILS_H_
