// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_HERMES_METRICS_UTIL_H_
#define CHROMEOS_NETWORK_HERMES_METRICS_UTIL_H_

#include "base/component_export.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"

namespace chromeos {
namespace hermes_metrics {

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogInstallViaQrCodeResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogInstallPendingProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogEnableProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogDisableProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogUninstallProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogRequestPendingProfilesResult(HermesResponseStatus status);

}  // namespace hermes_metrics
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::hermes_metrics {
using ::chromeos::hermes_metrics::LogInstallPendingProfileResult;
using ::chromeos::hermes_metrics::LogRequestPendingProfilesResult;
}  // namespace ash::hermes_metrics

#endif  // CHROMEOS_NETWORK_HERMES_METRICS_UTIL_H_
