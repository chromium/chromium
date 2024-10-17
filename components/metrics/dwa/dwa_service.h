// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
#define COMPONENTS_METRICS_DWA_DWA_SERVICE_H_

#include <cstdint>

#include "base/component_export.h"
#include "components/metrics/metrics_service_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/dwa/deidentified_web_analytics.pb.h"

namespace metrics::dwa {

// The DwaService is responsible for collecting and uploading deindentified web
// analytics events.
class COMPONENT_EXPORT(DWA) DwaService {
 public:
  DwaService();
  ~DwaService();

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
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
