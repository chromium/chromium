// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing preferences and some basic utility functions for using them.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_PREFS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_PREFS_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/prefs/pref_member.h"
#include "components/safe_browsing/core/common/features.h"

class PrefRegistrySimple;
class PrefService;
class GURL;

namespace base {
class Time;
}

namespace prefs {
// A list of times at which CSD pings were sent.
inline constexpr char kSafeBrowsingCsdPingTimestamps[] =
    "safebrowsing.csd_ping_timestamps";

// Boolean that is true when deep scanning is allowed.
inline constexpr char kSafeBrowsingDeepScanningEnabled[] =
    "safebrowsing.deep_scanning_enabled";

// Boolean that is true when SafeBrowsing is enabled.
inline constexpr char kSafeBrowsingEnabled[] = "safebrowsing.enabled";

// Boolean that is true when Safe Browsing Enhanced Protection is enabled.
inline constexpr char kSafeBrowsingEnhanced[] = "safebrowsing.enhanced";

// Timestamp indicating the last time a protego ping with a token was sent.
// This is only set if the user has enhanced protection enabled and is signed
// in with their account.
inline constexpr char kSafeBrowsingEsbProtegoPingWithTokenLastLogTime[] =
    "safebrowsing.esb_protego_ping_with_token_last_log_time";

// Timestamp indicating the last time a protego ping without a token was sent.
// This is only set if the user has enhanced protection enabled and is not
// signed in with their account.
inline constexpr char kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime[] =
    "safebrowsing.esb_protego_ping_without_token_last_log_time";

// Boolean that tells us whether users are given the option to opt in to
// Safe Browsing extended reporting. This is exposed as a preference that
// can be overridden by enterprise policy.
inline constexpr char kSafeBrowsingExtendedReportingOptInAllowed[] =
    "safebrowsing.extended_reporting_opt_in_allowed";

// A dictionary mapping incident types to a dict of incident key:digest pairs.
// The key is a string: a filename or pref name. Digests are 4 bytes. This pref
// is only set/updated if Chrome (Windows only) notices certain security
// incidents, e.g. the user downloaded binaries with invalid signatures.
inline constexpr char kSafeBrowsingIncidentsSent[] =
    "safebrowsing.incidents_sent";

// Boolean that is true when the SafeBrowsing interstitial should not allow
// users to proceed anyway.
inline constexpr char kSafeBrowsingProceedAnywayDisabled[] =
    "safebrowsing.proceed_anyway_disabled";

// Boolean indicating whether the user has ever seen a security interstitial.
inline constexpr char kSafeBrowsingSawInterstitialScoutReporting[] =
    "safebrowsing.saw_interstitial_sber2";

// Boolean indicating whether Safe Browsing Scout reporting is enabled, which
// collects data for malware detection.
inline constexpr char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";

// Boolean indicating whether Safe Browsing Scout reporting was enabled at the
// time that extended reporting was deprecated.
inline constexpr char kSafeBrowsingScoutReportingEnabledWhenDeprecated[] =
    "safebrowsing.scout_reporting_enabled_when_deprecated";

// Dictionary containing safe browsing triggers and the list of times they have
// fired recently. The keys are TriggerTypes (4-byte ints) and the values are
// lists of doubles.
inline constexpr char kSafeBrowsingTriggerEventTimestamps[] =
    "safebrowsing.trigger_event_timestamps";

// Dictionary that records the origin and navigation ID pairs of unhandled gaia
// password reuses. The keys are origin strings and the ID values are 8-byte
// ints. Only set/update if a Chrome user reuses their Gaia password on a
// phishing site.
inline constexpr char kSafeBrowsingUnhandledGaiaPasswordReuses[] =
    "safebrowsing.unhandled_sync_password_reuses";

// Integer timestamp of next time the PasswordCaptured event should be logged.
inline constexpr char kSafeBrowsingNextPasswordCaptureEventLogTime[] =
    "safebrowsing.next_password_capture_event_log_time";

// List of domains where Safe Browsing should trust. That means Safe Browsing
// won't check for malware/phishing/Uws on resources on these domains, or
// trigger warnings. Used for enterprise only.
inline constexpr char kSafeBrowsingAllowlistDomains[] =
    "safebrowsing.safe_browsing_whitelist_domains";

// String indicating the URL where password protection service should send user
// to change their password if they've been phished. Password protection service
// also captures new password on this page in a change password event. Used for
// enterprise only.
inline constexpr char kPasswordProtectionChangePasswordURL[] =
    "safebrowsing.password_protection_change_password_url";

// List of string indicating the URL(s) users use to log in. Password protection
// service will capture passwords on these URLs.
// This is managed by enterprise policy and has no effect on users who are not
// managed by enterprise policy.
inline constexpr char kPasswordProtectionLoginURLs[] =
    "safebrowsing.password_protection_login_urls";

// Integer indicating the password protection warning trigger. This is managed
// by enterprise policy and has no effect on users who are not managed by
// enterprise policy.
inline constexpr char kPasswordProtectionWarningTrigger[] =
    "safebrowsing.password_protection_warning_trigger";

// Last time Chrome refreshes advanced protection status for sign-in users (in
// microseconds);
inline constexpr char kAdvancedProtectionLastRefreshInUs[] =
    "safebrowsing.advanced_protection_last_refresh";

// Boolean that indicates if Chrome is allowed to provide extra
// features to users enrolled in the Advanced Protection Program.
inline constexpr char kAdvancedProtectionAllowed[] =
    "safebrowsing.advanced_protection_allowed";

// Integer epoch timestamp in seconds. Indicates the last logging time of Safe
// Browsing metrics.
inline constexpr char kSafeBrowsingMetricsLastLogTime[] =
    "safebrowsing.metrics_last_log_time";

// A dictionary of Safe Browsing events and their corresponding timestamps.
// Used for logging metrics. Structure: go/sb-event-ts-pref-struct.
inline constexpr char kSafeBrowsingEventTimestamps[] =
    "safebrowsing.event_timestamps";

// A timestamp indicating the expiration time of the Oblivious HTTP key used by
// hash prefix real time URL check.
inline constexpr char kSafeBrowsingHashRealTimeOhttpExpirationTime[] =
    "safebrowsing.hash_real_time_ohttp_expiration_time";

// The Oblivious HTTP key used by hash prefix real time URL check.
inline constexpr char kSafeBrowsingHashRealTimeOhttpKey[] =
    "safebrowsing.hash_real_time_ohttp_key";

// Boolean indicating whether users can receive surveys.
inline constexpr char kSafeBrowsingSurveysEnabled[] =
    "safebrowsing.surveys_enabled";

// A timestamp indicating the last time the account tailored security boolean
// was updated.
inline constexpr char kAccountTailoredSecurityUpdateTimestamp[] =
    "safebrowsing.aesb_update_time_windows_epoch_micros";

// Timestamp indicating when the next time the sync flow retry can happen is.
// This value is managed by the ChromeTailoredSecurityService.
inline constexpr char kTailoredSecurityNextSyncFlowTimestamp[] =
    "safebrowsing.aesb_next_sync_flow_timestamp";

// Timestamp indicating the last time the tailored security sync flow ran.
inline constexpr char kTailoredSecuritySyncFlowLastRunTime[] =
    "safebrowsing.aesb_sync_flow_start_timestamp";

// Integer that maps to TailoredSecurityUserInteractionState. Indicates the
// last known state of the tailored security sync flow.
// TODO(crbug.com/40925236): remove this preference value.
inline constexpr char kTailoredSecuritySyncFlowLastUserInteractionState[] =
    "safebrowsing.aesb_sync_flow_last_user_interaction_state";

// Integer that maps to TailoredSecurityRetryState. Indicates the last
// known state of the tailored security sync flow retry mechanism.
inline constexpr char kTailoredSecuritySyncFlowRetryState[] =
    "safebrowsing.aesb_sync_flow_retry_state";

// Timestamp indicating when the last user interaction state was observed as
// having the value of `UNSET`. It is possible that this value will never be
// set. This will only be set for syncing users where the retry detection logic
// ran and no outcome was set -- indicating that tailored security with retry
// capabilities had never run.
inline constexpr char kTailoredSecuritySyncFlowObservedOutcomeUnsetTimestamp[] =
    "safebrowsing.aesb_sync_flow_observed_outcome_unset_timestamp";

// Whether the user was shown the notification that they may want to enable
// Enhanced Safe Browsing due to their account tailored security state.
// This value is only relevant to the tailored security flow for non-syncing
// users.
inline constexpr char kAccountTailoredSecurityShownNotification[] =
    "safebrowsing.aesb_shown_notification";

// A boolean indicating if Enhanced Protection was enabled in sync with
// account tailored security.
inline constexpr char kEnhancedProtectionEnabledViaTailoredSecurity[] =
    "safebrowsing.esb_enabled_via_tailored_security";

// The last time the Extension Telemetry Service successfully
// uploaded its data.
inline constexpr char kExtensionTelemetryLastUploadTime[] =
    "safebrowsing.extension_telemetry_last_upload_time";

// The saved copy of the current configuration that will be used by
// the Extension Telemetry Service.
inline constexpr char kExtensionTelemetryConfig[] =
    "safebrowsing.extension_telemetry_configuration";

// A dictionary of extension ids and their file data from the
// Telemetry Service's file processor.
inline constexpr char kExtensionTelemetryFileData[] =
    "safebrowsing.extension_telemetry_file_data";

// A boolean indicating if hash-prefix real-time lookups are allowed by policy.
// If false, the lookups will instead be hash-prefix database lookups. If true,
// there is no such override; the hash-prefix real-time lookups might still not
// occur for unrelated reasons.
inline constexpr char kHashPrefixRealTimeChecksAllowedByPolicy[] =
    "safebrowsing.hash_prefix_real_time_checks_allowed_by_policy";

// A preference indicating that the user has seen the IPH telling them automatic
// deep scans are coming. Since IPH may be delayed for a variety of reasons
// (startup grace periods, other IPH in the session), we want to wait to enable
// automatic deep scans until they've actually seen the IPH.
inline constexpr char kSafeBrowsingAutomaticDeepScanningIPHSeen[] =
    "safebrowsing.automatic_deep_scanning_iph_seen";

// A preference indicating that the user has already done an automatic
// deep scan. This addresses an edge case where deep scan notices remain
// in the bubble after the user performs an automatic deep scan.
inline constexpr char kSafeBrowsingAutomaticDeepScanPerformed[] =
    "safe_browsing.automatic_deep_scan_performed";

}  // namespace prefs

