// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// Update the correct UMA metric based on which pref was changed and which UI
// the change was made on.
void RecordExtendedReportingPrefChanged(
    const PrefService& prefs,
    safe_browsing::ExtendedReportingOptInLocation location) {
  bool pref_value = safe_browsing::IsExtendedReportingEnabled(prefs);

  switch (location) {
    case safe_browsing::SBER_OPTIN_SITE_CHROME_SETTINGS:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.ChromeSettings",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.AndroidSettings",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_DOWNLOAD_FEEDBACK_POPUP:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.DownloadPopup",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_SECURITY_INTERSTITIAL:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.SecurityInterstitial",
                            pref_value);
      break;
    default:
      NOTREACHED();
  }
}

// A helper function to return a GURL containing just the scheme, host, port,
// and path from a URL. Equivalent to clearing any username, password, query,
// and ref. Return empty URL if |url| is not valid.
GURL GetSimplifiedURL(const GURL& url) {
  if (!url.is_valid() || !url.IsStandard())
    return GURL();

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();

  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace prefs {
const char kSafeBrowsingEnabled[] = "safebrowsing.enabled";
const char kSafeBrowsingEnhanced[] = "safebrowsing.enhanced";
const char kSafeBrowsingEnterpriseRealTimeUrlCheckMode[] =
    "safebrowsing.enterprise_real_time_url_check_mode";
const char kSafeBrowsingExtendedReportingOptInAllowed[] =
    "safebrowsing.extended_reporting_opt_in_allowed";
const char kSafeBrowsingIncidentsSent[] = "safebrowsing.incidents_sent";
const char kSafeBrowsingProceedAnywayDisabled[] =
    "safebrowsing.proceed_anyway_disabled";
const char kSafeBrowsingSawInterstitialScoutReporting[] =
    "safebrowsing.saw_interstitial_sber2";
const char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";
const char kSafeBrowsingTriggerEventTimestamps[] =
    "safebrowsing.trigger_event_timestamps";
const char kSafeBrowsingUnhandledGaiaPasswordReuses[] =
    "safebrowsing.unhandled_sync_password_reuses";
const char kSafeBrowsingNextPasswordCaptureEventLogTime[] =
    "safebrowsing.next_password_capture_event_log_time";
const char kSafeBrowsingWhitelistDomains[] =
    "safebrowsing.safe_browsing_whitelist_domains";
const char kPasswordProtectionChangePasswordURL[] =
    "safebrowsing.password_protection_change_password_url";
const char kPasswordProtectionLoginURLs[] =
    "safebrowsing.password_protection_login_urls";
const char kPasswordProtectionWarningTrigger[] =
    "safebrowsing.password_protection_warning_trigger";
const char kAdvancedProtectionLastRefreshInUs[] =
    "safebrowsing.advanced_protection_last_refresh";
const char kSafeBrowsingSendFilesForMalwareCheck[] =
    "safebrowsing.send_files_for_malware_check";
const char kUnsafeEventsReportingEnabled[] =
    "safebrowsing.unsafe_events_reporting";
const char kBlockLargeFileTransfer[] =
    "safebrowsing.block_large_file_transfers";
const char kDelayDeliveryUntilVerdict[] =
    "safebrowsing.delay_delivery_until_verdict";
const char kAllowPasswordProtectedFiles[] =
    "safebrowsing.allow_password_protected_files";
const char kCheckContentCompliance[] = "safebrowsing.check_content_compliance";
const char kBlockUnsupportedFiletypes[] =
    "safebrowsing.block_unsupported_filetypes";
const char kURLsToCheckComplianceOfDownloadedContent[] =
    "safebrowsing.urls_to_check_compliance_of_downloaded_content";
const char kURLsToCheckForMalwareOfUploadedContent[] =
    "safebrowsing.urls_to_check_for_malware_of_uploaded_content";
const char kURLsToNotCheckForMalwareOfDownloadedContent[] =
    "safebrowsing.urls_to_not_check_for_malware_of_downloaded_content";
const char kURLsToNotCheckComplianceOfUploadedContent[] =
    "policy.urls_to_not_check_compliance_of_uploaded_content";
const char kAdvancedProtectionAllowed[] =
    "safebrowsing.advanced_protection_allowed";

}  // namespace prefs

namespace safe_browsing {

SafeBrowsingState GetSafeBrowsingState(const PrefService& prefs) {
  if (IsEnhancedProtectionEnabled(prefs)) {
    return ENHANCED_PROTECTION;
  } else if (prefs.GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return STANDARD_PROTECTION;
  } else {
    return NO_SAFE_BROWSING;
  }
}

void SetSafeBrowsingState(PrefService* prefs, SafeBrowsingState state) {
  if (state == ENHANCED_PROTECTION) {
    SetEnhancedProtectionPref(prefs, true);
    SetStandardProtectionPref(prefs, true);
  } else if (state == STANDARD_PROTECTION) {
    SetEnhancedProtectionPref(prefs, false);
    SetStandardProtectionPref(prefs, true);
  } else {
    SetEnhancedProtectionPref(prefs, false);
    SetStandardProtectionPref(prefs, false);
  }
}

bool IsSafeBrowsingEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingEnabled);
}

