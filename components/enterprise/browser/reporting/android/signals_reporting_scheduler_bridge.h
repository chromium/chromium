// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_ANDROID_SIGNALS_REPORTING_SCHEDULER_BRIDGE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_ANDROID_SIGNALS_REPORTING_SCHEDULER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace enterprise_reporting {

class SignalsReportingSchedulerBridge {
 public:
  SignalsReportingSchedulerBridge();

  ~SignalsReportingSchedulerBridge();

  SignalsReportingSchedulerBridge(const SignalsReportingSchedulerBridge&) =
      delete;
  SignalsReportingSchedulerBridge& operator=(
      const SignalsReportingSchedulerBridge&) = delete;

  // Returns a reference to the singleton.
  static SignalsReportingSchedulerBridge& GetInstance();

  // Schedule a signals report `scheduled_time_delta` from now.
  void ScheduleReport(base::TimeDelta scheduled_time_delta,
                      base::OnceClosure report_callback);

  void CancelScheduledReport();

  // Kick off signals reporting process.
  void StartReporting();

 private:
  // We should be fine storing only one callback since Android does not support
  // multi-profile.
  base::OnceClosure report_callback_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_ANDROID_SIGNALS_REPORTING_SCHEDULER_BRIDGE_H_
