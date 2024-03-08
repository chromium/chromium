// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_METRICS_COLLECTOR_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_METRICS_COLLECTOR_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"

class PrefService;

namespace safe_browsing {

// This class is for logging Safe Browsing metrics regularly. Metrics are logged
// everyday or at startup, if the last logging time was more than a day ago.
// It is also responsible for adding Safe Browsing events in prefs and logging
// metrics when enhanced protection is disabled.
class SafeBrowsingMetricsCollector : public KeyedService {
 public:
  // Enum representing different types of Safe Browsing events, such as those
  // for measuring user friction, or security sensitive actions. They are used
  // as keys of the SafeBrowsingEventTimestamps pref. They are used for logging
  // histograms, entries must not be removed or reordered. Please update the
  // enums.xml file if new values are added.
  enum EventType {
    // The user state is disabled.
    USER_STATE_DISABLED = 0,
    // The user state is enabled.
    USER_STATE_ENABLED = 1,
    // The user bypasses the interstitial that is triggered by the Safe Browsing
    // database.
    DATABASE_INTERSTITIAL_BYPASS = 2,
    // The user bypasses the interstitial that is triggered by client-side
    // detection.
    CSD_INTERSTITIAL_BYPASS = 3,
    // The user bypasses the interstitial that is triggered by real time URL
    // check.
    URL_REAL_TIME_INTERSTITIAL_BYPASS = 4,
    // The user bypasses the dangerous download warning based on server
    // verdicts.
    DANGEROUS_DOWNLOAD_BYPASS = 5,
    // The user bypasses the password reuse modal warning.
    PASSWORD_REUSE_MODAL_BYPASS = 6,
    // The user accepts the extension install friction dialog (but does not
    // necessarily install the extension).
    // This dialog is only shown to ESB users. Added in M91.
    EXTENSION_ALLOWLIST_INSTALL_BYPASS = 7,
    // The user acknowledges and re-enables the extension that is not on the
    // allowlist.
    // This is only shown to ESB users. Added in M91.
    NON_ALLOWLISTED_EXTENSION_RE_ENABLED = 8,
    // User committed a security sensitive action, and was shown a Safe Browsing
    // interstitial.
    SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL = 9,
    // User committed a security sensitive action, and was shown a SSL
    // interstitial.
    SECURITY_SENSITIVE_SSL_INTERSTITIAL = 10,
    // User received a non-safe verdict from phishguard, because of
    // an on focus ping or a password reuse ping.
    SECURITY_SENSITIVE_PASSWORD_PROTECTION = 11,
    // User committed a security sensitive action related to downloads, as
    // checked by Safe Browsing.
    SECURITY_SENSITIVE_DOWNLOAD = 12,
    // The user bypasses an interstitial that is triggered by the hash-prefix
    // real-time lookup.
    HASH_PREFIX_REAL_TIME_INTERSTITIAL_BYPASS = 13,
    // The user bypasses an interstitial that is triggered by the hash-prefix
    // real-time lookup through Android Safe Browsing API.
    ANDROID_SAFEBROWSING_REAL_TIME_INTERSTITIAL_BYPASS = 14,
    // The user bypasses an interstitial that is triggered by the local Safe
    // Browsing database through Android Safe Browsing API.
    ANDROID_SAFEBROWSING_INTERSTITIAL_BYPASS = 15,
    // The user started a download deep scan
    DOWNLOAD_DEEP_SCAN = 16,

    kMaxValue = DOWNLOAD_DEEP_SCAN
  };

  using EventTypeFilter = base::RepeatingCallback<bool(const EventType&)>;

  // Enum representing the current user state. They are used as keys of the
  // SafeBrowsingEventTimestamps pref, entries must not be removed or reordered.
  // They are also used to construct suffixes of histograms. Please update the
  // MetricsCollectorUserState variants in the histograms.xml file if new values
  // are added.
  enum class UserState {
    // Standard protection is enabled.
    kStandardProtection = 0,
    // Enhanced protection is enabled.
    kEnhancedProtection = 1,
    // Safe Browsing is managed.
    kManaged = 2
  };

