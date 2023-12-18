// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_REQUEST_H_

#include <stdint.h>
#include "base/component_export.h"

namespace ash {

// Structure to hold FwupdRequest data received from fwupd.
struct COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdRequest {
  FwupdRequest();
  FwupdRequest(uint32_t id, uint32_t kind);
  FwupdRequest(const FwupdRequest& other);
  ~FwupdRequest();

  // The ID of the Request; corresponds to the enum values found in
  // https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-request.h
  uint32_t id;
  // The Kind of the Request; corresponds to the enum values found in
  // https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-request.h
  uint32_t kind;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_REQUEST_H_
