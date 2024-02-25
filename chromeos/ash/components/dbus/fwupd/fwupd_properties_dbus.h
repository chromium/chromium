// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_DBUS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_DBUS_H_

#include <cstdint>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace ash {

// Wrapper class of dbus::PropertySet that holds Dbus property fields
// pertaining to an FWUPD Dbus object. Properties of this class gets updated
// whenever dbus::PropertiesChanged() is called for the Fwupd interface.
class COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdDbusProperties
    : public dbus::PropertySet,
      public FwupdProperties {
 public:
  FwupdDbusProperties(dbus::ObjectProxy* object_proxy,
                      const PropertyChangedCallback& callback);
  ~FwupdDbusProperties() override;

  void SetPercentage(uint32_t new_percentage) override;
  void SetStatus(uint32_t new_status) override;

  uint32_t GetPercentage() override;
  uint32_t GetStatus() override;

  bool IsPercentageValid() override;
  bool IsStatusValid() override;

 private:
  dbus::Property<uint32_t> percentage;
  dbus::Property<uint32_t> status;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_PROPERTIES_DBUS_H_
