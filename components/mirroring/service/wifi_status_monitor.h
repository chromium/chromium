// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_WIFI_STATUS_MONITOR_H_
#define COMPONENTS_MIRRORING_SERVICE_WIFI_STATUS_MONITOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace mirroring {

class MessageDispatcher;
class ReceiverResponse;

struct WifiStatus {
  double snr;
  int32_t speed;         // The current WiFi speed.
  base::Time timestamp;  // Recording time of this status.
};

// Periodically sends requests to the Cast device for WiFi network status
// updates, processes responses, and maintains a recent history of data points.
// This data can be included in feedback logs to help identify and diagnose
// issues related to lousy network performance.
class COMPONENT_EXPORT(MIRRORING_SERVICE) WifiStatusMonitor {
 public:
  // |message_dispatcher| must keep alive during the lifetime of this class.
  explicit WifiStatusMonitor(MessageDispatcher* message_dispatcher);
  ~WifiStatusMonitor();

  // Gets the recorded status and clear |recent_status_|.
  std::vector<WifiStatus> GetRecentValues();

  // Sends GET_STATUS message to receiver.
  void QueryStatus();

  // Callback for the STATUS_RESPONSE message. Records the WiFi status reported
  // by receiver.
  void RecordStatus(const ReceiverResponse& response);

 private:
  MessageDispatcher* const message_dispatcher_;  // Outlives this class.

  base::RepeatingTimer query_timer_;

  // Stores the recent status. Will be reset when GetRecentValues() is called.
  base::circular_deque<WifiStatus> recent_status_;

  DISALLOW_COPY_AND_ASSIGN(WifiStatusMonitor);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_WIFI_STATUS_MONITOR_H_
