// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"

#include <stdint.h>

#include <functional>
#include <set>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/base/hash_value.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace {

#if defined(OS_ANDROID)
StatefulSSLHostStateDelegate::RecurrentInterstitialMode
    kRecurrentInterstitialDefaultMode =
        StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF;
#else
StatefulSSLHostStateDelegate::RecurrentInterstitialMode
    kRecurrentInterstitialDefaultMode =
        StatefulSSLHostStateDelegate::RecurrentInterstitialMode::IN_MEMORY;
#endif

// The number of times an error must recur before the recurrent error message is
// shown.
constexpr int kRecurrentInterstitialDefaultThreshold = 3;

// If "mode" is "pref", a pref stores the time at which each error most recently
// occurred, and the recurrent error message is shown if the error has recurred
// more than the threshold number of times with the most recent instance being
// less than |kRecurrentInterstitialResetTimeParam| seconds in the past. The
// default is 3 days.
constexpr int kRecurrentInterstitialDefaultResetTime =
    259200;  // 3 days in seconds

// The default expiration for certificate error bypasses is one week, unless
// overidden by a field trial group.  See https://crbug.com/487270.
const uint64_t kDeltaDefaultExpirationInSeconds = UINT64_C(604800);

// Keys for the per-site error + certificate finger to judgment content
// settings map.
const char kSSLCertDecisionCertErrorMapKey[] = "cert_exceptions_map";
const char kSSLCertDecisionExpirationTimeKey[] = "decision_expiration_time";
const char kSSLCertDecisionVersionKey[] = "version";

const int kDefaultSSLCertDecisionVersion = 1;

// Records a new occurrence of |error|. The occurrence is stored in the
// recurrent interstitial pref, which keeps track of the most recent timestamps
// at which each error type occurred (up to the |threshold| most recent
// instances per error). The list is reset if the clock has gone backwards at
// any point.
void UpdateRecurrentInterstitialPref(PrefService* pref_service,
                                     base::Clock* clock,
                                     int error,
                                     int threshold) {
  double now = clock->Now().ToJsTime();

  DictionaryPrefUpdate pref_update(pref_service,
                                   prefs::kRecurrentSSLInterstitial);
  base::Value* list_value =
      pref_update->FindKey(net::ErrorToShortString(error));
  if (list_value) {
    // Check that the values are in increasing order and wipe out the list if
    // not (presumably because the clock changed).
    double previous = 0;
    for (const auto& error_instance : list_value->GetList()) {
      double error_time = error_instance.GetDouble();
      if (error_time < previous) {
        list_value = nullptr;
        break;
      }
      previous = error_time;
    }
    if (now < previous)
      list_value = nullptr;
  }

  if (!list_value) {
    // Either there was no list of occurrences of this error, or it was corrupt
    // (i.e. out of order). Save a new list composed of just this one error
    // instance.
    base::ListValue error_list;
    error_list.Append(now);
    pref_update->SetKey(net::ErrorToShortString(error), std::move(error_list));
  } else {
    // Only up to |threshold| values need to be stored. If the list already
    // contains |threshold| values, pop one off the front and append the new one
    // at the end; otherwise just append the new one.
    while (base::MakeStrictNum(list_value->GetList().size()) >= threshold) {
      list_value->EraseListIter(list_value->GetList().begin());
    }
    list_value->Append(now);
  }
}

bool DoesRecurrentInterstitialPrefMeetThreshold(PrefService* pref_service,
                                                base::Clock* clock,
                                                int error,
                                                int threshold,
                                                int error_reset_time) {
  const base::DictionaryValue* pref =
      pref_service->GetDictionary(prefs::kRecurrentSSLInterstitial);
  const base::Value* list_value = pref->FindKey(net::ErrorToShortString(error));
  if (!list_value)
    return false;

  base::Time cutoff_time;
  cutoff_time = clock->Now() - base::TimeDelta::FromSeconds(error_reset_time);

  // Assume that the values in the list are in increasing order;
  // UpdateRecurrentInterstitialPref() maintains this ordering. Check if there
  // are more than |threshold| values after the cutoff time.
  base::Value::ConstListView error_list = list_value->GetList();
  for (size_t i = 0; i < error_list.size(); i++) {
    if (base::Time::FromJsTime(error_list[i].GetDouble()) >= cutoff_time)
      return base::MakeStrictNum(error_list.size() - i) >= threshold;
  }
  return false;
}

