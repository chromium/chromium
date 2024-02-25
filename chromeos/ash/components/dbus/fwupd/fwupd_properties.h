// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_H_

#include <cstdint>
#include "base/component_export.h"

// Abstract class that mirrors the properties of a FWUPD DBus object.
namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdProperties {
 public:
  virtual ~FwupdProperties() = default;

  virtual void SetPercentage(uint32_t new_percentage) = 0;
  virtual void SetStatus(uint32_t new_status) = 0;

  virtual uint32_t GetPercentage() = 0;
  virtual uint32_t GetStatus() = 0;

  virtual bool IsPercentageValid() = 0;
  virtual bool IsStatusValid() = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_H_
