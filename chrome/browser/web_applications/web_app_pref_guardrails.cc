// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_pref_guardrails.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

// Returns whether the time occurred within X days.
bool TimeOccurredWithinDays(absl::optional<base::Time> time, int days) {
  return time && (base::Time::Now() - time.value()).InDays() < days;
}

}  // namespace

// static
WebAppPrefGuardrails WebAppPrefGuardrails::GetForDesktopInstallIph(
    PrefService* pref_service) {
  return WebAppPrefGuardrails(pref_service, web_app::kIphGuardrails,
                              web_app::kIphPrefNames,
                              /*max_days_to_store_guardrails=*/absl::nullopt);
}

// static
WebAppPrefGuardrails WebAppPrefGuardrails::GetForMlInstallPrompt(
    PrefService* pref_service) {
  return WebAppPrefGuardrails(
      pref_service, web_app::kMlPromoGuardrails, web_app::kMlPromoPrefNames,
      webapps::features::kMaxDaysForMLPromotionGuardrailStorage.Get());
}

// static
WebAppPrefGuardrails WebAppPrefGuardrails::GetForLinkCapturingIph(
    PrefService* pref_service) {
  return WebAppPrefGuardrails(
      pref_service, web_app::kIPHLinkCapturingGuardrails,
      web_app::kIPHLinkCapturingPrefNames,
      features::kLinkCapturingIPHGuardrailStorageDuration.Get());
}

WebAppPrefGuardrails::~WebAppPrefGuardrails() = default;

void WebAppPrefGuardrails::RecordIgnore(const webapps::AppId& app_id,
                                        base::Time time) {
  // The ignore pref keys not being passed is an indication that ignore
  // guardrails need not be measured.
  if (pref_names_->last_ignore_time_name.empty() ||
      pref_names_->not_accepted_count_name.empty()) {
    return;
  }

  if (guardrail_data_->app_specific_mute_after_ignore_days.has_value()) {
    UpdateAppSpecificNotAcceptedPrefs(app_id, time,
                                      pref_names_->last_ignore_time_name);
  }

  if (guardrail_data_->global_mute_after_ignore_days.has_value()) {
    UpdateGlobalNotAcceptedPrefs(time, pref_names_->last_ignore_time_name);
  }
}

void WebAppPrefGuardrails::RecordDismiss(const webapps::AppId& app_id,
                                         base::Time time) {
  // The dismiss pref keys not being passed is an indication that dismiss
  // guardrails need not be measured.
  if (pref_names_->last_dismiss_time_name.empty() ||
      pref_names_->not_accepted_count_name.empty()) {
    return;
  }

  if (guardrail_data_->app_specific_mute_after_dismiss_days.has_value()) {
    UpdateAppSpecificNotAcceptedPrefs(app_id, time,
                                      pref_names_->last_dismiss_time_name);
  }

  if (guardrail_data_->global_mute_after_dismiss_days.has_value()) {
    UpdateGlobalNotAcceptedPrefs(time, pref_names_->last_dismiss_time_name);
  }
}

void WebAppPrefGuardrails::RecordAccept(const webapps::AppId& app_id) {
  UpdateIntWebAppPref(pref_service_, app_id,
                      pref_names_->not_accepted_count_name, 0);

  ScopedDictPrefUpdate update(pref_service_,
                              std::string(pref_names_->global_pref_name));
  update->Set(pref_names_->not_accepted_count_name, 0);

  if (!pref_names_->all_blocked_time_name.empty()) {
    update->Remove(pref_names_->all_blocked_time_name);
  }
}

bool WebAppPrefGuardrails::IsBlockedByGuardrails(const webapps::AppId& app_id) {
  // Since IsBlockedByGuardrails() is called every time to do a check, this is a
  // good place to reset guardrail blocks if any.
  if (ShouldResetGlobalGuardrails()) {
    ResetGlobalGuardrails(app_id);
  }

  absl::optional<std::string> app_block_reason = IsAppBlocked(app_id);
  if (app_block_reason.has_value()) {
    ScopedDictPrefUpdate global_update(
        pref_service_, std::string(pref_names_->global_pref_name));
    LogGlobalBlockReason(global_update, app_block_reason.value());
    return true;
  }

  absl::optional<std::string> global_block_reason = IsGloballyBlocked();
  if (global_block_reason.has_value()) {
    ScopedDictPrefUpdate global_update(
        pref_service_, std::string(pref_names_->global_pref_name));
    LogGlobalBlockReason(global_update, global_block_reason.value());
    if (global_block_reason == "global_not_accept_count_exceeded" &&
        !pref_names_->all_blocked_time_name.empty() && !IsGlobalBlockActive()) {
      global_update->Set(pref_names_->all_blocked_time_name,
                         base::TimeToValue(base::Time::Now()));
    }
    return true;
  }

  return false;
}

WebAppPrefGuardrails::WebAppPrefGuardrails(
    PrefService* pref_service,
    const GuardrailData& guardrail_data,
    const GuardrailPrefNames& guardrail_pref_names,
    absl::optional<int> max_days_to_store_guardrails)
    : pref_service_(pref_service),
      guardrail_data_(guardrail_data),
      pref_names_(guardrail_pref_names),
      max_days_to_store_guardrails_(max_days_to_store_guardrails) {}

