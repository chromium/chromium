// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/android/signals_reporting_scheduler_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/enterprise/browser/reporting/android/jni_headers/SignalsReportingSchedulerBridge_jni.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;

namespace enterprise_reporting {

// static
SignalsReportingSchedulerBridge&
SignalsReportingSchedulerBridge::GetInstance() {
  static base::NoDestructor<SignalsReportingSchedulerBridge> instance;
  return *instance.get();
}

static void JNI_SignalsReportingSchedulerBridge_StartReporting(JNIEnv* env) {
  SignalsReportingSchedulerBridge::GetInstance().StartReporting();
}

//
// SignalsReportingSchedulerBridge
//
SignalsReportingSchedulerBridge::SignalsReportingSchedulerBridge() = default;
SignalsReportingSchedulerBridge::~SignalsReportingSchedulerBridge() = default;

void SignalsReportingSchedulerBridge::ScheduleReport(
    base::TimeDelta scheduled_time_delta,
    base::OnceClosure report_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  report_callback_ = std::move(report_callback);
  Java_SignalsReportingSchedulerBridge_scheduleReportBackgroundTask(
      AttachCurrentThread(),
      static_cast<jlong>(scheduled_time_delta.InMilliseconds()));
}

void SignalsReportingSchedulerBridge::CancelScheduledReport() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Java_SignalsReportingSchedulerBridge_cancelReportBackgroundTask(
      AttachCurrentThread());
}

void SignalsReportingSchedulerBridge::StartReporting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (report_callback_) {
    std::move(report_callback_).Run();
  }
}

}  // namespace enterprise_reporting

DEFINE_JNI(SignalsReportingSchedulerBridge)
