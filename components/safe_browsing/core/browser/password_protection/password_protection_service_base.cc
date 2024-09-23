// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/password_protection_service_base.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

using password_manager::metrics_util::PasswordType;

namespace safe_browsing {

using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

namespace {

// Keys for storing password protection verdict into a base::Value::Dict.
const int kRequestTimeoutMs = 10000;
const char kPasswordProtectionRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/login";

// Check if the verdict makes this a security sensitive event.
bool IsSecuritySensitiveVerdict(
    LoginReputationClientResponse::VerdictType verdict_type) {
  switch (verdict_type) {
    case LoginReputationClientResponse::SAFE:
    // UNSPECIFIED is not considered sensitive because it is the default verdict
    // if no ping is sent (e.g. timeout, allowlist hit).
    case LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED:
      return false;
    case LoginReputationClientResponse::LOW_REPUTATION:
    case LoginReputationClientResponse::PHISHING:
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected verdict_type: " << verdict_type;
  return false;
}

// Log security sensitive event if required.
void MaybeRecordSecuritySensitiveEvent(
    SafeBrowsingMetricsCollector* metrics_collector,
    LoginReputationClientResponse::VerdictType verdict_type) {
  if (metrics_collector && IsSecuritySensitiveVerdict(verdict_type)) {
    metrics_collector->AddSafeBrowsingEventToPref(
        SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_PASSWORD_PROTECTION);
  }
}
}  // namespace

PasswordProtectionServiceBase::PasswordProtectionServiceBase(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    bool is_off_the_record,
    signin::IdentityManager* identity_manager,
    bool try_token_fetch,
    SafeBrowsingMetricsCollector* metrics_collector)
    : database_manager_(database_manager),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      token_fetcher_(std::move(token_fetcher)),
      is_off_the_record_(is_off_the_record),
      identity_manager_(identity_manager),
      try_token_fetch_(try_token_fetch),
      metrics_collector_(metrics_collector) {
  if (history_service)
    history_service_observation_.Observe(history_service);
}

PasswordProtectionServiceBase::~PasswordProtectionServiceBase() {
  tracker_.TryCancelAll();
  CancelPendingRequests();
  history_service_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

// static
bool PasswordProtectionServiceBase::CanGetReputationOfURL(const GURL& url) {
  if (VerdictCacheManager::has_artificial_cached_url()) {
    return true;
  }
  if (!safe_browsing::CanGetReputationOfUrl(url)) {
    return false;
  }
  const std::string hostname = url.HostNoBrackets();
  return !net::IsHostnameNonUnique(hostname);
}

bool PasswordProtectionServiceBase::ShouldShowModalWarning(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    LoginReputationClientResponse::VerdictType verdict_type) {
  if (trigger_type != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
      !IsSupportedPasswordTypeForModalWarning(password_type)) {
    return false;
  }

  return (verdict_type == LoginReputationClientResponse::PHISHING ||
          verdict_type == LoginReputationClientResponse::LOW_REPUTATION) &&
         IsWarningEnabled(password_type);
}

LoginReputationClientResponse::VerdictType
PasswordProtectionServiceBase::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    LoginReputationClientResponse* out_response) {
  return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
}

void PasswordProtectionServiceBase::CacheVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    const LoginReputationClientResponse& verdict,
    const base::Time& receive_time) {}

bool PasswordProtectionServiceBase::CanSendPing(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& main_frame_url,
    ReusedPasswordAccountType password_type) {
  return IsPingingEnabled(trigger_type, password_type) &&
         !IsURLAllowlistedForPasswordEntry(main_frame_url) &&
         !IsInExcludedCountry() &&
         // Although we can't get the reputation of the main frame URL for
         // password reuse on about:blank, the referrer chain still provides
         // enough useful information that we should send the ping.
         (main_frame_url == GURL("about:blank") ||
          CanGetReputationOfURL(main_frame_url));
}

void PasswordProtectionServiceBase::RequestFinished(
    PasswordProtectionRequest* request,
    RequestOutcome outcome,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);

#if !BUILDFLAG(IS_ANDROID)
  bool warning_shown = false;
#endif
  if (response) {
    ReusedPasswordAccountType password_type =
        GetPasswordProtectionReusedPasswordAccountType(request->password_type(),
                                                       request->username());
    if (outcome != RequestOutcome::RESPONSE_ALREADY_CACHED) {
      CacheVerdict(request->main_frame_url(), request->trigger_type(),
                   password_type, *response, base::Time::Now());
    }

    if (ShouldShowModalWarning(request->trigger_type(), password_type,
                               response->verdict_type())) {
      username_for_last_shown_warning_ = request->username();
      reused_password_account_type_for_last_shown_warning_ = password_type;
      saved_passwords_matching_domains_ = request->matching_domains();
      ShowModalWarning(request, response->verdict_type(),
                       response->verdict_token(), password_type);
      request->set_is_modal_warning_showing(true);
#if !BUILDFLAG(IS_ANDROID)
      warning_shown = true;
#endif
    }
  }

