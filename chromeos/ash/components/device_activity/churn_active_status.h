// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_ACTIVE_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_ACTIVE_STATUS_H_

#include <bitset>

#include "base/component_export.h"

namespace ash::device_activity {

// The Churn use case maintains an instance of this class to represent
// which of the past 18 months the device was active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    ChurnActiveStatus {
 public:
  // First 10 bits represent number months from 01/01/2000 to the current month.
  // Remaining 18 bits represents past 18 months when device was active from
  // current month.
  static constexpr int kChurnBitSize = 28;

  ChurnActiveStatus();
  explicit ChurnActiveStatus(int value);
  ChurnActiveStatus(const ChurnActiveStatus&) = delete;
  ChurnActiveStatus& operator=(const ChurnActiveStatus&) = delete;
  ~ChurnActiveStatus();

  int GetValueAsInt() const;

 private:
  std::bitset<kChurnBitSize> value_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_ACTIVE_STATUS_H_