bool IsEnhancedProtectionEnabled(const PrefService& prefs) {
  // SafeBrowsingEnabled is checked too due to devices being out
  // of sync or not on a version that includes SafeBrowsingEnhanced pref.
  return base::FeatureList::IsEnabled(kEnhancedProtection) &&
         prefs.GetBoolean(prefs::kSafeBrowsingEnhanced) &&
         IsSafeBrowsingEnabled(prefs);
}

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled);
}

ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs) {
  return IsExtendedReportingEnabled(prefs) ? SBER_LEVEL_SCOUT : SBER_LEVEL_OFF;
}

bool IsExtendedReportingOptInAllowed(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
}

bool IsExtendedReportingEnabled(const PrefService& prefs) {
  return (IsSafeBrowsingEnabled(prefs) &&
          prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled)) ||
         IsEnhancedProtectionEnabled(prefs);
}

bool IsExtendedReportingPolicyManaged(const PrefService& prefs) {
  return prefs.IsManagedPreference(prefs::kSafeBrowsingScoutReportingEnabled);
}

bool IsSafeBrowsingPolicyManaged(const PrefService& prefs) {
  return prefs.IsManagedPreference(prefs::kSafeBrowsingEnabled) ||
         prefs.IsManagedPreference(prefs::kSafeBrowsingEnhanced);
}

bool IsEnhancedProtectionMessageInInterstitialsEnabled() {
  return base::FeatureList::IsEnabled(
      kEnhancedProtectionMessageInInterstitials);
}

