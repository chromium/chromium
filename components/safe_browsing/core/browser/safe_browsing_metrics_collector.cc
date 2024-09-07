// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

using EventType = safe_browsing::SafeBrowsingMetricsCollector::EventType;
using UserState = safe_browsing::SafeBrowsingMetricsCollector::UserState;
using SafeBrowsingState = safe_browsing::SafeBrowsingState;

const int kMetricsLoggingIntervalDay = 1;

// The max length of event timestamps stored in pref.
const int kTimestampsMaxLength = 30;
// The quota for ESB disabled metrics. ESB disabled metrics should not be logged
// more than the quota in a week.
const int kEsbDisabledMetricsQuota = 3;
// Events that are older than 30 days are removed from pref.
const int kEventMaxDurationDay = 30;
// The ESB enabled duration is considered short if it's under 1 hour, long if
// it's at least 24 hours, and medium if it's in between.
const int kEsbShortEnabledUpperBoundHours = 1;
const int kEsbLongEnabledLowerBoundHours = 24;

std::string EventTypeToPrefKey(const EventType& type) {
  return base::NumberToString(static_cast<int>(type));
}

std::string UserStateToPrefKey(const UserState& user_state) {
  return base::NumberToString(static_cast<int>(user_state));
}

base::Value TimeToPrefValue(const base::Time& time) {
  return base::Int64ToValue(time.ToDeltaSinceWindowsEpoch().InSeconds());
}

base::Time PrefValueToTime(const base::Value& value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(base::ValueToInt64(value).value_or(0)));
}

}  // namespace

namespace safe_browsing {

SafeBrowsingMetricsCollector::SafeBrowsingMetricsCollector(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &SafeBrowsingMetricsCollector::OnEnhancedProtectionPrefChanged,
          base::Unretained(this)));
}

void SafeBrowsingMetricsCollector::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

void SafeBrowsingMetricsCollector::StartLogging() {
  base::TimeDelta log_interval = base::Days(kMetricsLoggingIntervalDay);
  base::Time last_log_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(
          pref_service_->GetInt64(prefs::kSafeBrowsingMetricsLastLogTime)));
  base::TimeDelta delay = base::Time::Now() - last_log_time;
  if (delay >= log_interval) {
    LogMetricsAndScheduleNextLogging();
  } else {
    ScheduleNextLoggingAfterInterval(log_interval - delay);
  }
}

void SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging() {
  LogDailyOptInMetrics();
  LogDailyEventMetrics();
  MaybeLogDailyEsbProtegoPingSent();
  RemoveOldEventsFromPref();

  pref_service_->SetInt64(
      prefs::kSafeBrowsingMetricsLastLogTime,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  ScheduleNextLoggingAfterInterval(base::Days(kMetricsLoggingIntervalDay));
}

void SafeBrowsingMetricsCollector::MaybeLogDailyEsbProtegoPingSent() {
  if (GetSafeBrowsingState(*pref_service_) !=
      SafeBrowsingState::ENHANCED_PROTECTION) {
    return;
  }

  auto last_ping_with_token = pref_service_->GetTime(
      prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime);
  auto last_ping_without_token = pref_service_->GetTime(
      prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime);
  auto most_recent_ping_type = last_ping_with_token > last_ping_without_token
                                   ? ProtegoPingType::kWithToken
                                   : ProtegoPingType::kWithoutToken;
  auto most_recent_ping_time =
      std::max(last_ping_with_token, last_ping_without_token);

  auto most_recent_collector_run_time = PrefValueToTime(
      pref_service_->GetValue(prefs::kSafeBrowsingMetricsLastLogTime));

  bool sent_ping_since_last_collector_run =
      most_recent_ping_time > most_recent_collector_run_time;

  auto logged_ping_type = ProtegoPingType::kNone;

  if (base::Time::Now() - last_ping_with_token < base::Hours(24)) {
    // If a ping with token was sent within the last 24 hours,
    // the most recent ping type is kWithToken.
    // If both last_ping_with_token and last_ping_without_token are present,
    // we log kWithToken instead of kWithoutToken because if a token has been
    // sent before, we are certain that this account is a signed in account
    // and the server has received the token.
    // The kWithoutToken ping could be sent after the account logged out.
    logged_ping_type = ProtegoPingType::kWithToken;
  } else if (base::Time::Now() - last_ping_without_token < base::Hours(24)) {
    // If no ping with token was sent but a ping without token was sent within
    // the last 24 hours, the most recent ping type is kWithoutToken.
    // Otherwise, it is the default value, kNone.
    logged_ping_type = ProtegoPingType::kWithoutToken;
  }
  base::UmaHistogramEnumeration(
      "SafeBrowsing.Enhanced.ProtegoRequestSentInLast24Hours",
      sent_ping_since_last_collector_run ? most_recent_ping_type
                                         : ProtegoPingType::kNone);

  base::UmaHistogramEnumeration(
      "SafeBrowsing.Enhanced.ProtegoRequestSentInLast24Hours2",
      logged_ping_type);

  auto logged_ping_last_7_days_type = ProtegoPingType::kNone;
  if (base::Time::Now() - last_ping_with_token < base::Days(7)) {
    // If a ping with token was sent within the last 7 days,
    // the most recent ping type is kWithToken.
    // If both last_ping_with_token and last_ping_without_token are present,
    // we log kWithToken instead of kWithoutToken because if a token has been
    // sent before, we are certain that this account is a signed in account
    // and the server has received the token.
    // The kWithoutToken ping could be sent after the account logged out.
    logged_ping_last_7_days_type = ProtegoPingType::kWithToken;
  } else if (base::Time::Now() - last_ping_without_token < base::Days(7)) {
    // If no ping with token was sent but a ping without token was sent within
    // the last 7 days, the most recent ping type is kWithoutToken.
    // Otherwise, it is the default value, kNone.
    logged_ping_last_7_days_type = ProtegoPingType::kWithoutToken;
  }

  base::UmaHistogramEnumeration(
      "SafeBrowsing.Enhanced.ProtegoRequestSentInLast7Days",
      logged_ping_last_7_days_type);
}

void SafeBrowsingMetricsCollector::ScheduleNextLoggingAfterInterval(
    base::TimeDelta interval) {
  metrics_collector_timer_.Stop();
  metrics_collector_timer_.Start(
      FROM_HERE, interval, this,
      &SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging);
}

void SafeBrowsingMetricsCollector::LogDailyOptInMetrics() {
  base::UmaHistogramEnumeration("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                                GetSafeBrowsingState(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.Extended",
                            IsExtendedReportingEnabled(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.SafeBrowsingModeManaged",
                            IsSafeBrowsingPolicyManaged(*pref_service_));
  base::UmaHistogramBoolean(
      "SafeBrowsing.Pref.Daily.PasswordLeakToggle",
      pref_service_->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled));
}

void SafeBrowsingMetricsCollector::LogDailyEventMetrics() {
  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  if (sb_state == SafeBrowsingState::NO_SAFE_BROWSING) {
    return;
  }
  UserState user_state = GetUserState();

  int total_bypass_count = 0;
  int total_security_sensitive_event_count = 0;
  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (IsBypassEventType(event_type)) {
      int bypass_count = GetEventCountSince(user_state, event_type,
                                            base::Time::Now() - base::Days(28));
      total_bypass_count += bypass_count;
    }
    if (IsSecuritySensitiveEventType(event_type)) {
      int security_sensitive_event_count = GetEventCountSince(
          user_state, event_type, base::Time::Now() - base::Days(28));
      total_security_sensitive_event_count += security_sensitive_event_count;
    }
  }
  base::UmaHistogramCounts100("SafeBrowsing.Daily.BypassCountLast28Days." +
                                  GetUserStateMetricSuffix(user_state) +
                                  ".AllEvents",
                              total_bypass_count);
  base::UmaHistogramCounts100(
      "SafeBrowsing.Daily.SecuritySensitiveCountLast28Days." +
          GetUserStateMetricSuffix(user_state) + ".AllEvents",
      total_security_sensitive_event_count);
}

