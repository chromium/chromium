// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_METRICS_H_

#include "dbus/message.h"

namespace ash {

// This enum is tied directly to a UMA enum defined in
// tools/metrics/histograms/enums.xml, existing entries should not be modified.
enum class GetFeedbackLogsV2DbusResult {
  kSuccess = 0,
  // The following three are all different types of timeouts reported by the
  // DBus service:
  // "No reply to a message expecting one, usually means a timeout occurred."
  kErrorNoReply = 1,
  // "Certain timeout errors, possibly ETIMEDOUT on a socket."
  kErrorTimeout = 2,
  // The bus doesn't know how to launch a service to supply the bus name you
  // wanted.
  kErrorServiceUnknown = 3,
  kErrorNotSupported = 4,
  kErrorDisconnected = 5,
  kErrorResponseMissing = 6,
  kErrorGeneric = 7,
  kErrorReadingData = 8,
  kErrorDeserializingJSonLogs = 9,

  kMaxValue = kErrorDeserializingJSonLogs
};

void RecordGetFeedbackLogsV2DbusResult(GetFeedbackLogsV2DbusResult result);

// Map the error name to GetFeedbackLogsV2DbusResult and emit UMA.
void RecordGetFeedbackLogsV2DbusError(dbus::ErrorResponse* err_response);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_METRICS_H_