  struct Event {
    Event(EventType type, base::Time timestamp);
    EventType type;
    base::Time timestamp;
  };

  explicit SafeBrowsingMetricsCollector(PrefService* pref_service_);

  SafeBrowsingMetricsCollector(const SafeBrowsingMetricsCollector&) = delete;
  SafeBrowsingMetricsCollector& operator=(const SafeBrowsingMetricsCollector&) =
      delete;

  ~SafeBrowsingMetricsCollector() override = default;

  // Checks the last logging time. If the time is longer than a day ago, log
  // immediately. Otherwise, schedule the next logging with delay.
  void StartLogging();

  // Adds |event_type| and the current timestamp to pref.
  void AddSafeBrowsingEventToPref(EventType event_type);

  // Uses |threat_source| to choose which EventType should be passed into
  // AddSafeBrowsingEventToPref
  void AddBypassEventToPref(ThreatSource threat_source);

  // Gets the latest event timestamp of the |event_type|. Returns nullopt if
  // the |event_type| didn't happen in the past.
  std::optional<base::Time> GetLatestEventTimestamp(EventType event_type);

  // Gets the latest event timestamp for security sensitive events. Returns
  // nullopt if a security sensitive event didn't happen in the past.
  virtual std::optional<base::Time> GetLatestSecuritySensitiveEventTimestamp();

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 private:
  friend class SafeBrowsingMetricsCollectorTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingMetricsCollectorTest, GetUserState);

  // The type of Protego ping that was sent by an enhanced protection
  // user. These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class ProtegoPingType {
    kUnknownType = 0,
    kNone = 1,
    kWithToken = 2,
    kWithoutToken = 3,
    kMaxValue = kWithoutToken,
  };

  static bool IsBypassEventType(const EventType& type);
  static bool IsSecuritySensitiveEventType(const EventType& type);
  static std::string GetUserStateMetricSuffix(const UserState& user_state);

  // For daily metrics.
  void LogMetricsAndScheduleNextLogging();
  void MaybeLogDailyEsbProtegoPingSent();
  void ScheduleNextLoggingAfterInterval(base::TimeDelta interval);
  void LogDailyOptInMetrics();
  void LogDailyEventMetrics();
  void RemoveOldEventsFromPref();

  // For pref listeners.
  void OnEnhancedProtectionPrefChanged();
  void LogEnhancedProtectionDisabledMetrics();
  void LogThrottledEnhancedProtectionDisabledMetrics();

  // Helper functions for Safe Browsing events in pref.
  void AddSafeBrowsingEventAndUserStateToPref(UserState user_state,
                                              EventType event_type);
  // Keep the possible returned values of GetTimesDisabledSuffix in sync with
  // MetricsCollectorTimesDisabledEnabledDuration in histograms.xml.
  std::string GetTimesDisabledSuffix();

  // Gets the latest event timestamp for events filtered by |event_type_filter|.
  // Returns nullopt if none of the events happened in the past.
  std::optional<base::Time> GetLatestEventTimestamp(
      EventTypeFilter event_type_filter);
  std::optional<SafeBrowsingMetricsCollector::Event>
  GetLatestEventFromEventType(UserState user_state, EventType event_type);
  std::optional<SafeBrowsingMetricsCollector::Event>
  GetLatestEventFromEventTypeFilter(UserState user_state,
                                    EventTypeFilter event_type_filter);
  const base::Value::Dict* GetSafeBrowsingEventDictionary(UserState user_state);
  int GetEventCountSince(UserState user_state,
                         EventType event_type,
                         base::Time since_time);
  UserState GetUserState();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::OneShotTimer metrics_collector_timer_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_METRICS_COLLECTOR_H_