void SafeBrowsingMetricsCollector::RemoveOldEventsFromPref() {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kSafeBrowsingEventTimestamps);
  base::Value::Dict& mutable_state_dict = update.Get();

  for (auto state_map : mutable_state_dict) {
    for (auto event_map : state_map.second.GetDict()) {
      event_map.second.GetList().EraseIf([&](const auto& timestamp) {
        return base::Time::Now() - PrefValueToTime(timestamp) >
               base::Days(kEventMaxDurationDay);
      });
    }
  }
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventToPref(
    EventType event_type) {
  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  // Skip the event if Safe Browsing is disabled.
  if (sb_state == SafeBrowsingState::NO_SAFE_BROWSING) {
    return;
  }

  AddSafeBrowsingEventAndUserStateToPref(GetUserState(), event_type);
}

void SafeBrowsingMetricsCollector::AddBypassEventToPref(
    ThreatSource threat_source) {
  EventType event;
  switch (threat_source) {
    case ThreatSource::LOCAL_PVER4:
      event = EventType::DATABASE_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::CLIENT_SIDE_DETECTION:
      event = EventType::CSD_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::URL_REAL_TIME_CHECK:
      event = EventType::URL_REAL_TIME_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::NATIVE_PVER5_REAL_TIME:
      event = EventType::HASH_PREFIX_REAL_TIME_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      event = EventType::ANDROID_SAFEBROWSING_REAL_TIME_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::ANDROID_SAFEBROWSING:
      event = EventType::ANDROID_SAFEBROWSING_INTERSTITIAL_BYPASS;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected threat source.";
      event = EventType::DATABASE_INTERSTITIAL_BYPASS;
  }
  AddSafeBrowsingEventToPref(event);
}

std::optional<base::Time> SafeBrowsingMetricsCollector::GetLatestEventTimestamp(
    EventType event_type) {
  return GetLatestEventTimestamp(base::BindRepeating(
      [](const EventType& target_event_type, const EventType& event_type) {
        return target_event_type == event_type;
      },
      event_type));
}

std::optional<base::Time> SafeBrowsingMetricsCollector::GetLatestEventTimestamp(
    EventTypeFilter event_type_filter) {
  // Events are not logged when Safe Browsing is disabled.
  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  if (sb_state == SafeBrowsingState::NO_SAFE_BROWSING) {
    return std::nullopt;
  }

  const std::optional<Event> event =
      GetLatestEventFromEventTypeFilter(GetUserState(), event_type_filter);
  return event ? std::optional<base::Time>(event.value().timestamp)
               : std::nullopt;
}

std::optional<base::Time>
SafeBrowsingMetricsCollector::GetLatestSecuritySensitiveEventTimestamp() {
  return GetLatestEventTimestamp(base::BindRepeating(
      &SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType));
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventAndUserStateToPref(
    UserState user_state,
    EventType event_type) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kSafeBrowsingEventTimestamps);
  base::Value::Dict& mutable_state_dict = update.Get();
  base::Value::Dict* event_dict =
      mutable_state_dict.EnsureDict(UserStateToPrefKey(user_state));
  base::Value::List* timestamps =
      event_dict->EnsureList(EventTypeToPrefKey(event_type));

  // Remove the oldest timestamp if the length of the timestamps hits the limit.
  while (timestamps->size() >= kTimestampsMaxLength) {
    timestamps->erase(timestamps->begin());
  }

  timestamps->Append(TimeToPrefValue(base::Time::Now()));
}

void SafeBrowsingMetricsCollector::OnEnhancedProtectionPrefChanged() {
  // Pref changed by policy is not initiated by users, so this case is ignored.
  if (IsSafeBrowsingPolicyManaged(*pref_service_)) {
    return;
  }

  if (!pref_service_->GetBoolean(prefs::kSafeBrowsingEnhanced)) {
    AddSafeBrowsingEventAndUserStateToPref(UserState::kEnhancedProtection,
                                           EventType::USER_STATE_DISABLED);
    LogEnhancedProtectionDisabledMetrics();
  } else {
    AddSafeBrowsingEventAndUserStateToPref(UserState::kEnhancedProtection,
                                           EventType::USER_STATE_ENABLED);
  }
}

