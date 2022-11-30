// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_

#include <windows.h>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Checks if an HWND needs to support transparency and should use a layered
// window.
VIZ_COMMON_EXPORT bool NeedsToUseLayerWindow(HWND hwnd);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_
