// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_uploader.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class CloudPolicyClient;
class DMToken;
}  // namespace policy

namespace enterprise_reporting {

class RealTimeReportController;

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
    kTriggerNone = 0,              // No trigger.
    kTriggerTimer = 1U << 0,       // The periodic timer expired.
    kTriggerUpdate = 1U << 1,      // An update was detected.
    kTriggerNewVersion = 1U << 2,  // A new version is running.
    kTriggerManual = 1U << 3,      // Trigger manually.
  };

  using ReportTriggerCallback = base::RepeatingCallback<void(ReportTrigger)>;

  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();

    void SetReportTriggerCallback(ReportTriggerCallback callback);

    virtual PrefService* GetPrefService() = 0;

    // Browser version
    virtual void StartWatchingUpdatesIfNeeded(
        base::Time last_upload,
        base::TimeDelta upload_interval) = 0;
    virtual void StopWatchingUpdates() = 0;
    virtual void OnBrowserVersionUploaded() = 0;

    virtual policy::DMToken GetProfileDMToken() = 0;
    virtual std::string GetProfileClientId() = 0;

   protected:
    ReportTriggerCallback trigger_report_callback_;
  };

  struct CreateParams {
    CreateParams();
    CreateParams(const CreateParams&) = delete;
    CreateParams& operator=(const CreateParams&) = delete;
    CreateParams(CreateParams&& other);
    CreateParams& operator=(CreateParams&& other);
    ~CreateParams();

    raw_ptr<policy::CloudPolicyClient> client;
    std::unique_ptr<ReportGenerator> report_generator;
    std::unique_ptr<RealTimeReportController> real_time_report_controller;
    std::unique_ptr<ChromeProfileRequestGenerator> profile_request_generator;
    std::unique_ptr<ReportScheduler::Delegate> delegate;
  };

  explicit ReportScheduler(CreateParams params);

  ReportScheduler(const ReportScheduler&) = delete;
  ReportScheduler& operator=(const ReportScheduler&) = delete;

  ~ReportScheduler();

  // Returns true if cloud reporting is enabled.
  bool IsReportingEnabled() const;

  // Returns true if next report has been scheduled. The report will be
  // scheduled only if the previous report is uploaded successfully and the
  // reporting policy is still enabled.
  bool IsNextReportScheduledForTesting() const;

  ReportTrigger GetActiveTriggerForTesting() const;

  void SetReportUploaderForTesting(std::unique_ptr<ReportUploader> uploader);
  Delegate* GetDelegateForTesting();

  void OnDMTokenUpdated();

  void UploadFullReport(base::OnceClosure on_report_uploaded);

 private:
  // Observes CloudReportingEnabled policy.
  void RegisterPrefObserver();

  // Handles kCloudReportingEnabled policy value change, including the first
  // policy value check during startup.
  void OnReportEnabledPrefChanged();

  // Stops the periodic timer and the update observer.
  void Stop();

  // Stop the timer if there is any and reschedule the next report based on
  // latest report frequency.
  void RestartReportTimer();

  // Register |cloud_policy_client_| with dm token and client id for desktop
  // browser only. (Chrome OS doesn't need this step here.)
  bool SetupBrowserPolicyClientRegistration();

  // Starts the periodic timer based on the last time a report was uploaded.
  void Start(base::Time last_upload_time);

  // Starts report generation in response to |trigger|.
  void GenerateAndUploadReport(ReportTrigger trigger);

  // Continues processing a report (contained in the |requests| collection) by
  // sending it to the uploader.
  void OnReportGenerated(ReportRequestQueue requests);

  // Finishes processing following report upload. |status| indicates the result
  // of the attempted upload.
  void OnReportUploaded(ReportUploader::ReportStatus status);

  // Initiates report generation for any triggers that arrived during generation
  // of another report.
  void RunPendingTriggers();

  // Records that |trigger| was responsible for an upload attempt.
  static void RecordUploadTrigger(ReportTrigger trigger);

  ReportType TriggerToReportType(ReportTrigger trigger);

  policy::DMToken GetDMToken();

  std::unique_ptr<Delegate> delegate_;

  // Policy value watcher
  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> cloud_policy_client_;

  base::WallClockTimer request_timer_;

  std::unique_ptr<ReportUploader> report_uploader_;

  std::unique_ptr<ReportGenerator> report_generator_;
  std::unique_ptr<ChromeProfileRequestGenerator> profile_request_generator_;
  std::unique_ptr<RealTimeReportController> real_time_report_controller_;

  // The trigger responsible for initiating active report generation.
  ReportTrigger active_trigger_ = kTriggerNone;

  // The set of triggers that have fired while processing a report (a bitfield
  // of ReportTrigger values). They will be handled following completion of the
  // in-process report.
  uint32_t pending_triggers_ = 0;

  std::string reporting_pref_name_;
  ReportType full_report_type_;

  base::OnceClosure on_manual_report_uploaded_;

  base::WeakPtrFactory<ReportScheduler> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_SCHEDULER_H_
