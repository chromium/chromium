// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/factory_ping_embargo_check.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash::system {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// These values must match the corresponding enum defined in enums.xml.
enum class EndDateValidityHistogramValue {
  kMalformed = 0,
  kInvalid = 1,
  kValid = 2,
  kMaxValue = kValid,
};

// This is in a helper function to help the compiler avoid generating duplicated
// code.
void RecordEndDateValidity(const char* uma_prefix,
                           EndDateValidityHistogramValue value) {
  if (uma_prefix)
    UMA_HISTOGRAM_ENUMERATION(std::string(uma_prefix) + ".EndDateValidity",
                              value);
}

}  // namespace

FactoryPingEmbargoState GetPingEmbargoState(
    StatisticsProvider* statistics_provider,
    const std::string& key_name,
    const char* uma_prefix) {
  const std::optional<std::string_view> ping_embargo_end_date =
      statistics_provider->GetMachineStatistic(key_name);
  if (!ping_embargo_end_date) {
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }
  base::Time parsed_time;
  if (!base::Time::FromUTCString(ping_embargo_end_date->data(), &parsed_time)) {
    LOG(ERROR) << key_name << " exists but cannot be parsed.";
    RecordEndDateValidity(uma_prefix,
                          EndDateValidityHistogramValue::kMalformed);
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }

  if (parsed_time - base::Time::Now() >= kEmbargoEndDateGarbageDateThreshold) {
    // If the date is more than this many days in the future, ignore it.
    // Because it indicates that the device is not connected to an NTP server
    // in the factory, and its internal clock could be off when the date is
    // written.
    RecordEndDateValidity(uma_prefix, EndDateValidityHistogramValue::kInvalid);
    return FactoryPingEmbargoState::kInvalid;
  }

  RecordEndDateValidity(uma_prefix, EndDateValidityHistogramValue::kValid);
  return base::Time::Now() > parsed_time ? FactoryPingEmbargoState::kPassed
                                         : FactoryPingEmbargoState::kNotPassed;
}

FactoryPingEmbargoState GetRlzPingEmbargoState(
    StatisticsProvider* statistics_provider) {
  return GetPingEmbargoState(statistics_provider, kRlzEmbargoEndDateKey,
                             /*uma_prefix=*/nullptr);
}

}  // namespace ash::system
