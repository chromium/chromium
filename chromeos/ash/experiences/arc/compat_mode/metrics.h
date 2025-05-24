// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_COMPAT_MODE_METRICS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_COMPAT_MODE_METRICS_H_

#include "chromeos/ash/experiences/arc/mojom/compatibility_mode.mojom.h"

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

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_COMPAT_MODE_METRICS_H_
