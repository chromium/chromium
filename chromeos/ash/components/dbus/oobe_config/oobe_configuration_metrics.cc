// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_metrics.h"

#include <dbus/dbus-protocol.h>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordDeleteFlexOobeConfigDBusResult(
    DeleteFlexOobeConfigDBusResult result) {
  base::UmaHistogramEnumeration(kDeleteFlexOobeConfigDBusResultMetricName,
                                result);
}

DeleteFlexOobeConfigDBusResult ConvertDeleteFlexOobeConfigDBusError(
    std::string dbus_error_code) {
  if (dbus_error_code == DBUS_ERROR_ACCESS_DENIED) {
    return DeleteFlexOobeConfigDBusResult::kErrorAccessDenied;
  } else if (dbus_error_code == DBUS_ERROR_NOT_SUPPORTED) {
    return DeleteFlexOobeConfigDBusResult::kErrorNotSupported;
  } else if (dbus_error_code == DBUS_ERROR_FILE_NOT_FOUND) {
    return DeleteFlexOobeConfigDBusResult::kErrorConfigNotFound;
  } else if (dbus_error_code == DBUS_ERROR_IO_ERROR) {
    return DeleteFlexOobeConfigDBusResult::kErrorIOError;
  } else {
    LOG(ERROR) << "Unknown DBus error code: " << dbus_error_code;
    return DeleteFlexOobeConfigDBusResult::kErrorUnknown;
  }
}

}  // namespace ash
