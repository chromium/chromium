// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_DRAW_QUAD_H_
#define COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_DRAW_QUAD_H_

#include "components/viz/client/hit_test_data_provider.h"

namespace viz {

// HitTestDataProviderDrawQuad is used to extract hit test data from DrawQuads
// in the CompositorFrame.
class VIZ_CLIENT_EXPORT HitTestDataProviderDrawQuad
    : public HitTestDataProvider {
 public:
  HitTestDataProviderDrawQuad(bool should_ask_for_child_region,
                              bool root_accepts_events);
  ~HitTestDataProviderDrawQuad() override;

  base::Optional<HitTestRegionList> GetHitTestData(
      const CompositorFrame& compositor_frame) const override;

 private:
  const bool should_ask_for_child_region_;

  // Only used by HitTestRegionList to indicate if it should accept events or
  // not.
  const bool root_accepts_events_;

  DISALLOW_COPY_AND_ASSIGN(HitTestDataProviderDrawQuad);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_DRAW_QUAD_H_