namespace safe_browsing {

// Enumerates the level of Safe Browsing Extended Reporting that is currently
// available.
enum class ExtendedReportingLevel {
  // Extended reporting is off.
  SBER_LEVEL_OFF = 0,
  // The Legacy level of extended reporting is available, reporting happens in
  // response to security incidents.
  SBER_LEVEL_LEGACY = 1,
  // The Scout level of extended reporting is available, some data can be
  // collected to actively detect dangerous apps and sites.
  SBER_LEVEL_SCOUT = 2,
  // The Scout level of extended reporting is deprecated, however, the user has
  // the ESB setting on.
  SBER_LEVEL_ENHANCED_PROTECTION = 3,
};

// Enumerates the states used for determining whether the Tailored Security flow
// needs to be retried.
enum TailoredSecurityRetryState {
  // Initialization value meaning that the tailored security feature has not
  // touched this value.
  UNSET = 0,
  // The flow started but has not completed yet. Note that the flow may never
  // complete because Chrome can exit before the logic is able to record a
  // different value. RUNNING was not selected as the name for this state
  // because the tailored security flow may or may not be running when this
  // state is observed.
  UNKNOWN = 1,
  // Retry is needed. This could be because the notification flow failed.
  RETRY_NEEDED = 2,
  // No retry is needed. This could be because either the notification was shown
  // to the user or the flow found a state that a notification is not shown for,
  // for example: if the account is controlled by a policy.
  NO_RETRY_NEEDED = 3
};

// Enumerates all the places where the Safe Browsing Extended Reporting
// preference can be changed.
// These values are written to logs.  New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
enum ExtendedReportingOptInLocation {
  // The chrome://settings UI.
  SBER_OPTIN_SITE_CHROME_SETTINGS = 0,
  // The Android settings UI.
  SBER_OPTIN_SITE_ANDROID_SETTINGS = 1,
  // The Download Feedback popup.
  SBER_OPTIN_SITE_DOWNLOAD_FEEDBACK_POPUP = 2,
  // Any security interstitial (malware, SSL, etc).
  SBER_OPTIN_SITE_SECURITY_INTERSTITIAL = 3,
  // New sites must be added before SBER_OPTIN_SITE_MAX.
  SBER_OPTIN_SITE_MAX
};

// Enumerates all the triggers of password protection.
enum PasswordProtectionTrigger {
  // Password protection is off.
  PASSWORD_PROTECTION_OFF = 0,
  // Password protection triggered by password reuse event.
  PASSWORD_REUSE = 1,
  // Password protection triggered by password reuse event on phishing page.
  PHISHING_REUSE = 2,
  // New triggers must be added before PASSWORD_PROTECTION_TRIGGER_MAX.
  PASSWORD_PROTECTION_TRIGGER_MAX,
};

// Enum representing possible values of the Safe Browsing state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.safe_browsing
enum class SafeBrowsingState {
  // The user is not opted into Safe Browsing.
  NO_SAFE_BROWSING = 0,
  // The user selected standard protection.
  STANDARD_PROTECTION = 1,
  // The user selected enhanced protection.
  ENHANCED_PROTECTION = 2,