const base::Value::Dict*
SafeBrowsingMetricsCollector::GetSafeBrowsingEventDictionary(
    UserState user_state) {
  const base::Value::Dict& state_dict =
      pref_service_->GetDict(prefs::kSafeBrowsingEventTimestamps);

  return state_dict.FindDict(UserStateToPrefKey(user_state));
}

std::optional<SafeBrowsingMetricsCollector::Event>
SafeBrowsingMetricsCollector::GetLatestEventFromEventType(
    UserState user_state,
    EventType event_type) {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(user_state);

  if (!event_dict) {
    return std::nullopt;
  }

  const base::Value::List* timestamps =
      event_dict->FindList(EventTypeToPrefKey(event_type));

  if (timestamps && timestamps->size() > 0) {
    base::Time time = PrefValueToTime(timestamps->back());
    return Event(event_type, time);
  }

  return std::nullopt;
}

std::optional<SafeBrowsingMetricsCollector::Event>
SafeBrowsingMetricsCollector::GetLatestEventFromEventTypeFilter(
    UserState user_state,
    EventTypeFilter event_type_filter) {
  std::vector<Event> bypass_events;
  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (!event_type_filter.Run(event_type)) {
      continue;
    }
    const std::optional<Event> latest_event =
        GetLatestEventFromEventType(user_state, event_type);
    if (latest_event) {
      bypass_events.emplace_back(latest_event.value());
    }
  }

  const auto latest_event = std::max_element(
      bypass_events.begin(), bypass_events.end(),
      [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });

  return (latest_event != bypass_events.end())
             ? std::optional<Event>(*latest_event)
             : std::nullopt;
}

void SafeBrowsingMetricsCollector::LogEnhancedProtectionDisabledMetrics() {
  base::UmaHistogramCounts100(
      "SafeBrowsing.EsbDisabled.TimesDisabledLast28Days." +
          GetTimesDisabledSuffix(),
      GetEventCountSince(UserState::kEnhancedProtection,
                         EventType::USER_STATE_DISABLED,
                         base::Time::Now() - base::Days(28)));

  int disabled_times_last_week = GetEventCountSince(
      UserState::kEnhancedProtection, EventType::USER_STATE_DISABLED,
      base::Time::Now() - base::Days(7));
  if (disabled_times_last_week <= kEsbDisabledMetricsQuota) {
    LogThrottledEnhancedProtectionDisabledMetrics();
  }
}

void SafeBrowsingMetricsCollector::
    LogThrottledEnhancedProtectionDisabledMetrics() {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(UserState::kEnhancedProtection);
  if (!event_dict) {
    return;
  }

  std::optional<SafeBrowsingMetricsCollector::Event> latest_bypass_event =
      GetLatestEventFromEventTypeFilter(
          UserState::kEnhancedProtection,
          base::BindRepeating(
              &SafeBrowsingMetricsCollector::IsBypassEventType));
  if (latest_bypass_event) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.EsbDisabled.LastBypassEventType",
        latest_bypass_event->type);
  }

  std::optional<SafeBrowsingMetricsCollector::Event>
      latest_security_sensitive_event = GetLatestEventFromEventTypeFilter(
          UserState::kEnhancedProtection,
          base::BindRepeating(
              &SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType));
  if (latest_security_sensitive_event) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.EsbDisabled.LastSecuritySensitiveEventType",
        latest_security_sensitive_event->type);
  }

  const std::optional<Event> latest_enabled_event = GetLatestEventFromEventType(
      UserState::kEnhancedProtection, EventType::USER_STATE_ENABLED);
  if (latest_enabled_event) {
    const auto days_since_enabled =
        (base::Time::Now() - latest_enabled_event.value().timestamp).InDays();
    base::UmaHistogramCounts100("SafeBrowsing.EsbDisabled.LastEnabledInterval",
                                /* sample */ days_since_enabled);
  }
}