// All SSL decisions are per host (and are shared arcoss schemes), so this
// canonicalizes all hosts into a secure scheme GURL to use with content
// settings. The returned GURL will be the passed in host with an empty path and
// https:// as the scheme.
GURL GetSecureGURLForHost(const std::string& host) {
  std::string url = "https://" + host;
  return GURL(url);
}

std::string GetKey(const net::X509Certificate& cert, int error) {
  // Since a security decision will be made based on the fingerprint, Chrome
  // should use the SHA-256 fingerprint for the certificate.
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  std::string base64_fingerprint;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(fingerprint.data),
                        sizeof(fingerprint.data)),
      &base64_fingerprint);
  return base::NumberToString(error) + base64_fingerprint;
}

bool HostFilterToPatternFilter(
    base::OnceCallback<bool(const std::string&)> host_filter,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // We only ever set origin-scoped exceptions which are of the form
  // "https://<host>:443". That is a valid URL, so we can compare |host_filter|
  // against its host.
  GURL url = GURL(primary_pattern.ToString());
  DCHECK(url.is_valid());
  return std::move(host_filter).Run(url.host());
}

}  // namespace

StatefulSSLHostStateDelegate::StatefulSSLHostStateDelegate(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map)
    : clock_(new base::DefaultClock()),
      browser_context_(browser_context),
      pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map),
      recurrent_interstitial_threshold_for_testing(-1),
      recurrent_interstitial_mode_for_testing(NOT_SET),
      recurrent_interstitial_reset_time_for_testing(-1) {
}

StatefulSSLHostStateDelegate::~StatefulSSLHostStateDelegate() = default;

void StatefulSSLHostStateDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kRecurrentSSLInterstitial);
}

void StatefulSSLHostStateDelegate::AllowCert(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          browser_context_, web_contents->GetMainFrame()->GetSiteInstance(),
          false /* can_create */);
  if (!storage_partition ||
      storage_partition != content::BrowserContext::GetDefaultStoragePartition(
                               browser_context_)) {
    // Decisions for non-default storage partitions are stored in memory only;
    // see comment on declaration of
    // |allowed_certs_for_non_default_storage_partitions_|.
    auto allowed_cert =
        AllowedCert(GetKey(cert, error), storage_partition->GetPath());
    allowed_certs_for_non_default_storage_partitions_[host].insert(
        allowed_cert);
    return;
  }

  GURL url = GetSecureGURLForHost(host);
  std::unique_ptr<base::Value> value(
      host_content_settings_map_->GetWebsiteSetting(
          url, url, ContentSettingsType::SSL_CERT_DECISIONS, std::string(),
          nullptr));

  if (!value.get() || !value->is_dict())
    value.reset(new base::DictionaryValue());

  base::DictionaryValue* dict;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  base::DictionaryValue* cert_dict =
      GetValidCertDecisionsDict(dict, CREATE_DICTIONARY_ENTRIES);
  // If a a valid certificate dictionary cannot be extracted from the content
  // setting, that means it's in an unknown format. Unfortunately, there's
  // nothing to be done in that case, so a silent fail is the only option.
  if (!cert_dict)
    return;

  dict->SetKey(kSSLCertDecisionVersionKey,
               base::Value(kDefaultSSLCertDecisionVersion));
  cert_dict->SetKey(GetKey(cert, error), base::Value(ALLOWED));

  // The map takes ownership of the value, so it is released in the call to
  // SetWebsiteSettingDefaultScope.
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SSL_CERT_DECISIONS, std::string(),
      std::move(value));
}

void StatefulSSLHostStateDelegate::Clear(
    base::RepeatingCallback<bool(const std::string&)> host_filter) {
  // Convert host matching to content settings pattern matching. Content
  // settings deletion is done synchronously on the UI thread, so we can use
  // |host_filter| by reference.
  HostContentSettingsMap::PatternSourcePredicate pattern_filter;
  if (!host_filter.is_null()) {
    pattern_filter =
        base::BindRepeating(&HostFilterToPatternFilter, host_filter);
  }

  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SSL_CERT_DECISIONS, base::Time(), base::Time::Max(),
      pattern_filter);
}

