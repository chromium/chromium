// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_UTILS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_UTILS_H_

#include <vector>

#include "components/viz/service/viz_service_export.h"
#include "ui/latency/latency_info.h"

namespace viz {

VIZ_SERVICE_EXPORT bool IsScroll(
    const std::vector<ui::LatencyInfo>& latency_infos);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_UTILS_H_