int SafeBrowsingMetricsCollector::GetEventCountSince(UserState user_state,
                                                     EventType event_type,
                                                     base::Time since_time) {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(user_state);
  if (!event_dict) {
    return 0;
  }
  const base::Value::List* timestamps =
      event_dict->FindList(EventTypeToPrefKey(event_type));
  if (!timestamps) {
    return 0;
  }

  return base::ranges::count_if(*timestamps, [&](const base::Value& timestamp) {
    return PrefValueToTime(timestamp) > since_time;
  });
}

UserState SafeBrowsingMetricsCollector::GetUserState() {
  if (IsSafeBrowsingPolicyManaged(*pref_service_)) {
    return UserState::kManaged;
  }

  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  switch (sb_state) {
    case SafeBrowsingState::ENHANCED_PROTECTION:
      return UserState::kEnhancedProtection;
    case SafeBrowsingState::STANDARD_PROTECTION:
      return UserState::kStandardProtection;
    case SafeBrowsingState::NO_SAFE_BROWSING:
      NOTREACHED_IN_MIGRATION() << "Unexpected Safe Browsing state.";
      return UserState::kStandardProtection;
  }
}

bool SafeBrowsingMetricsCollector::IsBypassEventType(const EventType& type) {
  switch (type) {
    case EventType::USER_STATE_DISABLED:
    case EventType::USER_STATE_ENABLED:
    case EventType::SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_SSL_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_PASSWORD_PROTECTION:
    case EventType::SECURITY_SENSITIVE_DOWNLOAD:
    case EventType::DOWNLOAD_DEEP_SCAN:
      return false;
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
    case EventType::CSD_INTERSTITIAL_BYPASS:
    case EventType::URL_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
    case EventType::HASH_PREFIX_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::ANDROID_SAFEBROWSING_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::ANDROID_SAFEBROWSING_INTERSTITIAL_BYPASS:
      return true;
  }
}

bool SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType(
    const EventType& type) {
  switch (type) {
    case EventType::USER_STATE_DISABLED:
    case EventType::USER_STATE_ENABLED:
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
    case EventType::CSD_INTERSTITIAL_BYPASS:
    case EventType::URL_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
    case EventType::HASH_PREFIX_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::ANDROID_SAFEBROWSING_REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::ANDROID_SAFEBROWSING_INTERSTITIAL_BYPASS:
      return false;
    case EventType::SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_SSL_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_PASSWORD_PROTECTION:
    case EventType::SECURITY_SENSITIVE_DOWNLOAD:
    case EventType::DOWNLOAD_DEEP_SCAN:
      return true;
  }
}

std::string SafeBrowsingMetricsCollector::GetUserStateMetricSuffix(
    const UserState& user_state) {
  switch (user_state) {
    case UserState::kStandardProtection:
      return "StandardProtection";
    case UserState::kEnhancedProtection:
      return "EnhancedProtection";
    case UserState::kManaged:
      return "Managed";
  }
}

std::string SafeBrowsingMetricsCollector::GetTimesDisabledSuffix() {
  const std::optional<Event> latest_enabled_event = GetLatestEventFromEventType(
      UserState::kEnhancedProtection, EventType::USER_STATE_ENABLED);

  if (!latest_enabled_event) {
    // This code path could be possible if ESB was enabled via policy but
    // later disabled by the user, since policy enables/disables are not
    // tracked. It's also possible if it's been longer than kEventMaxDurationDay
    // days since the latest enabled event.
    return "NeverEnabled";
  }
  const auto hours_since_enabled =
      (base::Time::Now() - latest_enabled_event.value().timestamp).InHours();
  return hours_since_enabled < kEsbShortEnabledUpperBoundHours
             ? "ShortEnabled"
             : hours_since_enabled < kEsbLongEnabledLowerBoundHours
                   ? "MediumEnabled"
                   : "LongEnabled";
}

SafeBrowsingMetricsCollector::Event::Event(EventType type, base::Time timestamp)
    : type(type), timestamp(timestamp) {}

}  // namespace safe_browsing
