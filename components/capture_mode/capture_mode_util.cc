// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/capture_mode/capture_mode_util.h"

#include "base/check.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"

namespace capture_mode {

bool IsGpuRasterizationSupported(ui::ContextFactory* context_factory) {
  DCHECK(context_factory);
  auto provider = context_factory->SharedMainThreadRasterContextProvider();

  if (!provider) {
    return false;
  }

  const auto& gpu_feature_info = provider->GetGpuFeatureInfo();
  return features::IsUiGpuRasterizationEnabled() &&
         gpu_feature_info
                 .status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] ==
             gpu::kGpuFeatureStatusEnabled;
}

}  // namespace capture_mode