  ResumeDeferredNavigationsIfNeeded(request);

  // If the request is canceled, the PasswordProtectionServiceBase is already
  // partially destroyed, and we won't be able to log accurate metrics.
  if (outcome != RequestOutcome::CANCELED) {
    auto verdict =
        response ? response->verdict_type()
                 : LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

    // If verdict declares a security sensitive event, log accordingly.
    MaybeRecordSecuritySensitiveEvent(metrics_collector_, verdict);

// Disabled on Android, because enterprise reporting extension is not supported.
#if !BUILDFLAG(IS_ANDROID)
    MaybeReportPasswordReuseDetected(
        request->main_frame_url(), request->username(),
        request->password_type(),
        verdict == LoginReputationClientResponse::PHISHING, warning_shown);
#endif

    // Persist a bit in CompromisedCredentials table when saved password is
    // reused on a phishing or low reputation site.
    auto is_unsafe_url =
        verdict == LoginReputationClientResponse::PHISHING ||
        verdict == LoginReputationClientResponse::LOW_REPUTATION;
    if (is_unsafe_url) {
      PersistPhishedSavedPasswordCredential(
          request->matching_reused_credentials());
    }
  }

  // Remove request from |pending_requests_| list. If it triggers warning, add
  // it into the |warning_requests_| list.
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       it++) {
    if (it->get() == request) {
      if (request->is_modal_warning_showing())
        warning_requests_.insert(std::move(request));
      pending_requests_.erase(it);
      break;
    }
  }
}

void PasswordProtectionServiceBase::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    PasswordProtectionRequest* request = it->get();
    // These are the requests for whom we're still waiting for verdicts.
    // We need to advance the iterator before we cancel because canceling
    // the request will invalidate it when RequestFinished is called.
    it++;
    request->Cancel(false);
  }
  DCHECK(pending_requests_.empty());
}

int PasswordProtectionServiceBase::GetStoredVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  return -1;
}

scoped_refptr<SafeBrowsingDatabaseManager>
PasswordProtectionServiceBase::database_manager() {
  return database_manager_;
}

// static
GURL PasswordProtectionServiceBase::GetPasswordProtectionRequestUrl() {
  GURL url(kPasswordProtectionRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  DCHECK(!api_key.empty());
  return url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
}

// static
int PasswordProtectionServiceBase::GetRequestTimeoutInMS() {
  return kRequestTimeoutMs;
}

void PasswordProtectionServiceBase::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(&PasswordProtectionServiceBase::
                              RemoveUnhandledSyncPasswordReuseOnURLsDeleted,
                          GetWeakPtr(), deletion_info.IsAllHistory(),
                          deletion_info.deleted_rows()));
}

void PasswordProtectionServiceBase::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_service_observation_.IsObservingSource(history_service));
  history_service_observation_.Reset();
}

bool PasswordProtectionServiceBase::IsWarningEnabled(
    ReusedPasswordAccountType password_type) {
  return GetPasswordProtectionWarningTriggerPref(password_type) ==
         PHISHING_REUSE;
}

// static
ReusedPasswordType
PasswordProtectionServiceBase::GetPasswordProtectionReusedPasswordType(
    password_manager::metrics_util::PasswordType password_type) {
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      return PasswordReuseEvent::SAVED_PASSWORD;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD:
      return PasswordReuseEvent::SIGN_IN_PASSWORD;
    case PasswordType::OTHER_GAIA_PASSWORD:
      return PasswordReuseEvent::OTHER_GAIA_PASSWORD;
    case PasswordType::ENTERPRISE_PASSWORD:
      return PasswordReuseEvent::ENTERPRISE_PASSWORD;
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
      return PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN;
    case PasswordType::PASSWORD_TYPE_COUNT:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN;
}

