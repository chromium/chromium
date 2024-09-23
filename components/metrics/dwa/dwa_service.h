// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
#define COMPONENTS_METRICS_DWA_DWA_SERVICE_H_

#include "base/component_export.h"
#include "components/metrics/metrics_service_client.h"
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
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
