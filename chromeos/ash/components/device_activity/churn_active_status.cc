// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_active_status.h"

namespace ash::device_activity {

ChurnActiveStatus::ChurnActiveStatus() = default;

ChurnActiveStatus::ChurnActiveStatus(int value) : value_(value) {}

ChurnActiveStatus::~ChurnActiveStatus() = default;

int ChurnActiveStatus::GetValueAsInt() const {
  return static_cast<int>(value_.to_ulong());
}

}  // namespace ash::device_activity
