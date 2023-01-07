// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_test_utils.h"

#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"

namespace ash::hermes_test_utils {

void CopyHermesStatus(HermesResponseStatus* dest_status,
                      HermesResponseStatus status) {
  *dest_status = status;
}

}  // namespace ash::hermes_test_utils
