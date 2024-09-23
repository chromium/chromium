// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_
#define COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "net/base/isolation_info.h"
#include "net/base/net_error_details.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace domain_reliability {

// The per-request data that is uploaded to the Domain Reliability collector.
struct DOMAIN_RELIABILITY_EXPORT DomainReliabilityBeacon {
 public:
  DomainReliabilityBeacon();
  DomainReliabilityBeacon(const DomainReliabilityBeacon& other);
  ~DomainReliabilityBeacon();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Outcome {
    // Default value. This should not be recorded to the histogram.
    kUnknown = 0,
    // Successfully uploaded.
    kUploaded = 1,
    // Removed for being expired.
    kExpired = 2,
    // Evicted to make room for newer beacons.
    kEvicted = 3,
    // Deleted for user clearing browsing data.
    kCleared = 5,
    // Beacon was deleted upon context shutdown.
    kContextShutDown = 5,

    // Keep last.
    kMaxValue = kContextShutDown,
  };

  // Converts the Beacon to JSON format for uploading. Calculates the age
  // relative to an upload time of |upload_time|.
  //
  // |last_network_change_time| is used to determine which beacons are
  // labeled as from a previous network connection.
  // |collector_url| is compared to the URLs in the beacons to determine which
  // are being uploaded to a same-origin collector.
  // |path_prefixes| are used to include only a known-safe (not PII) prefix of
  // URLs when uploading to a non-same-origin collector.
  base::Value::Dict ToValue(
      base::TimeTicks upload_time,
      base::TimeTicks last_network_change_time,
      const GURL& collector_url,
      const std::vector<std::unique_ptr<std::string>>& path_prefixes) const;

  // The URL that the beacon is reporting on, if included.
  // The scheme can be non-secure.
  GURL url;
  // The IsolationInfo that must be used when uploading the report.
  // This field does not appear in the uploaded report.
  net::IsolationInfo isolation_info;
  // Status string (e.g. "ok", "dns.nxdomain", "http.403").
  std::string status;
  // Granular QUIC error string (e.g. "quic.peer_going_away").
  std::string quic_error;
  // Net error code.  Encoded as a string in the final JSON.
  int chrome_error;
  // IP address of the server the request went to.
  std::string server_ip;
  // Whether the request went through a proxy. If true, |server_ip| will be
  // empty.
  bool was_proxied;
  // Protocol used to make the request.
  std::string protocol;
  // Network error details for the request.
  net::NetErrorDetails details;
  // HTTP response code returned by the server, or -1 if none was received.
  int http_response_code;
  // Elapsed time between starting and completing the request.
  base::TimeDelta elapsed;
  // Start time of the request.  Encoded as the request age in the final JSON.
  base::TimeTicks start_time;
  // Length of the chain of Domain Reliability uploads leading to this report.
  // Zero if the request was not caused by an upload, one if the request was
  // caused by an upload that itself contained no beacons caused by uploads,
  // et cetera.
  int upload_depth;
  // The probability that this request had of being reported ("sample rate").
  double sample_rate;

  // Records the ultimate outcome of this beacon, for metrics.
  Outcome outcome = Outcome::kUnknown;

  // Okay to copy and assign.
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_
