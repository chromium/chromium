// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HERMES_METRICS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HERMES_METRICS_UTIL_H_

#include "base/component_export.h"
#include "base/time/tick_clock.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "dbus/dbus_result.h"

namespace ash::hermes_metrics {

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogInstallViaQrCodeResult(HermesResponseStatus status,
                              dbus::DBusResult dbus_result,
                              bool is_initial_install);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogInstallPendingProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogEnableProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogDisableProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogUninstallProfileResult(HermesResponseStatus status);

void COMPONENT_EXPORT(CHROMEOS_NETWORK)
    LogRefreshInstalledProfilesLatency(base::TimeDelta call_latency);

}  // namespace ash::hermes_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HERMES_METRICS_UTIL_H_
