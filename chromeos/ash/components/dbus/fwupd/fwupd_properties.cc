// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"

#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace ash {

FwupdProperties::FwupdProperties(dbus::ObjectProxy* object_proxy,
                                 const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, kFwupdServiceInterface, callback) {
  RegisterProperty("Percentage", &percentage);
  RegisterProperty("Status", &status);
}

FwupdProperties::~FwupdProperties() = default;

}  // namespace ash
