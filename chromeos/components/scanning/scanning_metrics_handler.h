// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SCANNING_SCANNING_METRICS_HANDLER_H_
#define CHROMEOS_COMPONENTS_SCANNING_SCANNING_METRICS_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {

// ChromeOS Scan app metrics handler for recording metrics from the UI.
class ScanningMetricsHandler : public content::WebUIMessageHandler {
 public:
  ScanningMetricsHandler();
  ~ScanningMetricsHandler() override;

  ScanningMetricsHandler(const ScanningMetricsHandler&) = delete;
  ScanningMetricsHandler& operator=(const ScanningMetricsHandler&) = delete;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Records the settings for a scan job.
  void HandleRecordScanJobSettings(const base::ListValue* args);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SCANNING_SCANNING_METRICS_HANDLER_H_
