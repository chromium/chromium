// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/beacon.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/domain_reliability/util.h"
#include "net/base/net_errors.h"

namespace domain_reliability {

DomainReliabilityBeacon::DomainReliabilityBeacon() = default;

DomainReliabilityBeacon::DomainReliabilityBeacon(
    const DomainReliabilityBeacon& other) = default;

DomainReliabilityBeacon::~DomainReliabilityBeacon() {
  if (outcome != Outcome::kUnknown) {
    base::UmaHistogramEnumeration("Net.DomainReliability.BeaconOutcome",
                                  outcome);
  }
}

base::Value::Dict DomainReliabilityBeacon::ToValue(
    base::TimeTicks upload_time,
    base::TimeTicks last_network_change_time,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes) const {
  base::Value::Dict beacon_value;
  DCHECK(url.is_valid());
  GURL sanitized_url = SanitizeURLForReport(url, collector_url, path_prefixes);
  beacon_value.Set("url", sanitized_url.spec());
  beacon_value.Set("status", status);
  if (!quic_error.empty()) {
    beacon_value.Set("quic_error", quic_error);
  }
  if (chrome_error != net::OK) {
    base::Value::Dict failure_value;
    failure_value.Set("custom_error", net::ErrorToString(chrome_error));
    beacon_value.Set("failure_data", std::move(failure_value));
  }
  beacon_value.Set("server_ip", server_ip);
  beacon_value.Set("was_proxied", was_proxied);
  beacon_value.Set("protocol", protocol);
  if (details.quic_broken) {
    beacon_value.Set("quic_broken", details.quic_broken);
  }
  if (details.quic_port_migration_detected) {
    beacon_value.Set("quic_port_migration_detected",
                     details.quic_port_migration_detected);
  }
  if (http_response_code >= 0) {
    beacon_value.Set("http_response_code", http_response_code);
  }
  beacon_value.Set("request_elapsed_ms", static_cast<int>(elapsed.InMilliseconds()));
  base::TimeDelta request_age = upload_time - start_time;
  beacon_value.Set("request_age_ms", static_cast<int>(request_age.InMilliseconds()));
  bool network_changed = last_network_change_time > start_time;
  beacon_value.Set("network_changed", network_changed);
  beacon_value.Set("sample_rate", sample_rate);
  return beacon_value;
}

}  // namespace domain_reliability
