// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/security_interstitials/core/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace {

// Certificate error bypasses are remembered for one week.
const uint64_t kCertErrorBypassExpirationInSeconds = UINT64_C(604800);

// The expiration for HTTPS-First Mode bypasses is 15 days.
const uint64_t kHTTPSFirstModeBypassExpirationInSeconds = UINT64_C(1296000);

// Keys for the per-site error + certificate finger to judgment content
// settings map.
const char kSSLCertDecisionCertErrorMapKey[] = "cert_exceptions_map";
const char kSSLCertDecisionExpirationTimeKey[] = "decision_expiration_time";
const char kSSLCertDecisionVersionKey[] = "version";

const int kDefaultSSLCertDecisionVersion = 1;

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
  std::string base64_fingerprint = base::Base64Encode(fingerprint);
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
  return std::move(host_filter).Run(url.GetHost());
}

}  // namespace

StatefulSSLHostStateDelegate::StatefulSSLHostStateDelegate(
    content::BrowserContext* browser_context,
    HostContentSettingsMap* host_content_settings_map)
    : clock_(new base::DefaultClock()),
      browser_context_(browser_context),
      host_content_settings_map_(host_content_settings_map),
      https_only_mode_allowlist_(
          host_content_settings_map,
          clock_.get(),
          base::Seconds(kHTTPSFirstModeBypassExpirationInSeconds)),
      https_only_mode_enforcelist_(host_content_settings_map_, clock_.get()),
      https_first_balanced_mode_suppressed_for_testing(false) {}

StatefulSSLHostStateDelegate::~StatefulSSLHostStateDelegate() = default;

void StatefulSSLHostStateDelegate::AllowCert(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::StoragePartition* storage_partition) {
  if (!storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition()) {
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
  base::Value value(host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::SSL_CERT_DECISIONS, nullptr));

  if (!value.is_dict())
    value = base::Value(base::Value::Type::DICT);

  base::Value::Dict* cert_dict =
      GetValidCertDecisionsDict(CREATE_DICTIONARY_ENTRIES, value.GetDict());
  // If a a valid certificate dictionary cannot be extracted from the content
  // setting, that means it's in an unknown format. Unfortunately, there's
  // nothing to be done in that case, so a silent fail is the only option.
  if (!cert_dict)
    return;

  value.GetDict().Set(kSSLCertDecisionVersionKey,
                      kDefaultSSLCertDecisionVersion);
  cert_dict->Set(GetKey(cert, error), ALLOWED);

  // The map takes ownership of the value, so it is released in the call to
  // SetWebsiteSettingDefaultScope.
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SSL_CERT_DECISIONS, std::move(value));
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
  https_only_mode_allowlist_.Clear(base::Time(), base::Time::Max(),
                                   pattern_filter);

  // This might not be necessary since the engagement score of a site will be
  // cleared and the site will be removed from the enforcelist. However, there
  // could be a period between clearing and navigating where the enforcelist
  // would still be persisted on disk etc, so clear it explicitly.
  https_only_mode_enforcelist_.Clear(base::Time(), base::Time::Max(),
                                     pattern_filter);
}

content::SSLHostStateDelegate::CertJudgment
StatefulSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::StoragePartition* storage_partition) {
  if (!storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition()) {
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

  GURL url = GetSecureGURLForHost(host);
  base::Value value(host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::SSL_CERT_DECISIONS, nullptr));

  if (!value.is_dict())
    return DENIED;

  base::Value::Dict* cert_error_dict = GetValidCertDecisionsDict(
      DO_NOT_CREATE_DICTIONARY_ENTRIES, value.GetDict());
  if (!cert_error_dict) {
    // This revoke is necessary to clear any old expired setting that may be
    // lingering in the case that an old decision expried.
    RevokeUserAllowExceptions(host);
    return DENIED;
  }

  std::optional<int> policy_decision =
      cert_error_dict->FindInt(GetKey(cert, error));

  // If a policy decision was successfully retrieved and it's a valid value of
  // ALLOWED, return the valid value. Otherwise, return DENIED.
  if (policy_decision.has_value() && policy_decision.value() == ALLOWED)
    return ALLOWED;

  return DENIED;
}

void StatefulSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    InsecureContentType content_type) {
  switch (content_type) {
    case MIXED_CONTENT:
      ran_mixed_content_hosts_.insert(host);
      return;
    case CERT_ERRORS_CONTENT:
      ran_content_with_cert_errors_hosts_.insert(host);
      return;
  }
}

bool StatefulSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    InsecureContentType content_type) {
  switch (content_type) {
    case MIXED_CONTENT:
      return base::Contains(ran_mixed_content_hosts_, host);
    case CERT_ERRORS_CONTENT:
      return base::Contains(ran_content_with_cert_errors_hosts_, host);
  }
  NOTREACHED();
}

void StatefulSSLHostStateDelegate::AllowHttpForHost(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();
  https_only_mode_allowlist_.AllowHttpForHost(host, is_nondefault_storage);
}

bool StatefulSSLHostStateDelegate::IsHttpAllowedForHost(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();
  return https_only_mode_allowlist_.IsHttpAllowedForHost(host,
                                                         is_nondefault_storage);
}

void StatefulSSLHostStateDelegate::SetHttpsEnforcementForHost(
    const std::string& host,
    bool enforced,
    content::StoragePartition* storage_partition) {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();
  if (enforced) {
    https_only_mode_enforcelist_.EnforceForHost(host, is_nondefault_storage);
  } else {
    https_only_mode_enforcelist_.UnenforceForHost(host, is_nondefault_storage);
  }
}

bool StatefulSSLHostStateDelegate::IsHttpsEnforcedForUrl(
    const GURL& url,
    content::StoragePartition* storage_partition) {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();
  return https_only_mode_enforcelist_.IsEnforcedForUrl(url,
                                                       is_nondefault_storage);
}

std::set<GURL> StatefulSSLHostStateDelegate::GetHttpsEnforcedHosts(
    content::StoragePartition* storage_partition) const {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();
  return https_only_mode_enforcelist_.GetHosts(is_nondefault_storage);
}

void StatefulSSLHostStateDelegate::ClearHttpsOnlyModeAllowlist() {
  https_only_mode_allowlist_.ClearAllowlist(base::Time(), base::Time::Max());
}

void StatefulSSLHostStateDelegate::ClearHttpsEnforcelist() {
  https_only_mode_enforcelist_.ClearEnforcements(base::Time(),
                                                 base::Time::Max());
}

void StatefulSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SSL_CERT_DECISIONS, base::Value());
  // Decisions for non-default storage partitions are stored separately in
  // memory; delete those as well.
  allowed_certs_for_non_default_storage_partitions_.erase(host);

  https_only_mode_allowlist_.RevokeUserAllowExceptions(host);
  https_only_mode_enforcelist_.RevokeEnforcements(host);
}

bool StatefulSSLHostStateDelegate::HasAllowException(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  return HasCertAllowException(host, storage_partition) ||
         IsHttpAllowedForHost(host, storage_partition);
}

bool StatefulSSLHostStateDelegate::HasAllowExceptionForAnyHost(
    content::StoragePartition* storage_partition) {
  return HasCertAllowExceptionForAnyHost(storage_partition) ||
         IsHttpAllowedForAnyHost(storage_partition);
}

bool StatefulSSLHostStateDelegate::HasCertAllowExceptionForAnyHost(
    content::StoragePartition* storage_partition) {
  if (!storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition()) {
    return !allowed_certs_for_non_default_storage_partitions_.empty();
  }

  ContentSettingsForOneType content_settings_list =
      host_content_settings_map_->GetSettingsForOneType(
          ContentSettingsType::SSL_CERT_DECISIONS);
  return !content_settings_list.empty();
}

bool StatefulSSLHostStateDelegate::IsHttpAllowedForAnyHost(
    content::StoragePartition* storage_partition) {
  bool is_nondefault_storage =
      !storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition();

  return https_only_mode_allowlist_.IsHttpAllowedForAnyHost(
      is_nondefault_storage);
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
      browser_context_->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->CloseIdleConnections(base::NullCallback());
}