content::SSLHostStateDelegate::CertJudgment
StatefulSSLHostStateDelegate::QueryPolicy(const std::string& host,
                                          const net::X509Certificate& cert,
                                          int error,
                                          content::WebContents* web_contents) {
  DCHECK(web_contents);

  // If the appropriate flag is set, let requests on localhost go
  // through even if there are certificate errors. Errors on localhost
  // are unlikely to indicate actual security problems.
  GURL url = GetSecureGURLForHost(host);
  bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowInsecureLocalhost);
  if (allow_localhost && net::IsLocalhost(url))
    return ALLOWED;

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          browser_context_, web_contents->GetMainFrame()->GetSiteInstance(),
          false /* can_create */);
  if (!storage_partition ||
      storage_partition != content::BrowserContext::GetDefaultStoragePartition(
                               browser_context_)) {
    if (allowed_certs_for_non_default_storage_partitions_.find(host) ==
        allowed_certs_for_non_default_storage_partitions_.end()) {
      return DENIED;
    }
    AllowedCert allowed_cert =
        AllowedCert(GetKey(cert, error), storage_partition->GetPath());
    if (base::Contains(allowed_certs_for_non_default_storage_partitions_[host],
                       allowed_cert)) {
      return ALLOWED;
    }
    return DENIED;
  }

  std::unique_ptr<base::Value> value(
      host_content_settings_map_->GetWebsiteSetting(
          url, url, ContentSettingsType::SSL_CERT_DECISIONS, std::string(),
          nullptr));

  if (!value.get() || !value->is_dict())
    return DENIED;

  base::DictionaryValue* dict;  // Owned by value
  int policy_decision;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  base::DictionaryValue* cert_error_dict;  // Owned by value
  cert_error_dict =
      GetValidCertDecisionsDict(dict, DO_NOT_CREATE_DICTIONARY_ENTRIES);
  if (!cert_error_dict) {
    // This revoke is necessary to clear any old expired setting that may be
    // lingering in the case that an old decision expried.
    RevokeUserAllowExceptions(host);
    return DENIED;
  }

  success = cert_error_dict->GetIntegerWithoutPathExpansion(GetKey(cert, error),
                                                            &policy_decision);

  // If a policy decision was successfully retrieved and it's a valid value of
  // ALLOWED, return the valid value. Otherwise, return DENIED.
  if (success && policy_decision == ALLOWED)
    return ALLOWED;

  return DENIED;
}

void StatefulSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  switch (content_type) {
    case MIXED_CONTENT:
      ran_mixed_content_hosts_.insert(BrokenHostEntry(host, child_id));
      return;
    case CERT_ERRORS_CONTENT:
      ran_content_with_cert_errors_hosts_.insert(
          BrokenHostEntry(host, child_id));
      return;
  }
}

bool StatefulSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  auto entry = BrokenHostEntry(host, child_id);
  switch (content_type) {
    case MIXED_CONTENT:
      return base::Contains(ran_mixed_content_hosts_, entry);
    case CERT_ERRORS_CONTENT:
      return base::Contains(ran_content_with_cert_errors_hosts_, entry);
  }
  NOTREACHED();
  return false;
}

void StatefulSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);

  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SSL_CERT_DECISIONS, std::string(),
      nullptr);

  // Decisions for non-default storage partitions are stored separately in
  // memory; delete those as well.
  allowed_certs_for_non_default_storage_partitions_.erase(host);
}

bool StatefulSSLHostStateDelegate::HasAllowException(
    const std::string& host,
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          browser_context_, web_contents->GetMainFrame()->GetSiteInstance(),
          false /* can_create */);
  if (!storage_partition ||
      storage_partition != content::BrowserContext::GetDefaultStoragePartition(
                               browser_context_)) {
    return allowed_certs_for_non_default_storage_partitions_.find(host) !=
           allowed_certs_for_non_default_storage_partitions_.end();
  }

  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  std::unique_ptr<base::Value> value(
      host_content_settings_map_->GetWebsiteSetting(
          url, url, ContentSettingsType::SSL_CERT_DECISIONS, std::string(),
          nullptr));

  if (!value.get() || !value->is_dict())
    return false;

  base::DictionaryValue* dict;  // Owned by value
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    int policy_decision;  // Owned by dict
    success = it.value().GetAsInteger(&policy_decision);
    if (success && (static_cast<CertJudgment>(policy_decision) == ALLOWED))
      return true;
  }

  return false;
}

