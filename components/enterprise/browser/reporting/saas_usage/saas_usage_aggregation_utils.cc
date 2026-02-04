// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

constexpr char kNavigationCount[] = "navigation_count";
constexpr char kEncryptionProtocols[] = "encryption_protocols";
constexpr char kFirstSeenTime[] = "first_seen_time";
constexpr char kLastSeenTime[] = "last_seen_time";

base::DictValue& GetOrCreateEntry(base::DictValue& report_dict,
                                  std::string_view domain) {
  base::DictValue* entry = report_dict.FindDict(domain);
  auto now = base::TimeToValue(base::Time::Now());
  if (!entry) {
    base::DictValue new_entry =
        base::DictValue()
            .Set(kNavigationCount, 0)
            .Set(kEncryptionProtocols, base::ListValue())
            .Set(kFirstSeenTime, now.Clone());
    entry = report_dict.Set(domain, std::move(new_entry))->GetIfDict();
  }
  // This function is called every time a navigation is recorded, so we
  // update the end time to Now.
  entry->Set(kLastSeenTime, std::move(now));
  return *entry;
}

bool IsValidEntry(const base::Value& value, std::string_view domain) {
  if (!value.is_dict()) {
    LOG(ERROR) << "Invalid entry for domain: " << domain
               << ". Prefs may be corrupted.";
    return false;
  }

  const base::DictValue& entry = value.GetDict();
  if (!entry.FindInt(kNavigationCount).has_value()) {
    LOG(ERROR) << "Invalid navigation count for domain: " << domain
               << ". Prefs may be corrupted.";
    return false;
  }
  if (!entry.FindList(kEncryptionProtocols)) {
    LOG(ERROR) << "Invalid encryption protocols type for domain: " << domain
               << ". Prefs may be corrupted.";
    return false;
  }
  for (const auto& protocol : *entry.FindList(kEncryptionProtocols)) {
    if (!protocol.GetIfString()) {
      LOG(ERROR) << "Invalid encryption protocol found for domain: " << domain
                 << ". Prefs may be corrupted.";
      return false;
    }
  }

  if (!base::ValueToTime(entry.Find(kFirstSeenTime)).has_value()) {
    LOG(ERROR) << "Invalid first seen time for domain: " << domain
               << ". Prefs may be corrupted.";
    return false;
  }
  if (!base::ValueToTime(entry.Find(kLastSeenTime)).has_value()) {
    LOG(ERROR) << "Invalid last seen time for domain: " << domain
               << ". Prefs may be corrupted.";
    return false;
  }
  return true;
}

}  // namespace

namespace enterprise_reporting {

void RecordNavigation(PrefService& pref_service,
                      std::string_view domain,
                      std::string_view encryption_protocol) {
  ScopedDictPrefUpdate update(&pref_service, kSaasUsageReport);
  base::DictValue& entry = GetOrCreateEntry(*update, domain);

  int count = entry.FindInt(kNavigationCount).value_or(0);
  entry.Set(kNavigationCount, count + 1);

  if (encryption_protocol.empty()) {
    return;
  }

  base::ListValue* protocols = entry.FindList(kEncryptionProtocols);
  CHECK(protocols);

  if (!protocols->contains(encryption_protocol)) {
    protocols->Append(encryption_protocol);
  }
}

void PopulateSaasUsageDomainMetrics(
    PrefService& pref_service,
    ::chrome::cros::reporting::proto::SaasUsageReportEvent& report) {
  for (const auto [domain, entry] : pref_service.GetDict(kSaasUsageReport)) {
    if (!IsValidEntry(entry, domain)) {
      continue;
    }
    const base::DictValue& dict = entry.GetDict();
    auto* domain_metrics = report.add_domain_metrics();
    domain_metrics->set_domain(domain);
    domain_metrics->set_visit_count(dict.FindInt(kNavigationCount).value());
    domain_metrics->set_start_time_millis(
        base::ValueToTime(dict.Find(kFirstSeenTime))
            .value()
            .InMillisecondsSinceUnixEpoch());
    domain_metrics->set_end_time_millis(
        base::ValueToTime(dict.Find(kLastSeenTime))
            .value()
            .InMillisecondsSinceUnixEpoch());

    const base::ListValue* protocols = dict.FindList(kEncryptionProtocols);
    for (const auto& protocol : *protocols) {
      domain_metrics->add_encryption_protocols(protocol.GetString());
    }
  }
}

void ClearSaasUsageReport(PrefService& pref_service) {
  pref_service.ClearPref(kSaasUsageReport);
}

}  // namespace enterprise_reporting