bool StatefulSSLHostStateDelegate::
    HttpsFirstBalancedModeSuppressedForTesting() {
  return https_first_balanced_mode_suppressed_for_testing;
}

void StatefulSSLHostStateDelegate::
    SetHttpsFirstBalancedModeSuppressedForTesting(bool suppressed) {
  https_first_balanced_mode_suppressed_for_testing = suppressed;
}

void StatefulSSLHostStateDelegate::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  // Pointers to the existing Clock object must be reset before swapping the
  // underlying Clock object, otherwise they are dangling (briefly).
  https_only_mode_allowlist_.SetClockForTesting(nullptr);    // IN-TEST
  https_only_mode_enforcelist_.SetClockForTesting(nullptr);  // IN-TEST

  clock_ = std::move(clock);
  https_only_mode_allowlist_.SetClockForTesting(clock_.get());    // IN-TEST
  https_only_mode_enforcelist_.SetClockForTesting(clock_.get());  // IN-TEST
}

bool StatefulSSLHostStateDelegate::HasCertAllowException(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  if (!storage_partition ||
      storage_partition != browser_context_->GetDefaultStoragePartition()) {
    return base::Contains(allowed_certs_for_non_default_storage_partitions_,
                          host);
  }

  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  const base::Value value(host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::SSL_CERT_DECISIONS, nullptr));

  if (!value.is_dict())
    return false;

  for (const auto pair : value.GetDict()) {
    if (!pair.second.is_int())
      continue;

    if (static_cast<CertJudgment>(pair.second.GetInt()) == ALLOWED)
      return true;
  }

  return false;
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
base::Value::Dict* StatefulSSLHostStateDelegate::GetValidCertDecisionsDict(
    CreateDictionaryEntriesDisposition create_entries,
    base::Value::Dict& dict) {
  // Extract the version of the certificate decision structure from the content
  // setting.
  std::optional<int> version = dict.FindInt(kSSLCertDecisionVersionKey);
  if (!version) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    dict.Set(kSSLCertDecisionVersionKey, kDefaultSSLCertDecisionVersion);
    version = std::make_optional<int>(kDefaultSSLCertDecisionVersion);
  }

  // If the version is somehow a newer version than Chrome can handle, there's
  // really nothing to do other than fail silently and pretend it doesn't exist
  // (or is malformed).
  if (*version > kDefaultSSLCertDecisionVersion) {
    LOG(ERROR) << "Failed to parse a certificate error exception that is in a "
               << "newer version format (" << *version << ") than is supported"
               << "(" << kDefaultSSLCertDecisionVersion << ")";
    return nullptr;
  }

  // Extract the certificate decision's expiration time from the content
  // setting. If there is no expiration time, that means it should never expire
  // and it should reset only at session restart, so skip all of the expiration
  // checks.
  bool expired = false;
  base::Time now = clock_->Now();
  auto* decision_expiration_value =
      dict.Find(kSSLCertDecisionExpirationTimeKey);
  auto decision_expiration = base::ValueToTime(decision_expiration_value);

  // Check to see if the user's certificate decision has expired.
  // - Expired and |create_entries| is DO_NOT_CREATE_DICTIONARY_ENTRIES, return
  // nullptr.
  // - Expired and |create_entries| is CREATE_DICTIONARY_ENTRIES, update the
  // expiration time.
  if (decision_expiration <= now) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    expired = true;
    base::Time expiration_time =
        now + base::Seconds(kCertErrorBypassExpirationInSeconds);
    // Unfortunately, JSON (and thus content settings) doesn't support int64_t
    // values, only doubles. Since this mildly depends on precision, it is
    // better to store the value as a string.
    dict.Set(kSSLCertDecisionExpirationTimeKey,
             base::TimeToValue(expiration_time));
  }

  // Extract the map of certificate fingerprints to errors from the setting.
  base::Value::Dict* cert_error_dict =
      dict.FindDict(kSSLCertDecisionCertErrorMapKey);
  if (expired || !cert_error_dict) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return nullptr;

    cert_error_dict = dict.EnsureDict(kSSLCertDecisionCertErrorMapKey);
    cert_error_dict->clear();
  }

  return cert_error_dict;
}