  kMaxValue = ENHANCED_PROTECTION,
};

SafeBrowsingState GetSafeBrowsingState(const PrefService& prefs);

// Set the SafeBrowsing prefs. Also records if ESB was enabled in sync with
// Account-ESB via Tailored Security.
void SetSafeBrowsingState(PrefService* prefs,
                          SafeBrowsingState state,
                          bool is_esb_enabled_in_sync = false);

// Returns whether Safe Browsing is enabled for the user.
bool IsSafeBrowsingEnabled(const PrefService& prefs);

// Returns whether Safe Browsing enhanced protection is enabled for the user.
bool IsEnhancedProtectionEnabled(const PrefService& prefs);

// Returns the level of reporting available for the current user.
ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs);

// Returns whether the user is able to modify the Safe Browsing Extended
// Reporting opt-in.
bool IsExtendedReportingOptInAllowed(const PrefService& prefs);

// Returns whether Safe Browsing Extended Reporting is currently enabled.
// This should be used to decide if any of the reporting preferences are set,
// regardless of which specific one is set.
bool IsExtendedReportingEnabled(const PrefService& prefs);

// Returns whether Safe Browsing Extended Reporting is currently enabled.
// This function does not check the Safe Browsing Extended Reporting deprecation
// flag, kExtendedReportingRemovePrefDependency, so that the ping manager will
// keep sending CSBRR pings.
// TODO(crbug.com/336547987): Remove this temporary function when the mitigation
// is implemented and the deprecation flag is removed.
bool IsExtendedReportingEnabledBypassDeprecationFlag(const PrefService& prefs);

