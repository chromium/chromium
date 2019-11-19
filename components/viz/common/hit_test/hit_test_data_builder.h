// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_BUILDER_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_BUILDER_H_

#include "base/macros.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// TODO(crbug.com/923398): Move this back to viz/client once
// DirectLayerTreeFrameSink no longer needs this.
class VIZ_COMMON_EXPORT HitTestDataBuilder {
 public:
  static base::Optional<HitTestRegionList> CreateHitTestData(
      const CompositorFrame& frame,
      bool root_accepts_events,
      bool should_ask_for_child_region);

 private:
  DISALLOW_COPY_AND_ASSIGN(HitTestDataBuilder);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_BUILDER_H_