absl::optional<std::string> WebAppPrefGuardrails::IsAppBlocked(
    const webapps::AppId& app_id) {
  // Block if user ignored the action for the app N+ times.
  if (guardrail_data_->app_specific_not_accept_count.has_value()) {
    int app_ignored_count =
        GetIntWebAppPref(pref_service_, app_id,
                         pref_names_->not_accepted_count_name)
            .value_or(0);
    if (app_ignored_count >= guardrail_data_->app_specific_not_accept_count) {
      return base::StrCat({"app_specific_not_accept_count_exceeded:", app_id});
    }
  }

  // Block if user ignored the action for the app within N days.
  if (guardrail_data_->app_specific_mute_after_ignore_days.has_value()) {
    auto app_last_ignore = GetTimeWebAppPref(
        pref_service_, app_id, pref_names_->last_ignore_time_name);

    if (TimeOccurredWithinDays(
            app_last_ignore,
            *guardrail_data_->app_specific_mute_after_ignore_days)) {
      return base::StrCat({"app_specific_ignore_days_hit:", app_id});
    }
  }

  // Block if the user dismissed the action for the app within N days.
  if (guardrail_data_->app_specific_mute_after_dismiss_days.has_value()) {
    auto app_last_dismiss_time = GetTimeWebAppPref(
        pref_service_, app_id, pref_names_->last_dismiss_time_name);

    if (TimeOccurredWithinDays(
            app_last_dismiss_time,
            *guardrail_data_->app_specific_mute_after_dismiss_days)) {
      return base::StrCat({"app_specific_dismiss_days_hit:", app_id});
    }
  }

  return absl::nullopt;
}

absl::optional<std::string> WebAppPrefGuardrails::IsGloballyBlocked() {
  const base::Value::Dict& dict =
      pref_service_->GetDict(pref_names_->global_pref_name);

  // Block if user ignored the action last N+ times for any app.
  int global_ignored_count =
      dict.FindInt(pref_names_->not_accepted_count_name).value_or(0);
  if (global_ignored_count >= guardrail_data_->global_not_accept_count) {
    return "global_not_accept_count_exceeded";
  }

  // Block if user ignored the action for any app within N days.
  if (guardrail_data_->global_mute_after_ignore_days.has_value()) {
    auto global_last_ignore =
        base::ValueToTime(dict.Find(pref_names_->last_ignore_time_name));

    if (TimeOccurredWithinDays(
            global_last_ignore,
            *guardrail_data_->global_mute_after_ignore_days)) {
      return "global_ignore_days_hit";
    }
  }

  // Block if user dismissed the action for any app within N days.
  if (guardrail_data_->global_mute_after_dismiss_days.has_value()) {
    auto global_last_dismiss =
        base::ValueToTime(dict.Find(pref_names_->last_dismiss_time_name));

    if (TimeOccurredWithinDays(
            global_last_dismiss,
            *guardrail_data_->global_mute_after_dismiss_days)) {
      return "global_dismiss_days_hit";
    }
  }
  return absl::nullopt;
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

void WebAppPrefGuardrails::UpdateGlobalNotAcceptedPrefs(
    base::Time time,
    std::string_view time_path) {
  // TODO(b/313491176): Optimize so that a single ScopedPrefUpdate call takes
  // place instead of 2. Break this up into seaparate functions that increment
  // the integer pref and sset the time pref, and tkaes in a reference to
  // ScopedDictPrefUpdate.
  ScopedDictPrefUpdate update(pref_service_,
                              std::string(pref_names_->global_pref_name));
  int global_count =
      update->FindInt(pref_names_->not_accepted_count_name).value_or(0);
  update->Set(pref_names_->not_accepted_count_name,
              base::saturated_cast<int>(global_count + 1));
  update->Set(time_path, base::TimeToValue(time));
}

bool WebAppPrefGuardrails::ShouldResetGlobalGuardrails() {
  CHECK(!pref_names_->global_pref_name.empty());
  if (!IsGlobalBlockActive()) {
    return false;
  }

  // If max_days_to_store_guardrails_ is not set, then guardrails need not be
  // reset.
  if (!max_days_to_store_guardrails_.has_value()) {
    return false;
  }

  const base::Value::Dict& dict =
      pref_service_->GetDict(pref_names_->global_pref_name);
  const base::Value* value =
      dict.FindByDottedPath(pref_names_->all_blocked_time_name);
  if (!value) {
    return false;
  }

  absl::optional<base::Time> last_blocked_time = base::ValueToTime(value);

  // We only want to clear the guardrails if max_days_to_store_guardrails_ is
  // crossed.
  return !TimeOccurredWithinDays(last_blocked_time,
                                 *max_days_to_store_guardrails_);
}

void WebAppPrefGuardrails::ResetGlobalGuardrails(const webapps::AppId& app_id) {
  ScopedDictPrefUpdate update(pref_service_,
                              std::string(pref_names_->global_pref_name));
  if (!pref_names_->all_blocked_time_name.empty()) {
    update->Remove(pref_names_->all_blocked_time_name);
  }

  if (!pref_names_->block_reason_name.empty()) {
    update->Remove(pref_names_->block_reason_name);
  }

  update->Set(pref_names_->not_accepted_count_name, 0);
}

bool WebAppPrefGuardrails::IsGlobalBlockActive() {
  CHECK(!pref_names_->global_pref_name.empty());
  if (pref_names_->all_blocked_time_name.empty()) {
    return false;
  }
  const base::Value::Dict& dict =
      pref_service_->GetDict(pref_names_->global_pref_name);
  return dict.contains(pref_names_->all_blocked_time_name);
}

void WebAppPrefGuardrails::LogGlobalBlockReason(
    ScopedDictPrefUpdate& global_update,
    const std::string& reason) {
  if (pref_names_->block_reason_name.empty() ||
      pref_names_->global_pref_name.empty()) {
    return;
  }

  global_update->Set(pref_names_->block_reason_name, reason);
}

}  // namespace web_app
