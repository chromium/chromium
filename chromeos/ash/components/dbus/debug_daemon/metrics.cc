// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/debug_daemon/metrics.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "dbus/dbus-protocol.h"

namespace ash {

constexpr char kGetFeedbackLogsV2DbusResult[] =
    "Feedback.ChromeOSApp.GetFeedbackLogsV2.DBusResult";

void RecordGetFeedbackLogsV2DbusResult(GetFeedbackLogsV2DbusResult result) {
  base::UmaHistogramEnumeration(kGetFeedbackLogsV2DbusResult, result);
}

// Error names were defined in
// gen/amd64-generic/chroot/build/amd64-generic/usr/include/dbus-1.0/dbus/
// dbus-protocol.h.
void RecordGetFeedbackLogsV2DbusError(dbus::ErrorResponse* err_response) {
  if (!err_response) {
    RecordGetFeedbackLogsV2DbusResult(GetFeedbackLogsV2DbusResult::kSuccess);
    return;
  }
  const std::string& error_name = err_response->GetErrorName();
  LOG(ERROR) << "GetFeedbackLogsV2 Dbus error name: " << error_name;

  if (error_name == DBUS_ERROR_NO_REPLY) {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorNoReply);
  } else if (error_name == DBUS_ERROR_TIMEOUT) {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorTimeout);
  } else if (error_name == DBUS_ERROR_SERVICE_UNKNOWN) {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorServiceUnknown);
  } else if (error_name == DBUS_ERROR_NOT_SUPPORTED) {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorNotSupported);
  } else if (error_name == DBUS_ERROR_DISCONNECTED) {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorDisconnected);
  } else {
    RecordGetFeedbackLogsV2DbusResult(
        GetFeedbackLogsV2DbusResult::kErrorGeneric);
  }
}

}  // namespace ash
