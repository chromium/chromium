// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_COMPARE_RESULT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_COMPARE_RESULT_H_

namespace viz {

enum BspCompareResult {
  BSP_FRONT,
  BSP_BACK,
  BSP_SPLIT,
  BSP_COPLANAR_FRONT,
  BSP_COPLANAR_BACK,
  BSP_COPLANAR,
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_COMPARE_RESULT_H_
