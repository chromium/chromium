// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_properties_fake.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"

namespace ash {

FwupdPropertiesFake::FwupdPropertiesFake(uint32_t percentage, uint32_t status)
    : percentage(percentage), status(status) {}

FwupdPropertiesFake::~FwupdPropertiesFake() = default;

void FwupdPropertiesFake::SetPercentage(uint32_t new_percentage) {
  percentage = new_percentage;
}

void FwupdPropertiesFake::SetStatus(uint32_t new_status) {
  status = new_status;
}

uint32_t FwupdPropertiesFake::GetStatus() {
  return status;
}

uint32_t FwupdPropertiesFake::GetPercentage() {
  return percentage;
}

bool FwupdPropertiesFake::IsPercentageValid() {
  return true;
}

bool FwupdPropertiesFake::IsStatusValid() {
  return true;
}

}  // namespace ash
