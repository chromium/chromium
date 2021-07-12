// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_METRICS_H_
#define COMPONENTS_ARC_COMPAT_MODE_METRICS_H_

#include "components/arc/mojom/compatibility_mode.mojom.h"

namespace arc {

enum class ResizeLockActionType {
  ResizeToPhone,
  ResizeToTablet,
  TurnOnResizeLock,
  TurnOffResizeLock,
};

enum class ResizeLockStateHistogramType {
  InitialState,
};

void RecordResizeLockAction(ResizeLockActionType type);

void RecordResizeLockStateHistogram(ResizeLockStateHistogramType type,
                                    mojom::ArcResizeLockState state);

const char* GetResizeLockActionNameForTesting(ResizeLockActionType type);
const char* GetResizeLockStateHistogramNameForTesting(
    ResizeLockStateHistogramType type);

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_METRICS_H_
