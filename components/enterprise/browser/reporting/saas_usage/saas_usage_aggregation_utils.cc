// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"

#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

constexpr char kNavigationCount[] = "navigation_count";
constexpr char kEncryptionProtocols[] = "encryption_protocols";

base::DictValue& GetOrCreateEntry(base::DictValue& report_dict,
                                  std::string_view domain) {
  base::DictValue* entry = report_dict.FindDict(domain);
  if (!entry) {
    base::DictValue new_entry =
        base::DictValue()
            .Set(kNavigationCount, 0)
            .Set(kEncryptionProtocols, base::ListValue());
    entry = report_dict.Set(domain, std::move(new_entry))->GetIfDict();
  }
  return *entry;
}

}  // namespace

namespace enterprise_reporting {

void RecordNavigation(PrefService* pref_service,
                      std::string_view domain,
                      std::string_view encryption_protocol) {
  CHECK(pref_service);
  ScopedDictPrefUpdate update(pref_service, kSaasUsageReport);
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