// Returns whether the active Extended Reporting pref is currently managed by
// enterprise policy, meaning the user can't change it.
bool IsExtendedReportingPolicyManaged(const PrefService& prefs);

// Return whether the Safe Browsing preference is managed. It can be managed by
// either the SafeBrowsingEnabled policy(legacy) or the
// SafeBrowsingProtectionLevel policy(new).
bool IsSafeBrowsingPolicyManaged(const PrefService& prefs);

// Return whether the Safe Browsing preference is controlled by an extension.
bool IsSafeBrowsingExtensionControlled(const PrefService& prefs);

// Returns whether a user can receive HaTS surveys.
bool IsSafeBrowsingSurveysEnabled(const PrefService& prefs);

// Returns whether a user can bypass a warning.
bool IsSafeBrowsingProceedAnywayDisabled(const PrefService& prefs);

// Returns whether hash-prefix real-time lookups are allowed for the user based
// on enterprise policy.
bool AreHashPrefixRealTimeLookupsAllowedByPolicy(const PrefService& prefs);

// Returns whether deep scanning is allowed based on enterprise policy.
bool AreDeepScansAllowedByPolicy(const PrefService& prefs);

// Updates UMA metrics about Safe Browsing Extended Reporting states.
void RecordExtendedReportingMetrics(const PrefService& prefs);