// TODO(jww): This will revoke all of the decisions in the browser context.
// However, the networking stack actually keeps track of its own list of
// exceptions per-HttpNetworkTransaction in the SSLConfig structure (see the
// allowed_bad_certs Vector in net/ssl/ssl_config.h). This dual-tracking of
// exceptions introduces a problem where the browser context can revoke a
// certificate, but if a transaction reuses a cached version of the SSLConfig
// (probably from a pooled socket), it may bypass the intestitial layer.
//
// Over time, the cached versions should expire and it should converge on
// showing the interstitial. We probably need to introduce into the networking
// stack a way revoke SSLConfig's allowed_bad_certs lists per socket.
//
// For now, RevokeUserAllowExceptionsHard is our solution for the rare case
// where it is necessary to revoke the preferences immediately. It does so by
// flushing idle sockets, thus it is a big hammer and should be wielded with
// extreme caution as it can have a big, negative impact on network performance.
void StatefulSSLHostStateDelegate::RevokeUserAllowExceptionsHard(
    const std::string& host) {
  RevokeUserAllowExceptions(host);
  auto* network_context =
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetNetworkContext();
  network_context->CloseIdleConnections(base::NullCallback());
}

void StatefulSSLHostStateDelegate::DidDisplayErrorPage(int error) {
  if (error != net::ERR_CERT_SYMANTEC_LEGACY &&
      error != net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
    return;
  }
  RecurrentInterstitialMode mode_param = GetRecurrentInterstitialMode();
  const int threshold = GetRecurrentInterstitialThreshold();
  if (mode_param ==
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::IN_MEMORY) {
    const auto count_it = recurrent_errors_.find(error);
    if (count_it == recurrent_errors_.end()) {
      recurrent_errors_[error] = 1;
      return;
    }
    if (count_it->second >= threshold) {
      return;
    }
    recurrent_errors_[error] = count_it->second + 1;
  } else if (mode_param ==
             StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF) {
    UpdateRecurrentInterstitialPref(pref_service_, clock_.get(), error,
                                    threshold);
  }
}

bool StatefulSSLHostStateDelegate::HasSeenRecurrentErrors(int error) const {
  RecurrentInterstitialMode mode_param = GetRecurrentInterstitialMode();
  const int threshold = GetRecurrentInterstitialThreshold();
  if (mode_param ==
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::IN_MEMORY) {
    const auto count_it = recurrent_errors_.find(error);
    if (count_it == recurrent_errors_.end())
      return false;
    return count_it->second >= threshold;
  } else if (mode_param ==
             StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF) {
    return DoesRecurrentInterstitialPrefMeetThreshold(
        pref_service_, clock_.get(), error, threshold,
        GetRecurrentInterstitialResetTime());
  }

  return false;
}

void StatefulSSLHostStateDelegate::ResetRecurrentErrorCountForTesting() {
  recurrent_errors_.clear();
  DictionaryPrefUpdate pref_update(pref_service_,
                                   prefs::kRecurrentSSLInterstitial);
  pref_update->Clear();
}

void StatefulSSLHostStateDelegate::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void StatefulSSLHostStateDelegate::SetRecurrentInterstitialThresholdForTesting(
    int threshold) {
  recurrent_interstitial_threshold_for_testing = threshold;
}

void StatefulSSLHostStateDelegate::SetRecurrentInterstitialModeForTesting(
    StatefulSSLHostStateDelegate::RecurrentInterstitialMode mode) {
  recurrent_interstitial_mode_for_testing = mode;
}

void StatefulSSLHostStateDelegate::SetRecurrentInterstitialResetTimeForTesting(
    int reset) {
  recurrent_interstitial_reset_time_for_testing = reset;
}

int StatefulSSLHostStateDelegate::GetRecurrentInterstitialThreshold() const {
  if (recurrent_interstitial_threshold_for_testing == -1) {
    return kRecurrentInterstitialDefaultThreshold;
  } else {
    return recurrent_interstitial_threshold_for_testing;
  }
}

int StatefulSSLHostStateDelegate::GetRecurrentInterstitialResetTime() const {
  if (recurrent_interstitial_reset_time_for_testing == -1) {
    return kRecurrentInterstitialDefaultResetTime;
  } else {
    return recurrent_interstitial_reset_time_for_testing;
  }
}

