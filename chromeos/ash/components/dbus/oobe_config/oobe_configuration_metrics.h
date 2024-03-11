// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_METRICS_H_

#include <string>

namespace ash {

inline constexpr char kDeleteFlexOobeConfigDBusResultMetricName[] =
    "OOBE.OobeConfig.DeleteFlexOobeConfig.DBusResult";

// Enumerates all possible outcomes of the DeleteFlexConfig DBus method call.
// This enum is tied directly to an UMA enum defined in
// tools/metrics/histograms/metadata/oobe/enums.xml.
enum class DeleteFlexOobeConfigDBusResult {
  kSuccess = 0,
  kErrorUnknown = 1,
  kErrorAccessDenied = 2,
  kErrorNotSupported = 3,
  kErrorConfigNotFound = 4,
  kErrorIOError = 5,

  kMaxValue = kErrorIOError
};

void RecordDeleteFlexOobeConfigDBusResult(
    DeleteFlexOobeConfigDBusResult result);

DeleteFlexOobeConfigDBusResult ConvertDeleteFlexOobeConfigDBusError(
    std::string dbus_error_code);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_METRICS_H_
