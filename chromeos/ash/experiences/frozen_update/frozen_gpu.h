// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_GPU_H_
#define CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_GPU_H_

namespace ash {

// Identifying information for a GPU which will require updates to be frozen.
struct FrozenGpu {
  // Vendor PCIID
  unsigned int vendor;
  // Device PCIID
  unsigned int device;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_GPU_H_
