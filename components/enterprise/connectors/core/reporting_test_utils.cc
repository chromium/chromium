// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_test_utils.h"

#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace enterprise_connectors::test {

namespace {

base::Value::List CreateOptInEventsList(
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::Value::List enabled_opt_in_events_list;
  for (const auto& enabled_opt_in_event : enabled_opt_in_events) {
    base::Value::Dict event_value;
    event_value.Set(kKeyOptInEventName, enabled_opt_in_event.first);

    base::Value::List url_patterns_list;
    for (const auto& url_pattern : enabled_opt_in_event.second) {
      url_patterns_list.Append(url_pattern);
    }
    event_value.Set(kKeyOptInEventUrlPatterns, std::move(url_patterns_list));

    enabled_opt_in_events_list.Append(std::move(event_value));
  }
  return enabled_opt_in_events_list;
}

}  // namespace

void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names,
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events,
    bool machine_scope) {
  ScopedListPrefUpdate settings_list(prefs, kOnSecurityEventPref);
  settings_list->clear();
  prefs->ClearPref(kOnSecurityEventScopePref);
  if (!enabled) {
    return;
  }

  base::Value::Dict settings;

  settings.Set(kKeyServiceProvider, base::Value("google"));
  if (!enabled_event_names.empty()) {
    base::Value::List enabled_event_name_list;
    for (const auto& enabled_event_name : enabled_event_names) {
      enabled_event_name_list.Append(enabled_event_name);
    }
    settings.Set(kKeyEnabledEventNames, std::move(enabled_event_name_list));
  }

  if (!enabled_opt_in_events.empty()) {
    settings.Set(kKeyEnabledOptInEvents,
                 CreateOptInEventsList(enabled_opt_in_events));
  }

  settings_list->Append(std::move(settings));

  prefs->SetInteger(
      kOnSecurityEventScopePref,
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

}  // namespace enterprise_connectors::test