ReusedPasswordAccountType
PasswordProtectionServiceBase::GetPasswordProtectionReusedPasswordAccountType(
    password_manager::metrics_util::PasswordType password_type,
    const std::string& username) const {
  ReusedPasswordAccountType reused_password_account_type;
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::SAVED_PASSWORD);
      return reused_password_account_type;
    case PasswordType::ENTERPRISE_PASSWORD:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
      return reused_password_account_type;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD: {
      reused_password_account_type.set_is_account_syncing(
          IsPrimaryAccountSyncingHistory());
      if (!IsPrimaryAccountSignedIn()) {
        reused_password_account_type.set_account_type(
            ReusedPasswordAccountType::UNKNOWN);
        return reused_password_account_type;
      }
      reused_password_account_type.set_account_type(
          IsAccountGmail(username) ? ReusedPasswordAccountType::GMAIL
                                   : ReusedPasswordAccountType::GSUITE);
      return reused_password_account_type;
    }
    case PasswordType::OTHER_GAIA_PASSWORD: {
      AccountInfo account_info = GetAccountInfoForUsername(username);
      if (account_info.account_id.empty() ||
          account_info.hosted_domain.empty()) {
        reused_password_account_type.set_account_type(
            ReusedPasswordAccountType::UNKNOWN);
        return reused_password_account_type;
      }
      reused_password_account_type.set_account_type(
          IsAccountGmail(username) ? ReusedPasswordAccountType::GMAIL
                                   : ReusedPasswordAccountType::GSUITE);
      return reused_password_account_type;
    }
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
    case PasswordType::PASSWORD_TYPE_COUNT:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::UNKNOWN);
      return reused_password_account_type;
  }
  NOTREACHED_IN_MIGRATION();
  return reused_password_account_type;
}

// static
PasswordType
PasswordProtectionServiceBase::ConvertReusedPasswordAccountTypeToPasswordType(
    ReusedPasswordAccountType password_type) {
  if (password_type.is_account_syncing()) {
    return PasswordType::PRIMARY_ACCOUNT_PASSWORD;
  } else if (password_type.account_type() ==
             ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    return PasswordType::ENTERPRISE_PASSWORD;
  } else if (password_type.account_type() ==
             ReusedPasswordAccountType::SAVED_PASSWORD) {
    return PasswordType::SAVED_PASSWORD;
  } else if (password_type.account_type() ==
             ReusedPasswordAccountType::UNKNOWN) {
    return PasswordType::PASSWORD_TYPE_UNKNOWN;
  } else {
    return PasswordType::OTHER_GAIA_PASSWORD;
  }
}

bool PasswordProtectionServiceBase::IsSupportedPasswordTypeForPinging(
    PasswordType password_type) const {
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      return true;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD:
      return true;
    case PasswordType::ENTERPRISE_PASSWORD:
      return true;
    case PasswordType::OTHER_GAIA_PASSWORD:
      return true;
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
    case PasswordType::PASSWORD_TYPE_COUNT:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool PasswordProtectionServiceBase::IsSupportedPasswordTypeForModalWarning(
    ReusedPasswordAccountType password_type) const {
  if (password_type.account_type() == ReusedPasswordAccountType::SAVED_PASSWORD)
    return true;

// Currently password reuse warnings are only supported for saved passwords
// and GAIA passwords on Android.
#if BUILDFLAG(IS_ANDROID)
  return password_type.account_type() == ReusedPasswordAccountType::GMAIL;
#else
  if (password_type.account_type() ==
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE)
    return true;

  if (password_type.account_type() != ReusedPasswordAccountType::GMAIL &&
      password_type.account_type() != ReusedPasswordAccountType::GSUITE)
    return false;

  return true;
#endif
}

bool PasswordProtectionServiceBase::CanGetAccessToken() {
  if (!try_token_fetch_ || is_off_the_record_)
    return false;

  // Return true if the primary user account of an ESB user is signed in.
  return pref_service_ && IsEnhancedProtectionEnabled(*pref_service_) &&
         identity_manager_ &&
         safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager_);
}

}  // namespace safe_browsing
