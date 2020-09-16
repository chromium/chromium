// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_
#define CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_

#include <cstdint>
#include <string>
#include <utility>

#include "base/process/process_metrics.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace memory {
namespace pressure {

// Export for unittest.
CHROMEOS_EXPORT uint64_t CalculateReservedFreeKB(const std::string& zoneinfo);

// Export for unittest.
CHROMEOS_EXPORT uint64_t
CalculateAvailableMemoryUserSpaceKB(const base::SystemMemoryInfoKB& info,
                                    uint64_t reserved_free,
                                    uint64_t min_filelist,
                                    uint64_t ram_swap_weight);

// GetAvailableMemoryKB returns the available memory in KiB.
CHROMEOS_EXPORT uint64_t GetAvailableMemoryKB();

// GetMemoryMarginsKB returns a pair of uint64_t. The first value is the
// critical memory pressure level, the second value is the moderate memory
// pressure level.
CHROMEOS_EXPORT std::pair<uint64_t, uint64_t> GetMemoryMarginsKB();

// The memory parameters are saved for optimization.  If these memory
// parameters are changed, call this function to update the saved values.
void UpdateMemoryParameters();

}  // namespace pressure
}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_
