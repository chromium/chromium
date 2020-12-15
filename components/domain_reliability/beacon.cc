// Copyright 2014 The Chromium Authors. All rights reserved.
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

using base::Value;
using base::DictionaryValue;

DomainReliabilityBeacon::DomainReliabilityBeacon() = default;

DomainReliabilityBeacon::DomainReliabilityBeacon(
    const DomainReliabilityBeacon& other) = default;

DomainReliabilityBeacon::~DomainReliabilityBeacon() {
  if (outcome != Outcome::kUnknown) {
    base::UmaHistogramEnumeration("Net.DomainReliability.BeaconOutcome",
                                  outcome);
  }
}

std::unique_ptr<Value> DomainReliabilityBeacon::ToValue(
    base::TimeTicks upload_time,
    base::TimeTicks last_network_change_time,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes) const {
  auto beacon_value = std::make_unique<DictionaryValue>();
  DCHECK(url.is_valid());
  GURL sanitized_url = SanitizeURLForReport(url, collector_url, path_prefixes);
  beacon_value->SetString("url", sanitized_url.spec());
  beacon_value->SetString("status", status);
  if (!quic_error.empty())
    beacon_value->SetString("quic_error", quic_error);
  if (chrome_error != net::OK) {
    auto failure_value = std::make_unique<DictionaryValue>();
    failure_value->SetString("custom_error",
                             net::ErrorToString(chrome_error));
    beacon_value->Set("failure_data", std::move(failure_value));
  }
  beacon_value->SetString("server_ip", server_ip);
  beacon_value->SetBoolean("was_proxied", was_proxied);
  beacon_value->SetString("protocol", protocol);
  if (details.quic_broken)
    beacon_value->SetBoolean("quic_broken", details.quic_broken);
  if (details.quic_port_migration_detected)
    beacon_value->SetBoolean("quic_port_migration_detected",
                             details.quic_port_migration_detected);
  if (http_response_code >= 0)
    beacon_value->SetInteger("http_response_code", http_response_code);
  beacon_value->SetInteger("request_elapsed_ms", elapsed.InMilliseconds());
  base::TimeDelta request_age = upload_time - start_time;
  beacon_value->SetInteger("request_age_ms", request_age.InMilliseconds());
  bool network_changed = last_network_change_time > start_time;
  beacon_value->SetBoolean("network_changed", network_changed);
  beacon_value->SetDouble("sample_rate", sample_rate);
  return std::move(beacon_value);
}

}  // namespace domain_reliability
