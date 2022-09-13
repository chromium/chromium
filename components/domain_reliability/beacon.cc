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

base::Value DomainReliabilityBeacon::ToValue(
    base::TimeTicks upload_time,
    base::TimeTicks last_network_change_time,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes) const {
  base::Value beacon_value(base::Value::Type::DICTIONARY);
  DCHECK(url.is_valid());
  GURL sanitized_url = SanitizeURLForReport(url, collector_url, path_prefixes);
  beacon_value.SetStringKey("url", sanitized_url.spec());
  beacon_value.SetStringKey("status", status);
  if (!quic_error.empty())
    beacon_value.SetStringKey("quic_error", quic_error);
  if (chrome_error != net::OK) {
    base::Value failure_value(base::Value::Type::DICTIONARY);
    failure_value.SetStringKey("custom_error",
                               net::ErrorToString(chrome_error));
    beacon_value.SetKey("failure_data", std::move(failure_value));
  }
  beacon_value.SetStringKey("server_ip", server_ip);
  beacon_value.SetBoolKey("was_proxied", was_proxied);
  beacon_value.SetStringKey("protocol", protocol);
  if (details.quic_broken)
    beacon_value.SetBoolKey("quic_broken", details.quic_broken);
  if (details.quic_port_migration_detected)
    beacon_value.SetBoolKey("quic_port_migration_detected",
                            details.quic_port_migration_detected);
  if (http_response_code >= 0)
    beacon_value.SetIntKey("http_response_code", http_response_code);
  beacon_value.SetIntKey("request_elapsed_ms", elapsed.InMilliseconds());
  base::TimeDelta request_age = upload_time - start_time;
  beacon_value.SetIntKey("request_age_ms", request_age.InMilliseconds());
  bool network_changed = last_network_change_time > start_time;
  beacon_value.SetBoolKey("network_changed", network_changed);
  beacon_value.SetDoubleKey("sample_rate", sample_rate);
  return beacon_value;
}

}  // namespace domain_reliability
