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

class PrefRegistrySimple;
class PrefService;
class GURL;

namespace base {
class Time;
}

namespace prefs {
// A list of times at which CSD pings were sent.
extern const char kSafeBrowsingCsdPingTimestamps[];

// Boolean that is true when SafeBrowsing is enabled.
extern const char kSafeBrowsingEnabled[];

// Boolean that is true when Safe Browsing Enhanced Protection is enabled.
extern const char kSafeBrowsingEnhanced[];

// Integer indicating the state of real time URL check. This is managed
// by enterprise policy and has no effect on users who are not managed by
// enterprise policy.
extern const char kSafeBrowsingEnterpriseRealTimeUrlCheckMode[];

// Integer indicating the scope at which the
// kSafeBrowsingEnterpriseRealTimeUrlCheckMode pref is set.
extern const char kSafeBrowsingEnterpriseRealTimeUrlCheckScope[];

// Boolean that tells us whether users are given the option to opt in to Safe
// Browsing extended reporting. This is exposed as a preference that can be
// overridden by enterprise policy.
extern const char kSafeBrowsingExtendedReportingOptInAllowed[];

// A dictionary mapping incident types to a dict of incident key:digest pairs.
// The key is a string: a filename or pref name. Digests are 4 bytes. This pref
// is only set/updated if Chrome (Windows only) notices certain security
// incidents, e.g. the user downloaded binaries with invalid signatures.
extern const char kSafeBrowsingIncidentsSent[];

// Boolean that is true when the SafeBrowsing interstitial should not allow
// users to proceed anyway.
extern const char kSafeBrowsingProceedAnywayDisabled[];

// Boolean indicating whether the user has ever seen a security interstitial.
extern const char kSafeBrowsingSawInterstitialScoutReporting[];

// Boolean indicating whether Safe Browsing Scout reporting is enabled, which
// collects data for malware detection.
extern const char kSafeBrowsingScoutReportingEnabled[];

// Dictionary containing safe browsing triggers and the list of times they have
// fired recently. The keys are TriggerTypes (4-byte ints) and the values are
// lists of doubles.
extern const char kSafeBrowsingTriggerEventTimestamps[];

// Dictionary that records the origin and navigation ID pairs of unhandled gaia
// password reuses. The keys are origin strings and the ID values are 8-byte
// ints. Only set/update if a Chrome user reuses their Gaia password on a
// phishing site.
extern const char kSafeBrowsingUnhandledGaiaPasswordReuses[];

// Integer timestamp of next time the PasswordCaptured event should be logged.
extern const char kSafeBrowsingNextPasswordCaptureEventLogTime[];

// List of domains where Safe Browsing should trust. That means Safe Browsing
// won't check for malware/phishing/Uws on resources on these domains, or
// trigger warnings. Used for enterprise only.
extern const char kSafeBrowsingAllowlistDomains[];

// String indicating the URL where password protection service should send user
// to change their password if they've been phished. Password protection service
// also captures new password on this page in a change password event. Used for
// enterprise only.
extern const char kPasswordProtectionChangePasswordURL[];

// List of string indicating the URL(s) users use to log in. Password protection
// service will capture passwords on these URLs.
// This is managed by enterprise policy and has no effect on users who are not
// managed by enterprise policy.
extern const char kPasswordProtectionLoginURLs[];

// Integer indicating the password protection warning trigger. This is managed
// by enterprise policy and has no effect on users who are not managed by
// enterprise policy.
extern const char kPasswordProtectionWarningTrigger[];

// Last time Chrome refreshes advanced protection status for sign-in users (in
// microseconds);
extern const char kAdvancedProtectionLastRefreshInUs[];

// Boolean that indicates if Chrome is allowed to provide extra
// features to users enrolled in the Advanced Protection Program.
extern const char kAdvancedProtectionAllowed[];

// Integer epoch timestamp in seconds. Indicates the last logging time of Safe
// Browsing metrics.
extern const char kSafeBrowsingMetricsLastLogTime[];

// A dictionary of Safe Browsing events and their corresponding timestamps.
// Used for logging metrics. Structure: go/sb-event-ts-pref-struct.
extern const char kSafeBrowsingEventTimestamps[];

// A timestamp indicating the last time the account tailored security boolean
// was updated.
extern const char kAccountTailoredSecurityUpdateTimestamp[];

// Whether the user was shown the notification that they may want to enable
// Enhanced Safe Browsing due to their account tailored security state.
extern const char kAccountTailoredSecurityShownNotification[];

// A boolean indicating if Enhanced Protection was enabled in sync with
// account tailored security.
extern const char kEnhancedProtectionEnabledViaTailoredSecurity[];

// The last time the Extension Telemetry Service successfully
// uploaded its data.
extern const char kExtensionTelemetryLastUploadTime[];

// The saved copy of the current configuration that will be used by
// the Extension Telemetry Service.
extern const char kExtensionTelemetryConfig[];
}  // namespace prefs

