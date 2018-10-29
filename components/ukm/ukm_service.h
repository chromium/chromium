// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_SERVICE_H_
#define COMPONENTS_UKM_UKM_SERVICE_H_

#include <stddef.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/ukm/ukm_reporting_service.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
class MetricsServiceClient;
class UkmBrowserTestBase;
class UkmEGTestHelper;
}

namespace ukm {

namespace debug {
class UkmDebugDataExtractor;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This maps to the enum UkmResetReason.
enum class ResetReason {
  kOnSyncPrefsChanged = 0,
  kUpdatePermissions = 1,
  kMaxValue = kUpdatePermissions,
};

// The URL-Keyed Metrics (UKM) service is responsible for gathering and
// uploading reports that contain fine grained performance metrics including
// URLs for top-level navigations.
class UkmService : public UkmRecorderImpl {
 public:
  // Constructs a UkmService.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of UkmService.
  UkmService(PrefService* pref_service,
             metrics::MetricsServiceClient* client,
             bool restrict_to_whitelist_entries);
  ~UkmService() override;

  // Initializes the UKM service.
  void Initialize();

  // Enables/disables transmission of accumulated logs. Logs that have already
  // been created will remain persisted to disk.
  void EnableReporting();
  void DisableReporting();

#if defined(OS_ANDROID) || defined(OS_IOS)
  void OnAppEnterBackground();
  void OnAppEnterForeground();
#endif

  // Records any collected data into logs, and writes to disk.
  void Flush();

  // Deletes any unsent local data.
  void Purge();

  // Resets the client prefs (client_id/session_id). |reason| should be passed
  // to provide the reason of the reset - this is only used for UMA logging.
  void ResetClientState(ResetReason reason);

  // Registers the specified |provider| to provide additional metrics into the
  // UKM log. Should be called during MetricsService initialization only.
  void RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider> provider);

  // Registers the names of all of the preferences used by UkmService in
  // the provided PrefRegistry.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  int32_t report_count() const { return report_count_; }

 private:
  friend ::metrics::UkmBrowserTestBase;
  friend ::metrics::UkmEGTestHelper;
  friend ::ukm::debug::UkmDebugDataExtractor;
  friend ::ukm::UkmUtilsForTest;

  // Starts metrics client initialization.
  void StartInitTask();

  // Called when initialization tasks are complete, to notify the scheduler
  // that it can begin calling RotateLog.
  void FinishedInitTask();

  // Periodically called by scheduler_ to advance processing of logs.
  void RotateLog();

  // Constructs a new Report from available data and stores it in
  // persisted_logs_.
  void BuildAndStoreLog();

  // Starts an upload of the next log from persisted_logs_.
  void StartScheduledUpload();

  // Called by log_uploader_ when the an upload is completed.
  void OnLogUploadComplete(int response_code);

  // ukm::UkmRecorderImpl:
  bool ShouldRestrictToWhitelistedEntries() const override;

  // A weak pointer to the PrefService used to read and write preferences.
  PrefService* pref_service_;

  // If true, only whitelisted Entries should be recorded.
  bool restrict_to_whitelist_entries_;

  // The UKM client id stored in prefs.
  uint64_t client_id_;

  // The UKM session id stored in prefs.
  int32_t session_id_;

  // The number of reports generated this session.
  int32_t report_count_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  metrics::MetricsServiceClient* const client_;

  // Registered metrics providers.
  metrics::DelegatingProvider metrics_providers_;

  // Log reporting service.
  ukm::UkmReportingService reporting_service_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<metrics::MetricsRotationScheduler> scheduler_;

  SEQUENCE_CHECKER(sequence_checker_);

  bool initialize_started_;
  bool initialize_complete_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as UkmService.
  base::WeakPtrFactory<UkmService> self_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UkmService);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SERVICE_H_
