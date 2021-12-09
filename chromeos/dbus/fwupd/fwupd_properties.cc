// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_properties.h"

#include "chromeos/dbus/fwupd/dbus_constants.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {

FwupdProperties::FwupdProperties(dbus::ObjectProxy* object_proxy,
                                 const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, kFwupdServiceInterface, callback) {
  RegisterProperty("Percentage", &percentage);
  RegisterProperty("Status", &status);
}

FwupdProperties::~FwupdProperties() = default;

}  // namespace chromeos