namespace safe_browsing {

// Enumerates the level of Safe Browsing Extended Reporting that is currently
// available.
enum ExtendedReportingLevel {
  // Extended reporting is off.
  SBER_LEVEL_OFF = 0,
  // The Legacy level of extended reporting is available, reporting happens in
  // response to security incidents.
  SBER_LEVEL_LEGACY = 1,
  // The Scout level of extended reporting is available, some data can be
  // collected to actively detect dangerous apps and sites.
  SBER_LEVEL_SCOUT = 2,
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

enum EnterpriseRealTimeUrlCheckMode {
  REAL_TIME_CHECK_DISABLED = 0,
  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED = 1,
};

SafeBrowsingState GetSafeBrowsingState(const PrefService& prefs);

// Set the SafeBrowsing prefs. Also records if ESB was enabled in sync with
// Account-ESB via Tailored Security.
void SetSafeBrowsingState(PrefService* prefs,
                          SafeBrowsingState state,
                          bool is_esb_enabled_in_sync = false);

// Returns whether Safe Browsing is enabled for the user.
bool IsSafeBrowsingEnabled(const PrefService& prefs);

// Returns whether Safe Browsing Standard Protection is enabled for the user.
bool IsStandardProtectionEnabled(const PrefService& prefs);

// Returns whether Safe Browsing enhanced protection is enabled for the user.
bool IsEnhancedProtectionEnabled(const PrefService& prefs);

// Returns whether the currently active Safe Browsing Extended Reporting
// preference exists (eg: has been set before).
bool ExtendedReportingPrefExists(const PrefService& prefs);

// Returns the level of reporting available for the current user.
ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs);

// Returns whether the user is able to modify the Safe Browsing Extended
// Reporting opt-in.
bool IsExtendedReportingOptInAllowed(const PrefService& prefs);

// Returns whether Safe Browsing Extended Reporting is currently enabled.
// This should be used to decide if any of the reporting preferences are set,
// regardless of which specific one is set.
bool IsExtendedReportingEnabled(const PrefService& prefs);

// Returns whether the active Extended Reporting pref is currently managed by
// enterprise policy, meaning the user can't change it.
bool IsExtendedReportingPolicyManaged(const PrefService& prefs);

// Return whether the Safe Browsing preference is managed. It can be managed by
// either the SafeBrowsingEnabled policy(legacy) or the
// SafeBrowsingProtectionLevel policy(new).
bool IsSafeBrowsingPolicyManaged(const PrefService& prefs);

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

// Called when a security interstitial is closed by the user.
// |on_show_pref_existed| indicates whether the pref existed when the
// interstitial was shown. |on_show_pref_value| contains the pref value when the
// interstitial was shown.
void UpdateMetricsAfterSecurityInterstitial(const PrefService& prefs,
                                            bool on_show_pref_existed,
                                            bool on_show_pref_value);

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
bool MatchesURLList(const GURL& target_url, const std::vector<GURL> url_list);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_PREFS_H_
