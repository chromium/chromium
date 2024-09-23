// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SWITCHES_H_
#define COMPONENTS_VIZ_COMMON_SWITCHES_H_

#include <stdint.h>

#include <optional>

#include "base/feature_list.h"
#include "components/viz/common/viz_common_export.h"

namespace switches {

// Keep list in alphabetical order.
VIZ_COMMON_EXPORT extern const char kDeadlineToSynchronizeSurfaces[];
VIZ_COMMON_EXPORT extern const char kDelegatedInkRenderer[];
VIZ_COMMON_EXPORT extern const char kDisableAdpf[];
VIZ_COMMON_EXPORT extern const char kDisableFrameRateLimit[];
VIZ_COMMON_EXPORT extern const char kDoubleBufferCompositing[];
VIZ_COMMON_EXPORT extern const char kEnableHardwareOverlays[];
VIZ_COMMON_EXPORT extern const char kRunAllCompositorStagesBeforeDraw[];
VIZ_COMMON_EXPORT extern const char kShowAggregatedDamage[];
VIZ_COMMON_EXPORT extern const char kTintCompositedContentModulate[];

// kShowDCLayerDebugBorders shows the debug borders of the overlays and the
// damage rect after using overlays on Windows. Do not use
// kShowDCLayerDebugBorders and kShowAggregatedDamage together because
// kShowAggregatedDamage sets the entire frame as damaged and this causes
// incorrect damage rect borders after using overlays.
VIZ_COMMON_EXPORT extern const char kShowDCLayerDebugBorders[];

VIZ_COMMON_EXPORT std::optional<uint32_t> GetDeadlineToSynchronizeSurfaces();

enum class DelegatedInkRendererMode { kNone, kSystem, kSkia };
VIZ_COMMON_EXPORT std::optional<DelegatedInkRendererMode>
GetDelegatedInkRendererMode();

}  // namespace switches

#endif  // COMPONENTS_VIZ_COMMON_SWITCHES_H_