void RecordExtendedReportingMetrics(const PrefService& prefs) {
  // This metric tracks the extended browsing opt-in based on whichever setting
  // the user is currently seeing. It tells us whether extended reporting is
  // happening for this user.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended",
                        IsExtendedReportingEnabled(prefs));

  // Track whether this user has ever seen a security interstitial.
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.Pref.SawInterstitial",
      prefs.GetBoolean(prefs::kSafeBrowsingSawInterstitialScoutReporting));
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSafeBrowsingScoutReportingEnabled,
                                false);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingSawInterstitialScoutReporting, false);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingProceedAnywayDisabled,
                                false);
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingIncidentsSent);
  registry->RegisterDictionaryPref(
      prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  registry->RegisterStringPref(
      prefs::kSafeBrowsingNextPasswordCaptureEventLogTime,
      "0");  // int64 as string
  registry->RegisterListPref(prefs::kSafeBrowsingWhitelistDomains);
  registry->RegisterStringPref(prefs::kPasswordProtectionChangePasswordURL, "");
  registry->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
  registry->RegisterIntegerPref(prefs::kPasswordProtectionWarningTrigger,
                                PASSWORD_PROTECTION_OFF);
  registry->RegisterInt64Pref(prefs::kAdvancedProtectionLastRefreshInUs, 0);
  registry->RegisterIntegerPref(prefs::kSafeBrowsingSendFilesForMalwareCheck,
                                DO_NOT_SCAN);
  registry->RegisterBooleanPref(prefs::kAdvancedProtectionAllowed, true);
  registry->RegisterIntegerPref(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
      REAL_TIME_CHECK_DISABLED);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingTriggerEventTimestamps);
  registry->RegisterBooleanPref(prefs::kUnsafeEventsReportingEnabled, false);
  registry->RegisterIntegerPref(prefs::kBlockLargeFileTransfer, 0);
  registry->RegisterIntegerPref(prefs::kDelayDeliveryUntilVerdict, DELAY_NONE);
  registry->RegisterIntegerPref(
      prefs::kAllowPasswordProtectedFiles,
      AllowPasswordProtectedFilesValues::ALLOW_UPLOADS_AND_DOWNLOADS);
  registry->RegisterIntegerPref(prefs::kCheckContentCompliance, CHECK_NONE);
  registry->RegisterIntegerPref(prefs::kBlockUnsupportedFiletypes,
                                BLOCK_UNSUPPORTED_FILETYPES_NONE);
  registry->RegisterListPref(prefs::kURLsToCheckComplianceOfDownloadedContent);
  registry->RegisterListPref(prefs::kURLsToNotCheckComplianceOfUploadedContent);
  registry->RegisterListPref(prefs::kURLsToCheckForMalwareOfUploadedContent);
  registry->RegisterListPref(
      prefs::kURLsToNotCheckForMalwareOfDownloadedContent);
}

void SetExtendedReportingPrefAndMetric(
    PrefService* prefs,
    bool value,
    ExtendedReportingOptInLocation location) {
  prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, value);
  RecordExtendedReportingPrefChanged(*prefs, location);
}

void SetExtendedReportingPrefForTests(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, value);
}

void SetEnhancedProtectionPrefForTests(PrefService* prefs, bool value) {
  // SafeBrowsingEnabled pref needs to be turned on in order for enhanced
  // protection pref to be turned on. This method is only used for tests.
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, value);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, value);
}

void SetEnhancedProtectionPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, value);
}

void SetStandardProtectionPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, value);
}

void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs) {
  // Remember that this user saw an interstitial.
  prefs->SetBoolean(prefs::kSafeBrowsingSawInterstitialScoutReporting, true);
}

base::ListValue GetSafeBrowsingPreferencesList(PrefService* prefs) {
  base::ListValue preferences_list;

  const char* safe_browsing_preferences[] = {
      prefs::kSafeBrowsingEnabled,
      prefs::kSafeBrowsingExtendedReportingOptInAllowed,
      prefs::kSafeBrowsingScoutReportingEnabled, prefs::kSafeBrowsingEnhanced};

  // Add the status of the preferences if they are Enabled or Disabled for the
  // user.
  for (const char* preference : safe_browsing_preferences) {
    preferences_list.Append(base::Value(preference));
    bool enabled = prefs->GetBoolean(preference);
    preferences_list.Append(base::Value(enabled ? "Enabled" : "Disabled"));
  }
  return preferences_list;
}

void GetSafeBrowsingWhitelistDomainsPref(
    const PrefService& prefs,
    std::vector<std::string>* out_canonicalized_domain_list) {
  const base::ListValue* pref_value =
      prefs.GetList(prefs::kSafeBrowsingWhitelistDomains);
  CanonicalizeDomainList(*pref_value, out_canonicalized_domain_list);
}

