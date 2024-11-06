// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
#define COMPONENTS_METRICS_DWA_DWA_SERVICE_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/dwa_reporting_service.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/dwa/deidentified_web_analytics.pb.h"

namespace metrics::dwa {

// The DwaService is responsible for collecting and uploading deindentified web
// analytics events.
class COMPONENT_EXPORT(DWA) DwaService {
 public:
  DwaService(MetricsServiceClient* client, PrefService* pref_service);
  DwaService(const DwaService&) = delete;
  DwaService& operator=(const DwaService&) = delete;

  ~DwaService();

  void EnableReporting();
  void DisableReporting();

  // Flushes any event currently in the recorder to prefs.
  void Flush(metrics::MetricsLogsEventManager::CreateReason reason);

  // Clears all event and log data.
  void Purge();

  // Records coarse system profile into CoarseSystemInfo of the deidentified web
  // analytics report proto.
  static void RecordCoarseSystemInformation(
      MetricsServiceClient& client,
      const PrefService& local_state,
      ::dwa::CoarseSystemInfo* coarse_system_info);

  // Generate client id which changes between days. We store this id in a
  // uint64 instead of base::Uuid as it is eventually stored in a proto with
  // this type. We are not concerned with id collisions as ids are only meant to
  // be compared within single days and they are used for k-anonymity (where it
  // would mean undercounting for k-anonymity).
  static uint64_t GetEphemeralClientId(PrefService& local_state);

  // Register prefs from `dwa_pref_names.h`.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  metrics::UnsentLogStore* unsent_log_store();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Periodically called by |scheduler_| to advance processing of logs.
  void RotateLog();

  // Constructs a new DeidentifiedWebAnalyticsReport from available data and
  // stores it in |unsent_log_store_|.
  void BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason reason);

  // Retrieves the storage parameters to control the reporting service.
  static UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits();

  // Manages on-device recording of events.
  raw_ptr<DwaRecorder> recorder_;

  // The metrics client |this| is service is associated.
  raw_ptr<MetricsServiceClient> client_;

  // A weak pointer to the PrefService used to read and write preferences.
  raw_ptr<PrefService> pref_service_;

  // Service for uploading serialized logs.
  DwaReportingService reporting_service_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<MetricsRotationScheduler> scheduler_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as DwaService.
  base::WeakPtrFactory<DwaService> self_ptr_factory_{this};
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
