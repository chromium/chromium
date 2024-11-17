// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_service_settings.h"

#include "components/enterprise/connectors/core/service_provider_config.h"

namespace enterprise_connectors {
namespace {

}  // namespace

ReportingServiceSettings::ReportingServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;
  const base::Value::Dict& settings_dict = settings_value.GetDict();

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_dict.FindString(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_name_ = *service_provider_name;
    if (service_provider_config.count(service_provider_name_)) {
      reporting_config_ =
          service_provider_config.at(service_provider_name_).reporting;
    }
  }
  if (!reporting_config_) {
    DLOG(ERROR) << "No reporting config for corresponding service provider";
    return;
  }

  const base::Value::List* enabled_event_name_list_value =
      settings_dict.FindList(kKeyEnabledEventNames);
  if (enabled_event_name_list_value) {
    for (const base::Value& enabled_event_name_value :
         *enabled_event_name_list_value) {
      if (enabled_event_name_value.is_string())
        enabled_event_names_.insert(enabled_event_name_value.GetString());
      else
        DVLOG(1) << "Enabled event name list contains a non string value!";
    }
  } else {
    // When the list of enabled event names is not set, we assume all events are
    // enabled. This is to support the feature of selecting the "All always on"
    // option in the policy UI, which means to always enable all events, even
    // when new events may be added in the future. And this is also to support
    // existing customer policies that were created before we introduced the
    // concept of enabling/disabling events.
    for (const char* event : kAllReportingEvents) {
      enabled_event_names_.insert(event);
    }
  }

  const base::Value::List* enabled_opt_in_events_value =
      settings_dict.FindList(kKeyEnabledOptInEvents);
  if (enabled_opt_in_events_value) {
    for (const base::Value& event : *enabled_opt_in_events_value) {
      DCHECK(event.is_dict());
      const std::string* name = event.GetDict().FindString(kKeyOptInEventName);
      const base::Value::List* url_patterns_value =
          event.GetDict().FindList(kKeyOptInEventUrlPatterns);

      DCHECK(url_patterns_value);
      for (const base::Value& url_pattern : *url_patterns_value) {
        DCHECK(url_pattern.is_string());

        enabled_opt_in_events_[*name].push_back(url_pattern.GetString());
      }
    }
  }
}

std::optional<ReportingSettings>
ReportingServiceSettings::GetReportingSettings() const {
  if (!IsValid())
    return std::nullopt;

  ReportingSettings settings;

  settings.enabled_event_names.insert(enabled_event_names_.begin(),
                                      enabled_event_names_.end());

  settings.enabled_opt_in_events.insert(enabled_opt_in_events_.begin(),
                                        enabled_opt_in_events_.end());

  return settings;
}

bool ReportingServiceSettings::IsValid() const {
  // The settings are valid only if a provider with a reporting config was used,
  // and events are enabled. The list could be empty. The absence of a list
  // means "all events", but the presence of an empty list means "no events".
  return reporting_config_ && !enabled_event_names_.empty();
}

ReportingServiceSettings::ReportingServiceSettings(ReportingServiceSettings&&) =
    default;
ReportingServiceSettings::~ReportingServiceSettings() = default;

}  // namespace enterprise_connectors