StatefulSSLHostStateDelegate::RecurrentInterstitialMode
StatefulSSLHostStateDelegate::GetRecurrentInterstitialMode() const {
  if (recurrent_interstitial_mode_for_testing == NOT_SET) {
    return kRecurrentInterstitialDefaultMode;
  } else {
    return recurrent_interstitial_mode_for_testing;
  }
}

// This helper function gets the dictionary of certificate fingerprints to
// errors of certificates that have been accepted by the user from the content
// dictionary that has been passed in. The returned pointer is owned by the the
// argument dict that is passed in.
//
// If create_entries is set to |DO_NOT_CREATE_DICTIONARY_ENTRIES|,
// GetValidCertDecisionsDict will return nullptr if there is anything invalid
// about the setting, such as an invalid version or invalid value types (in
// addition to there not being any values in the dictionary). If create_entries
// is set to |CREATE_DICTIONARY_ENTRIES|, if no dictionary is found or the
// decisions are expired, a new dictionary will be created.
base::DictionaryValue* StatefulSSLHostStateDelegate::GetValidCertDecisionsDict(
    base::DictionaryValue* dict,
    CreateDictionaryEntriesDisposition create_entries) {
  // Extract the version of the certificate decision structure from the content
  // setting.
  int version;
  bool success = dict->GetInteger(kSSLCertDecisionVersionKey, &version);
  if (!success) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    dict->SetInteger(kSSLCertDecisionVersionKey,
                     kDefaultSSLCertDecisionVersion);
    version = kDefaultSSLCertDecisionVersion;
  }

  // If the version is somehow a newer version than Chrome can handle, there's
  // really nothing to do other than fail silently and pretend it doesn't exist
  // (or is malformed).
  if (version > kDefaultSSLCertDecisionVersion) {
    LOG(ERROR) << "Failed to parse a certificate error exception that is in a "
               << "newer version format (" << version << ") than is supported ("
               << kDefaultSSLCertDecisionVersion << ")";
    return nullptr;
  }

  // Extract the certificate decision's expiration time from the content
  // setting. If there is no expiration time, that means it should never expire
  // and it should reset only at session restart, so skip all of the expiration
  // checks.
  bool expired = false;
  base::Time now = clock_->Now();
  base::Time decision_expiration;
  if (dict->HasKey(kSSLCertDecisionExpirationTimeKey)) {
    std::string decision_expiration_string;
    int64_t decision_expiration_int64;
    success = dict->GetString(kSSLCertDecisionExpirationTimeKey,
                              &decision_expiration_string);
    if (!base::StringToInt64(base::StringPiece(decision_expiration_string),
                             &decision_expiration_int64)) {
      LOG(ERROR) << "Failed to parse a certificate error exception that has a "
                 << "bad value for an expiration time: "
                 << decision_expiration_string;
      return nullptr;
    }
    decision_expiration =
        base::Time::FromInternalValue(decision_expiration_int64);
  }

  // Check to see if the user's certificate decision has expired.
  // - Expired and |create_entries| is DO_NOT_CREATE_DICTIONARY_ENTRIES, return
  // nullptr.
  // - Expired and |create_entries| is CREATE_DICTIONARY_ENTRIES, update the
  // expiration time.
  if (decision_expiration.ToInternalValue() <= now.ToInternalValue()) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    expired = true;
    base::Time expiration_time =
        now + base::TimeDelta::FromSeconds(kDeltaDefaultExpirationInSeconds);
    // Unfortunately, JSON (and thus content settings) doesn't support int64_t
    // values, only doubles. Since this mildly depends on precision, it is
    // better to store the value as a string.
    dict->SetString(kSSLCertDecisionExpirationTimeKey,
                    base::NumberToString(expiration_time.ToInternalValue()));
  }

  // Extract the map of certificate fingerprints to errors from the setting.
  base::DictionaryValue* cert_error_dict = nullptr;  // Will be owned by dict
  if (expired ||
      !dict->GetDictionary(kSSLCertDecisionCertErrorMapKey, &cert_error_dict)) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    cert_error_dict =
        dict->SetDictionary(kSSLCertDecisionCertErrorMapKey,
                            std::make_unique<base::DictionaryValue>());
  }

  return cert_error_dict;
}
