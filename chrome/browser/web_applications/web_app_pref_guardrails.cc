// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_pref_guardrails.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/json/values_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

// static
WebAppPrefGuardrails WebAppPrefGuardrails::GetForDesktopInstallIph(
    PrefService* pref_service) {
  return WebAppPrefGuardrails(pref_service, web_app::kIphGuardrails,
                              web_app::kIphPrefNames);
}

WebAppPrefGuardrails::~WebAppPrefGuardrails() = default;

void WebAppPrefGuardrails::RecordIgnore(const webapps::AppId& app_id,
                                        base::Time time) {
  // If the ignore count guardrails are not defined or the pref names are not
  // set, exit early since the ignore guardrails are not required to be
  // counted.
  if (!HasIgnoreGuardrails()) {
    return;
  }

  UpdateAppSpecificNotAcceptedPrefs(app_id, time,
                                    pref_names_->last_ignore_time_name);
  UpdateAppAgnosticNotAcceptedPrefs(time, pref_names_->last_ignore_time_name);
}

void WebAppPrefGuardrails::RecordDismiss(const webapps::AppId& app_id,
                                         base::Time time) {
  // If the dismiss count guardrails are not defined or the pref names are not
  // set, exit early since the dismiss guardrails are not required to be
  // counted.
  if (!HasDismissGuardrails()) {
    return;
  }

  UpdateAppSpecificNotAcceptedPrefs(app_id, time,
                                    pref_names_->last_dismiss_time_name);
  UpdateAppAgnosticNotAcceptedPrefs(time, pref_names_->last_dismiss_time_name);
}

void WebAppPrefGuardrails::RecordAccept(const webapps::AppId& app_id) {
  UpdateIntWebAppPref(pref_service_, app_id,
                      pref_names_->not_accepted_count_name, 0);

  ScopedDictPrefUpdate update(pref_service_,
                              std::string(pref_names_->agnostic_pref_name));
  update->Set(pref_names_->not_accepted_count_name, 0);

  if (!pref_names_->all_blocked_name.empty()) {
    update->Remove(pref_names_->all_blocked_name);
  }
}

bool WebAppPrefGuardrails::IsBlockedByGuardrails(const webapps::AppId& app_id) {
  bool blocked_due_to_ignore = false;
  if (HasIgnoreGuardrails()) {
    blocked_due_to_ignore = ComputeBlockedDueToIgnoreCounts(app_id);
  }
  return blocked_due_to_ignore;
}

WebAppPrefGuardrails::WebAppPrefGuardrails(
    PrefService* pref_service,
    const GuardrailData& guardrail_data,
    const GuardrailPrefNames& guardrail_pref_names)
    : pref_service_(pref_service),
      guardrail_data_(guardrail_data),
      pref_names_(guardrail_pref_names) {}

WebAppPrefGuardrails::WebAppPrefGuardrails(
    const WebAppPrefGuardrails& web_app) = default;

bool WebAppPrefGuardrails::HasIgnoreGuardrails() {
  return !pref_names_->not_accepted_count_name.empty() &&
         !pref_names_->last_ignore_time_name.empty() &&
         guardrail_data_->app_specific_not_accept_count.has_value() &&
         guardrail_data_->app_agnostic_not_accept_count.has_value();
}

bool WebAppPrefGuardrails::HasDismissGuardrails() {
  return !pref_names_->last_dismiss_time_name.empty() &&
         !pref_names_->not_accepted_count_name.empty() &&
         guardrail_data_->app_specific_mute_after_dismiss_days.has_value() &&
         guardrail_data_->app_agnostic_mute_after_dismiss_days.has_value();
}

bool WebAppPrefGuardrails::ComputeBlockedDueToIgnoreCounts(
    const webapps::AppId& app_id) {
  CHECK(HasIgnoreGuardrails());

  // Block if user ignored the action for the app N+ times.
  int app_ignored_count = GetIntWebAppPref(pref_service_, app_id,
                                           pref_names_->not_accepted_count_name)
                              .value_or(0);
  if (app_ignored_count >= *guardrail_data_->app_specific_not_accept_count) {
    return true;
  }
  // Block if user ignored the action for the app within N days.
  auto app_last_ignore = GetTimeWebAppPref(pref_service_, app_id,
                                           pref_names_->last_ignore_time_name);

  if (guardrail_data_->app_specific_mute_after_ignore_days.has_value() &&
      TimeOccurredWithinDays(
          app_last_ignore,
          *guardrail_data_->app_specific_mute_after_ignore_days)) {
    return true;
  }

  const base::Value::Dict& dict =
      pref_service_->GetDict(pref_names_->agnostic_pref_name);

  // Block if user ignored the action last N+ times for any app.
  int global_ignored_count =
      dict.FindInt(pref_names_->not_accepted_count_name).value_or(0);
  if (global_ignored_count >= *guardrail_data_->app_agnostic_not_accept_count) {
    return true;
  }
  // Block if user ignored the action for any app within N days.
  auto global_last_ignore =
      base::ValueToTime(dict.Find(pref_names_->last_ignore_time_name));

  if (guardrail_data_->app_agnostic_mute_after_ignore_days.has_value() &&
      TimeOccurredWithinDays(
          global_last_ignore,
          *guardrail_data_->app_agnostic_mute_after_ignore_days)) {
    return true;
  }
  return false;
}

void WebAppPrefGuardrails::UpdateAppSpecificNotAcceptedPrefs(
    const webapps::AppId& app_id,
    base::Time time,
    std::string_view time_path) {
  // TODO(b/313491176): Optimize so that a single ScopedPrefUpdate call takes
  // place instead of 2. Break this up into seaparate functions that increment
  // the integer pref and sset the time pref, and tkaes in a reference to
  // ScopedDictPrefUpdate.
  absl::optional<int> ignored_count = GetIntWebAppPref(
      pref_service_, app_id, pref_names_->not_accepted_count_name);
  int new_count = base::saturated_cast<int>(1 + ignored_count.value_or(0));

  UpdateIntWebAppPref(pref_service_, app_id,
                      pref_names_->not_accepted_count_name, new_count);
  UpdateTimeWebAppPref(pref_service_, app_id, time_path, time);
}

void WebAppPrefGuardrails::UpdateAppAgnosticNotAcceptedPrefs(
    base::Time time,
    std::string_view time_path) {
  // TODO(b/313491176): Optimize so that a single ScopedPrefUpdate call takes
  // place instead of 2. Break this up into seaparate functions that increment
  // the integer pref and sset the time pref, and tkaes in a reference to
  // ScopedDictPrefUpdate.
  ScopedDictPrefUpdate update(pref_service_,
                              std::string(pref_names_->agnostic_pref_name));
  int global_count =
      update->FindInt(pref_names_->not_accepted_count_name).value_or(0);
  update->Set(pref_names_->not_accepted_count_name,
              base::saturated_cast<int>(global_count + 1));
  update->Set(time_path, base::TimeToValue(time));
}

}  // namespace web_app
