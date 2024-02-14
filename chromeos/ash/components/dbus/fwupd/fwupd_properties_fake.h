// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_FAKE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_FAKE_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"

// Implementation of FwupdProperties that can be used for tests, since the
// values can be set manually and it doesn't rely on a DBus ObjectProxy.
namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdPropertiesFake
    : public FwupdProperties {
 public:
  FwupdPropertiesFake(uint32_t percentage, uint32_t status);
  ~FwupdPropertiesFake() override;

  void SetPercentage(uint32_t new_percentage) override;
  void SetStatus(uint32_t new_status) override;

  uint32_t GetPercentage() override;
  uint32_t GetStatus() override;

  bool IsPercentageValid() override;
  bool IsStatusValid() override;

 private:
  uint32_t percentage;
  uint32_t status;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_FAKE_H_
