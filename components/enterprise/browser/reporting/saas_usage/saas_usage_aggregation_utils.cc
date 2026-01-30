// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
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

}  // namespace enterprise_reporting
