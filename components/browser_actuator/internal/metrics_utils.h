// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_METRICS_UTILS_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_METRICS_UTILS_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/browser_actuator/public/common.h"

namespace browser_actuator {

// Maps a PayloadType to its corresponding histogram variant name suffix.
// LINT.IfChange(PayloadTypeToMetricSuffix)
inline std::string_view PayloadTypeToMetricSuffix(PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kControl:
      return "Control";
    case PayloadType::kExperimentalTriggering:
      return "GlicExperimentalTriggering";
    case PayloadType::kUnspecified:
      NOTREACHED();
  }
  NOTREACHED();
}
// LINT.ThenChange(
//     //components/browser_actuator/public/common.h:PayloadType,
//     //tools/metrics/histograms/metadata/browser_actuator/histograms.xml)

// Why a downstream typed payload was discarded before handler delivery.
// LINT.IfChange(DownstreamPayloadDropReason)
enum class DownstreamPayloadDropReason {
  // The wire enum was UNSPECIFIED or an unknown payload type.
  kUnknownType = 0,
  // `Any.type_url` did not match the expected URL for `payload_type`.
  kTypeUrlMismatch = 1,
  // Nothing registered a factory for this payload type. This is the steady
  // state when the owning feature is disabled.
  kNoHandler = 2,
  // A handler exists in principle, and the payload could not be delivered: the
  // channel was gone, the session had been destroyed, or the factory declined
  // to build a handler. Unlike `kNoHandler`, this is always unexpected.
  kDispatchFailed = 3,
  kMaxValue = kDispatchFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:BrowserActuatorPayloadDropReason)

inline void RecordDownstreamPayloadDropped(DownstreamPayloadDropReason reason) {
  base::UmaHistogramEnumeration("Browser.Actuator.Downstream.PayloadDropped",
                                reason);
}

// Tracks and records telemetry metrics for an in-flight upstream request.
class UpstreamRequestLog {
 public:
  explicit UpstreamRequestLog(PayloadType payload_type)
      : histogram_suffix_(PayloadTypeToMetricSuffix(payload_type)),
        start_time_(base::TimeTicks::Now()) {}
  ~UpstreamRequestLog() = default;

  UpstreamRequestLog(UpstreamRequestLog&&) = default;
  UpstreamRequestLog& operator=(UpstreamRequestLog&&) = default;

  UpstreamRequestLog(const UpstreamRequestLog&) = delete;
  UpstreamRequestLog& operator=(const UpstreamRequestLog&) = delete;

  void RecordPayloadSize(size_t size_bytes) const {
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"Browser.Actuator.Upstream.PayloadSize.", histogram_suffix_}),
        size_bytes);
  }

  void RecordResponse(std::optional<int> http_status_code) const {
    base::TimeDelta latency = base::TimeTicks::Now() - start_time_;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"Browser.Actuator.Upstream.UploadLatency.", histogram_suffix_}),
        latency);

    if (http_status_code.has_value()) {
      base::UmaHistogramSparse("Browser.Actuator.Upstream.HttpStatus",
                               *http_status_code);
    }
  }

 private:
  std::string_view histogram_suffix_;
  base::TimeTicks start_time_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_METRICS_UTILS_H_
