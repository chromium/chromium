// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_
#define CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_

#include <cstdint>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace chromeos {
namespace memory {
namespace pressure {

// Export for unittest.
CHROMEOS_EXPORT uint64_t CalculateReservedFreeKB(const std::string& zoneinfo);

// Export for unittest.
CHROMEOS_EXPORT double ParsePSIMemory(const std::string& contents);

// Export for unittest.
CHROMEOS_EXPORT uint64_t
CalculateAvailableMemoryUserSpaceKB(const base::SystemMemoryInfoKB& info,
                                    uint64_t reserved_free,
                                    uint64_t min_filelist,
                                    uint64_t ram_swap_weight);

// Export for unittest
CHROMEOS_EXPORT std::vector<uint64_t> GetMarginFileParts(
    const std::string& margin_file);

// GetAvailableMemoryKB returns the available memory in KiB.
CHROMEOS_EXPORT uint64_t GetAvailableMemoryKB();

// GetMemoryMarginsKB returns a pair of uint64_t. The first value is the
// critical memory pressure level, the second value is the moderate memory
// pressure level.
CHROMEOS_EXPORT std::pair<uint64_t, uint64_t> GetMemoryMarginsKB();

// The memory parameters are saved for optimization.  If these memory
// parameters are changed, call this function to update the saved values.
void UpdateMemoryParameters();

class PressureObserver : public base::CheckedObserver {
 public:
  // Called when the system is under critical memory pressure.
  virtual void OnCriticalPressure() = 0;

  // Called when the system is under moderate memory pressure.
  virtual void OnModeratePressure() = 0;
};

// Check memory pressure periodically and notify the observers when memory
// pressure is high.
class COMPONENT_EXPORT(CHROMEOS_MEMORY) PressureChecker {
 public:
  static PressureChecker* GetInstance();

  // Set the delay between checks. Setting zero delay stops the checking timer.
  void SetCheckingDelay(base::TimeDelta delay);

  void AddObserver(PressureObserver* observer);

  void RemoveObserver(PressureObserver* observer);

 private:
  friend struct base::DefaultSingletonTraits<PressureChecker>;

  PressureChecker();
  ~PressureChecker();

  // Check memory pressure and notify the observers when memory pressure is
  // high.
  void CheckPressure();

  base::ObserverList<PressureObserver> pressure_observers_;

  // A timer to check the memory pressure periodically.
  base::RepeatingTimer checking_timer_;

  base::WeakPtrFactory<PressureChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PressureChecker);
};

}  // namespace pressure
}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_PRESSURE_PRESSURE_H_