// Registers user preferences related to Safe Browsing.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Registers local state prefs related to Safe Browsing.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Sets the currently active Safe Browsing Extended Reporting preference to the
// specified value. The |location| indicates the UI where the change was
// made.
void SetExtendedReportingPrefAndMetric(PrefService* prefs,
                                       bool value,
                                       ExtendedReportingOptInLocation location);

// This variant is used to simplify test code by omitting the location.
void SetExtendedReportingPrefForTests(PrefService* prefs, bool value);

// Set the current configuration being used by the Extension Telemetry Service
void SetExtensionTelemetryConfig(PrefService& prefs,
                                 const base::Value::Dict& config);

// Get the current configuration being used by the Extension Telemetry Service
const base::Value::Dict& GetExtensionTelemetryConfig(const PrefService& prefs);

// Get the current processed file data stored in the Extension Telemetry
// Service.
const base::Value::Dict& GetExtensionTelemetryFileData(
    const PrefService& prefs);

// Sets the last time the Extension Telemetry Service successfully uploaded
// its data.
void SetLastUploadTimeForExtensionTelemetry(PrefService& prefs,
                                            const base::Time& time);

// Returns the `kExtensionTelemetryLastUploadTime` user preference.
base::Time GetLastUploadTimeForExtensionTelemetry(PrefService& prefs);

// Sets the currently active Safe Browsing Enhanced Protection to the specified
// value.
void SetEnhancedProtectionPrefForTests(PrefService* prefs, bool value);

// Set prefs to enable Safe Browsing Enhanced Protection.
void SetEnhancedProtectionPref(PrefService* prefs, bool value);

// Set prefs to enable Safe Browsing Standard Protection.
void SetStandardProtectionPref(PrefService* prefs, bool value);

// Called to indicate that a security interstitial is about to be shown to the
// user. This may trigger the user to begin seeing the Scout opt-in text
// depending on their experiment state.
void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs);

// Returns a list of preferences to be shown in chrome://safe-browsing. The
// preferences are passed as an alternating sequence of preference names and
// values represented as strings.
base::Value::List GetSafeBrowsingPreferencesList(PrefService* prefs);

// Returns a list of policies to be shown in chrome://safe-browsing. The
// policies are passed as an alternating sequence of policy names and
// values represented as strings.
base::Value::List GetSafeBrowsingPoliciesList(PrefService* prefs);

// Returns a list of valid domains that Safe Browsing service trusts.
void GetSafeBrowsingAllowlistDomainsPref(
    const PrefService& prefs,
    std::vector<std::string>* out_canonicalized_domain_list);

// Helper function to validate and canonicalize a list of domain strings.
void CanonicalizeDomainList(
    const base::Value::List& raw_domain_list,
    std::vector<std::string>* out_canonicalized_domain_list);

// Helper function to determine if |url| matches Safe Browsing allowlist domains
// (a.k. a prefs::kSafeBrowsingAllowlistDomains).
bool IsURLAllowlistedByPolicy(const GURL& url, const PrefService& pref);

// Helper function to get a list of Safe Browsing allowlist domains
// (a.k. a prefs::kSafeBrowsingAllowlistDomains).
// Called on UI thread.
std::vector<std::string> GetURLAllowlistByPolicy(PrefService* pref_service);

// Helper function to determine if any entry on the |url_chain| matches Safe
// Browsing allowlist domains.
// Called on UI thread.
bool MatchesEnterpriseAllowlist(const PrefService& pref,
                                const std::vector<GURL>& url_chain);

// Helper function to get the pref value of password protection login URLs.
void GetPasswordProtectionLoginURLsPref(const PrefService& prefs,
                                        std::vector<GURL>* out_login_url_list);

// Helper function that returns true if |url| matches any password protection
// login URLs. Returns false otherwise.
bool MatchesPasswordProtectionLoginURL(const GURL& url,
                                       const PrefService& prefs);

// Helper function to get the pref value of password protection change password
// URL.
GURL GetPasswordProtectionChangePasswordURLPref(const PrefService& prefs);

// Helper function that returns true if |url| matches password protection
// change password URL. Returns false otherwise.
bool MatchesPasswordProtectionChangePasswordURL(const GURL& url,
                                                const PrefService& prefs);

// Helper function to match a |target_url| against |url_list|.
bool MatchesURLList(const GURL& target_url, const std::vector<GURL>& url_list);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_PREFS_H_
