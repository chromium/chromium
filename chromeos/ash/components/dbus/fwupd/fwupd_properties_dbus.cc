// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_properties_dbus.h"

#include <cstdint>

#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace ash {

FwupdDbusProperties::FwupdDbusProperties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, kFwupdServiceInterface, callback) {
  RegisterProperty("Percentage", &percentage);
  RegisterProperty("Status", &status);
}

FwupdDbusProperties::~FwupdDbusProperties() = default;

void FwupdDbusProperties::SetPercentage(uint32_t new_percentage) {
  // Manually call set_valid because ReplaceValue doesn't.
  percentage.set_valid(true);
  percentage.ReplaceValue(new_percentage);
}

void FwupdDbusProperties::SetStatus(uint32_t new_status) {
  // Manually call set_valid because ReplaceValue doesn't.
  status.set_valid(true);
  status.ReplaceValue(new_status);
}

uint32_t FwupdDbusProperties::GetStatus() {
  CHECK(status.is_valid());
  return status.value();
}

uint32_t FwupdDbusProperties::GetPercentage() {
  CHECK(percentage.is_valid());
  return percentage.value();
}

bool FwupdDbusProperties::IsPercentageValid() {
  return percentage.is_valid();
}

bool FwupdDbusProperties::IsStatusValid() {
  return status.is_valid();
}

}  // namespace ash