void CanonicalizeDomainList(
    const base::ListValue& raw_domain_list,
    std::vector<std::string>* out_canonicalized_domain_list) {
  out_canonicalized_domain_list->clear();
  for (auto it = raw_domain_list.GetList().begin();
       it != raw_domain_list.GetList().end(); it++) {
    // Verify if it is valid domain string.
    url::CanonHostInfo host_info;
    std::string canonical_host =
        net::CanonicalizeHost(it->GetString(), &host_info);
    if (!canonical_host.empty())
      out_canonicalized_domain_list->push_back(canonical_host);
  }
}

bool IsURLWhitelistedByPolicy(const GURL& url,
                              StringListPrefMember* pref_member) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  if (!pref_member)
    return false;

  std::vector<std::string> sb_whitelist_domains = pref_member->GetValue();
  return std::find_if(sb_whitelist_domains.begin(), sb_whitelist_domains.end(),
                      [&url](const std::string& domain) {
                        return url.DomainIs(domain);
                      }) != sb_whitelist_domains.end();
}

bool IsURLWhitelistedByPolicy(const GURL& url, const PrefService& pref) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  if (!pref.HasPrefPath(prefs::kSafeBrowsingWhitelistDomains))
    return false;
  const base::ListValue* whitelist =
      pref.GetList(prefs::kSafeBrowsingWhitelistDomains);
  for (const base::Value& value : whitelist->GetList()) {
    if (url.DomainIs(value.GetString()))
      return true;
  }
  return false;
}

bool MatchesEnterpriseWhitelist(const PrefService& pref,
                                const std::vector<GURL>& url_chain) {
  for (const GURL& url : url_chain) {
    if (IsURLWhitelistedByPolicy(url, pref))
      return true;
  }
  return false;
}

void GetPasswordProtectionLoginURLsPref(const PrefService& prefs,
                                        std::vector<GURL>* out_login_url_list) {
  const base::ListValue* pref_value =
      prefs.GetList(prefs::kPasswordProtectionLoginURLs);
  out_login_url_list->clear();
  for (const base::Value& value : pref_value->GetList()) {
    GURL login_url(value.GetString());
    // Skip invalid or none-http/https login URLs.
    if (login_url.is_valid() && login_url.SchemeIsHTTPOrHTTPS())
      out_login_url_list->push_back(login_url);
  }
}

bool MatchesPasswordProtectionLoginURL(const GURL& url,
                                       const PrefService& prefs) {
  if (!url.is_valid())
    return false;

  std::vector<GURL> login_urls;
  GetPasswordProtectionLoginURLsPref(prefs, &login_urls);
  return MatchesURLList(url, login_urls);
}

bool MatchesURLList(const GURL& target_url, const std::vector<GURL> url_list) {
  if (url_list.empty() || !target_url.is_valid())
    return false;
  GURL simple_target_url = GetSimplifiedURL(target_url);
  for (const GURL& url : url_list) {
    if (GetSimplifiedURL(url) == simple_target_url) {
      return true;
    }
  }
  return false;
}

GURL GetPasswordProtectionChangePasswordURLPref(const PrefService& prefs) {
  if (!prefs.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL))
    return GURL();
  GURL change_password_url_from_pref(
      prefs.GetString(prefs::kPasswordProtectionChangePasswordURL));
  // Skip invalid or non-http/https URL.
  if (change_password_url_from_pref.is_valid() &&
      change_password_url_from_pref.SchemeIsHTTPOrHTTPS()) {
    return change_password_url_from_pref;
  }

  return GURL();
}

bool MatchesPasswordProtectionChangePasswordURL(const GURL& url,
                                                const PrefService& prefs) {
  if (!url.is_valid())
    return false;

  GURL change_password_url = GetPasswordProtectionChangePasswordURLPref(prefs);
  if (change_password_url.is_empty())
    return false;

  return GetSimplifiedURL(change_password_url) == GetSimplifiedURL(url);
}

}  // namespace safe_browsing
