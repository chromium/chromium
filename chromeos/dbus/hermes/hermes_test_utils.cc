// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/hermes_test_utils.h"

#include "chromeos/dbus/hermes/hermes_response_status.h"

namespace chromeos {
namespace hermes_test_utils {

void CopyHermesStatus(HermesResponseStatus* dest_status,
                      HermesResponseStatus status) {
  *dest_status = status;
}

}  // namespace hermes_test_utils
}  // namespace chromeos
