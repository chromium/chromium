// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_METRICS_UTIL_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_METRICS_UTIL_H_

#include "chromeos/dbus/hermes/hermes_response_status.h"

namespace chromeos {
namespace cellular_setup {
namespace metrics {

void LogInstallViaQrCodeResult(HermesResponseStatus status);
void LogInstallPendingProfileResult(HermesResponseStatus status);

}  // namespace metrics
}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_METRICS_UTIL_H_
