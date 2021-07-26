// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_uploader.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace enterprise_reporting {

class ReportingDelegateFactory;
class RealTimeUploader;

// Schedules report generation and upload every 24 hours (and upon browser
// update for desktop Chrome) while cloud reporting is enabled via
// administrative policy. If either of these triggers fires while a report is
// being generated, processing is deferred until the existing processing
// completes.
class ReportScheduler {
 public:
  // The trigger leading to report generation. Values are bitmasks in the
  // |pending_triggers_| bitfield.
  enum ReportTrigger : uint32_t {
    kTriggerNone = 0,                    // No trigger.
    kTriggerTimer = 1U << 0,             // The periodic timer expired.
    kTriggerUpdate = 1U << 1,            // An update was detected.
    kTriggerNewVersion = 1U << 2,        // A new version is running.
    kTriggerExtensionRequest = 1U << 3,  // Pending extension requests updated.
    // Pending extension requests updated, with encrypted realtime pipeline.
    kTriggerExtensionRequestRealTime = 1U << 4,
  };

  using ReportTriggerCallback = base::RepeatingCallback<void(ReportTrigger)>;

  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();

    void SetReportTriggerCallback(ReportTriggerCallback callback);

    virtual PrefService* GetLocalState() = 0;

    // Browser version
    virtual void StartWatchingUpdatesIfNeeded(
        base::Time last_upload,
        base::TimeDelta upload_interval) = 0;
    virtual void StopWatchingUpdates() = 0;
    virtual void OnBrowserVersionUploaded() = 0;

    // Extension request
    virtual void StartWatchingExtensionRequestIfNeeded() = 0;
    virtual void StopWatchingExtensionRequest() = 0;
    virtual void OnExtensionRequestUploaded() = 0;

   protected:
    ReportTriggerCallback trigger_report_callback_;
  };

  ReportScheduler(
      policy::CloudPolicyClient* client,
      std::unique_ptr<ReportGenerator> report_generator,
      std::unique_ptr<RealTimeReportGenerator> real_time_report_generator,
      ReportingDelegateFactory* delegate_factory);

  ReportScheduler(
      policy::CloudPolicyClient* client,
      std::unique_ptr<ReportGenerator> report_generator,
      std::unique_ptr<RealTimeReportGenerator> real_time_report_generator,
      std::unique_ptr<ReportScheduler::Delegate> delegate);

  ~ReportScheduler();

  // Returns true if cloud reporting is enabled.
  bool IsReportingEnabled() const;

  // Returns true if next report has been scheduled. The report will be
  // scheduled only if the previous report is uploaded successfully and the
  // reporting policy is still enabled.
  bool IsNextReportScheduledForTesting() const;

  void SetReportUploaderForTesting(std::unique_ptr<ReportUploader> uploader);
  void SetExtensionRequestUploaderForTesting(
      std::unique_ptr<RealTimeUploader> uploader);

  void OnDMTokenUpdated();

 private:
  // Observes CloudReportingEnabled policy.
  void RegisterPrefObserver();

  // Handles kCloudReportingEnabled policy value change, including the first
  // policy value check during startup.
  void OnReportEnabledPrefChanged();

  // Stops the periodic timer and the update observer.
  void Stop();

  // Register |cloud_policy_client_| with dm token and client id for desktop
  // browser only. (Chrome OS doesn't need this step here.)
  bool SetupBrowserPolicyClientRegistration();

  // Starts the periodic timer based on the last time a report was uploaded.
  void Start(base::Time last_upload_time);

  // Starts report generation in response to |trigger|.
  void GenerateAndUploadReport(ReportTrigger trigger);

  // Continues processing a report (contained in the |requests| collection) by
  // sending it to the uploader.
  void OnReportGenerated(ReportGenerator::ReportRequests requests);

  // Finishes processing following report upload. |status| indicates the result
  // of the attempted upload.
  void OnReportUploaded(ReportUploader::ReportStatus status);

  // Initiates report generation for any triggers that arrived during generation
  // of another report.
  void RunPendingTriggers();

  // Creates and uploads extension requests with real time reporting pipeline.
  void UploadExtensionRequests();

  // Records that |trigger| was responsible for an upload attempt.
  static void RecordUploadTrigger(ReportTrigger trigger);

  std::unique_ptr<Delegate> delegate_;

  // Policy value watcher
  PrefChangeRegistrar pref_change_registrar_;

  policy::CloudPolicyClient* cloud_policy_client_;

  base::WallClockTimer request_timer_;

  std::unique_ptr<ReportUploader> report_uploader_;

  std::unique_ptr<ReportGenerator> report_generator_;

  std::unique_ptr<RealTimeReportGenerator> real_time_report_generator_;

  std::unique_ptr<RealTimeUploader> extension_request_uploader_;

  // The trigger responsible for initiating active report generation.
  ReportTrigger active_trigger_ = kTriggerNone;

  // The set of triggers that have fired while processing a report (a bitfield
  // of ReportTrigger values). They will be handled following completion of the
  // in-process report.
  uint32_t pending_triggers_ = 0;

  base::WeakPtrFactory<ReportScheduler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportScheduler);
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_
