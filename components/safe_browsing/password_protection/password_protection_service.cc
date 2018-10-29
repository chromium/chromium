// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_service.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/whitelist_checker_client.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

using content::BrowserThread;
using content::WebContents;
using history::HistoryService;
using password_manager::metrics_util::PasswordType;

namespace safe_browsing {

using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

namespace {

// Keys for storing password protection verdict into a DictionaryValue.
const char kCacheCreationTime[] = "cache_creation_time";
const char kVerdictProto[] = "verdict_proto";
const int kRequestTimeoutMs = 10000;
const char kPasswordProtectionRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/login";
const char kPasswordOnFocusCacheKey[] = "password_on_focus_cache_key";

// Helper function to determine if the given origin matches content settings
// map's patterns.
bool OriginMatchPrimaryPattern(
    const GURL& origin,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern_unused) {
  return ContentSettingsPattern::FromURLNoWildcard(origin) == primary_pattern;
}

// Returns the number of path segments in |cache_expression_path|.
// For example, return 0 for "/", since there is no path after the leading
// slash; return 3 for "/abc/def/gh.html".
size_t GetPathDepth(const std::string& cache_expression_path) {
  return base::SplitString(base::StringPiece(cache_expression_path), "/",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
      .size();
}

// Given a URL of either http or https scheme, return its http://hostname.
// e.g., "https://www.foo.com:80/bar/test.cgi" -> "http://www.foo.com".
GURL GetHostNameWithHTTPScheme(const GURL& url) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  std::string result(url::kHttpScheme);
  result.append(url::kStandardSchemeSeparator).append(url.host());
  return GURL(result);
}

}  // namespace

PasswordProtectionService::PasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    HistoryService* history_service,
    HostContentSettingsMap* host_content_settings_map)
    : stored_verdict_count_password_on_focus_(-1),
      stored_verdict_count_password_entry_(-1),
      database_manager_(database_manager),
      url_loader_factory_(url_loader_factory),
      history_service_observer_(this),
      content_settings_(host_content_settings_map),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (history_service)
    history_service_observer_.Add(history_service);
}

PasswordProtectionService::~PasswordProtectionService() {
  tracker_.TryCancelAll();
  CancelPendingRequests();
  history_service_observer_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

bool PasswordProtectionService::CanGetReputationOfURL(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || net::IsLocalhost(url))
    return false;

  const std::string hostname = url.HostNoBrackets();
  return !net::IsHostnameNonUnique(hostname) &&
         hostname.find('.') != std::string::npos;
}

bool PasswordProtectionService::ShouldShowModalWarning(
    LoginReputationClientRequest::TriggerType trigger_type,
    PasswordReuseEvent::ReusedPasswordType password_type,
    LoginReputationClientResponse::VerdictType verdict_type) {
  if (trigger_type != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
      !IsSupportedPasswordTypeForModalWarning(password_type)) {
    return false;
  }

  // Shows modal warning for sync password reuse only if user's currently logged
  // in.
  if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD &&
      GetSyncAccountType() == PasswordReuseEvent::NOT_SIGNED_IN) {
    return false;
  }

  return (verdict_type == LoginReputationClientResponse::PHISHING ||
          verdict_type == LoginReputationClientResponse::LOW_REPUTATION) &&
         IsWarningEnabled();
}

// We cache both types of pings under the same content settings type (
// CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION). Since UNFAMILIAR_LOGIN_PAGE
// verdicts are only enabled on extended reporting users, we cache them one
// layer lower in the content setting DictionaryValue than PASSWORD_REUSE_EVENT
// verdicts.
// In other words, to cache a PASSWORD_REUSE_EVENT verdict we needs three levels
// of keys: (1) origin, (2) password type, (3) cache expression
// returned in verdict.
// To cache a UNFAMILIAR_LOGIN_PAGE, three levels of keys are used:
// (1) origin, (2) 2nd level key is always |kPasswordOnFocusCacheKey|,
// (3) cache expression.
LoginReputationClientResponse::VerdictType
PasswordProtectionService::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordType password_type,
    LoginReputationClientResponse* out_response) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);

  if (!url.is_valid() || !CanGetReputationOfURL(url))
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

  GURL hostname = GetHostNameWithHTTPScheme(url);
  std::unique_ptr<base::DictionaryValue> cache_dictionary =
      base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));

  if (!cache_dictionary || cache_dictionary->empty())
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

  base::Value* verdict_dictionary = nullptr;
  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    // All UNFAMILIAR_LOGIN_PAGE verdicts (a.k.a password on focus ping)
    // are cached under |kPasswordOnFocusCacheKey|.
    verdict_dictionary = cache_dictionary->FindKey(kPasswordOnFocusCacheKey);
    if (!verdict_dictionary)
      return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  } else {
    verdict_dictionary =
        cache_dictionary->FindKey(base::NumberToString(password_type));
    if (!verdict_dictionary)
      return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  }

  std::vector<std::string> paths;
  GeneratePathVariantsWithoutQuery(url, &paths);
  int max_path_depth = -1;
  LoginReputationClientResponse::VerdictType most_matching_verdict =
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  // For all the verdicts of the same origin, we key them by |cache_expression|.
  // Its corresponding value is a DictionaryValue contains its creation time and
  // the serialized verdict proto.
  for (const auto& item : verdict_dictionary->DictItems()) {
    int verdict_received_time;
    LoginReputationClientResponse verdict;
    // Ignore any entry that we cannot parse. These invalid entries will be
    // cleaned up during shutdown.
    if (!ParseVerdictEntry(&item.second, &verdict_received_time, &verdict))
      continue;
    // Since password protection content settings are keyed by origin, we only
    // need to compare the path part of the cache_expression and the given url.
    std::string cache_expression_path =
        GetCacheExpressionPath(verdict.cache_expression());

    // Finds the most specific match.
    int path_depth = static_cast<int>(GetPathDepth(cache_expression_path));
    if (path_depth > max_path_depth &&
        PathVariantsMatchCacheExpression(paths, cache_expression_path)) {
      max_path_depth = path_depth;
      // If the most matching verdict is expired, set the result to
      // VERDICT_TYPE_UNSPECIFIED.
      most_matching_verdict =
          IsCacheExpired(verdict_received_time, verdict.cache_duration_sec())
              ? LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED
              : verdict.verdict_type();
      out_response->CopyFrom(verdict);
    }
  }
  return most_matching_verdict;
}

void PasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordType password_type,
    LoginReputationClientResponse* verdict,
    const base::Time& receive_time) {
  DCHECK(verdict);
  DCHECK(content_settings_);
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);

  if (!CanGetReputationOfURL(url) || IsIncognito()) {
    return;
  }

  GURL hostname = GetHostNameWithHTTPScheme(url);
  int* stored_verdict_count =
      trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE
          ? &stored_verdict_count_password_on_focus_
          : &stored_verdict_count_password_entry_;
  std::unique_ptr<base::DictionaryValue> cache_dictionary =
      base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));

  if (!cache_dictionary || !cache_dictionary)
    cache_dictionary = std::make_unique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> verdict_entry(
      CreateDictionaryFromVerdict(verdict, receive_time));

  base::Value* verdict_dictionary = nullptr;
  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    // All UNFAMILIAR_LOGIN_PAGE verdicts (a.k.a password on focus ping)
    // are cached under |kPasswordOnFocusCacheKey|.
    verdict_dictionary = cache_dictionary->FindKeyOfType(
        kPasswordOnFocusCacheKey, base::Value::Type::DICTIONARY);
    if (!verdict_dictionary) {
      verdict_dictionary = cache_dictionary->SetKey(
          kPasswordOnFocusCacheKey, base::Value(base::Value::Type::DICTIONARY));
    }
  } else {
    std::string password_type_key = base::NumberToString(password_type);
    verdict_dictionary = cache_dictionary->FindKeyOfType(
        password_type_key, base::Value::Type::DICTIONARY);
    if (!verdict_dictionary) {
      verdict_dictionary = cache_dictionary->SetKey(
          password_type_key, base::Value(base::Value::Type::DICTIONARY));
    }
  }

  // Increases stored verdict count if we haven't seen this cache expression
  // before.
  if (!verdict_dictionary->FindKey(verdict->cache_expression()))
    *stored_verdict_count = GetStoredVerdictCount(trigger_type) + 1;

  // If same cache_expression is already in this verdict_dictionary, we simply
  // override it.
  verdict_dictionary->SetKey(
      verdict->cache_expression(),
      base::Value::FromUniquePtrValue(std::move(verdict_entry)));
  content_settings_->SetWebsiteSettingDefaultScope(
      hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
      std::string(), std::move(cache_dictionary));
}

void PasswordProtectionService::CleanUpExpiredVerdicts() {
  DCHECK(content_settings_);

  if (GetStoredVerdictCount(
          LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) <= 0 &&
      GetStoredVerdictCount(
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT) <= 0)
    return;

  ContentSettingsForOneType password_protection_settings;
  content_settings_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);

  for (const ContentSettingPatternSource& source :
       password_protection_settings) {
    GURL primary_pattern_url = GURL(source.primary_pattern.ToString());
    // Find all verdicts associated with this origin.
    std::unique_ptr<base::DictionaryValue> cache_dictionary =
        base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
            primary_pattern_url, GURL(),
            CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(), nullptr));
    bool has_expired_password_on_focus_entry = RemoveExpiredVerdicts(
        LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
        cache_dictionary.get());
    bool has_expired_password_reuse_entry = RemoveExpiredVerdicts(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        cache_dictionary.get());

    if (cache_dictionary->size() == 0u) {
      content_settings_->ClearSettingsForOneTypeWithPredicate(
          CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, base::Time(),
          base::Time::Max(),
          base::BindRepeating(&OriginMatchPrimaryPattern, primary_pattern_url));
    } else if (has_expired_password_on_focus_entry ||
               has_expired_password_reuse_entry) {
      // Set the website setting of this origin with the updated
      // |cache_dictionary|.
      content_settings_->SetWebsiteSettingDefaultScope(
          primary_pattern_url, GURL(),
          CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
          std::move(cache_dictionary));
    }
  }
}

void PasswordProtectionService::StartRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    ReusedPasswordType reused_password_type,
    const std::vector<std::string>& matching_domains,
    LoginReputationClientRequest::TriggerType trigger_type,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<PasswordProtectionRequest> request(
      new PasswordProtectionRequest(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, reused_password_type, matching_domains,
          trigger_type, password_field_exists, this, GetRequestTimeoutInMS()));

  request->Start();
  pending_requests_.insert(std::move(request));
}

void PasswordProtectionService::MaybeStartPasswordFieldOnFocusRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestOutcome reason;
  if (CanSendPing(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  main_frame_url,
                  PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN, &reason)) {
    StartRequest(web_contents, main_frame_url, password_form_action,
                 password_form_frame_url,
                 PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN,
                 {}, /* matching_domains: not used for this type */
                 LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  }
}

void PasswordProtectionService::MaybeStartProtectedPasswordEntryRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    ReusedPasswordType reused_password_type,
    const std::vector<std::string>& matching_domains,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsSupportedPasswordTypeForPinging(reused_password_type))
    return;

  RequestOutcome reason;
  if (CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  main_frame_url, reused_password_type, &reason)) {
    StartRequest(web_contents, main_frame_url, GURL(), GURL(),
                 reused_password_type, matching_domains,
                 LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 password_field_exists);
  } else {
    MaybeLogPasswordReuseLookupEvent(web_contents, reason, nullptr);
    if (CanShowInterstitial(reason, reused_password_type, main_frame_url))
      ShowInterstitial(web_contents, reused_password_type);
  }
}

bool PasswordProtectionService::CanSendPing(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& main_frame_url,
    ReusedPasswordType password_type,
    RequestOutcome* reason) {
  *reason = RequestOutcome::UNKNOWN;
  if (IsPingingEnabled(trigger_type, password_type, reason) &&
      !IsURLWhitelistedForPasswordEntry(main_frame_url, reason)) {
    return true;
  }
  LogNoPingingReason(trigger_type, *reason, password_type,
                     GetSyncAccountType());
  return false;
}

void PasswordProtectionService::RequestFinished(
    PasswordProtectionRequest* request,
    bool already_cached,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);

  if (response) {
    if (!already_cached) {
      CacheVerdict(request->main_frame_url(), request->trigger_type(),
                   request->reused_password_type(), response.get(),
                   base::Time::Now());
    }
    if (ShouldShowModalWarning(request->trigger_type(),
                               request->reused_password_type(),
                               response->verdict_type())) {
      ShowModalWarning(request->web_contents(), response->verdict_token(),
                       request->reused_password_type());
      request->set_is_modal_warning_showing(true);
    }
  }

  request->HandleDeferredNavigations();

  // Remove request from |pending_requests_| list. If it triggers warning, add
  // it into the !warning_reqeusts_| list.
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

void PasswordProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

scoped_refptr<SafeBrowsingDatabaseManager>
PasswordProtectionService::database_manager() {
  return database_manager_;
}

GURL PasswordProtectionService::GetPasswordProtectionRequestUrl() {
  GURL url(kPasswordProtectionRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  DCHECK(!api_key.empty());
  return url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));
}

int PasswordProtectionService::GetStoredVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  DCHECK(content_settings_);
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  int* stored_verdict_count =
      trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE
          ? &stored_verdict_count_password_on_focus_
          : &stored_verdict_count_password_entry_;
  // If we have already computed this, return its value.
  if (*stored_verdict_count >= 0)
    return *stored_verdict_count;

  ContentSettingsForOneType password_protection_settings;
  content_settings_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);
  stored_verdict_count_password_on_focus_ = 0;
  stored_verdict_count_password_entry_ = 0;
  if (password_protection_settings.empty())
    return 0;

  for (const ContentSettingPatternSource& source :
       password_protection_settings) {
    std::unique_ptr<base::DictionaryValue> cache_dictionary =
        base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
            GURL(source.primary_pattern.ToString()), GURL(),
            CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(), nullptr));
    if (cache_dictionary.get() && !cache_dictionary->empty()) {
      for (const auto& item : cache_dictionary->DictItems()) {
        if (item.first == base::StringPiece(kPasswordOnFocusCacheKey)) {
          stored_verdict_count_password_on_focus_ += item.second.DictSize();
        } else {
          stored_verdict_count_password_entry_ += item.second.DictSize();
        }
      }
    }
  }
  return *stored_verdict_count;
}

int PasswordProtectionService::GetRequestTimeoutInMS() {
  return kRequestTimeoutMs;
}

void PasswordProtectionService::FillUserPopulation(
    LoginReputationClientRequest::TriggerType trigger_type,
    LoginReputationClientRequest* request_proto) {
  ChromeUserPopulation* user_population = request_proto->mutable_population();
  user_population->set_user_population(
      IsExtendedReporting() ? ChromeUserPopulation::EXTENDED_REPORTING
                            : ChromeUserPopulation::SAFE_BROWSING);
  user_population->set_profile_management_status(
      GetProfileManagementStatus(GetBrowserPolicyConnector()));
  user_population->set_is_history_sync_enabled(IsHistorySyncEnabled());
  user_population->set_is_under_advanced_protection(
      IsUnderAdvancedProtection());
  user_population->set_is_incognito(IsIncognito());
}

void PasswordProtectionService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindRepeating(
          &PasswordProtectionService::RemoveContentSettingsOnURLsDeleted,
          GetWeakPtr(), deletion_info.IsAllHistory(),
          deletion_info.deleted_rows()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindRepeating(&PasswordProtectionService::
                              RemoveUnhandledSyncPasswordReuseOnURLsDeleted,
                          GetWeakPtr(), deletion_info.IsAllHistory(),
                          deletion_info.deleted_rows()));
}

void PasswordProtectionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

void PasswordProtectionService::RemoveContentSettingsOnURLsDeleted(
    bool all_history,
    const history::URLRows& deleted_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content_settings_);

  if (all_history) {
    content_settings_->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION);
    stored_verdict_count_password_on_focus_ = 0;
    stored_verdict_count_password_entry_ = 0;
    return;
  }

  // For now, if a URL is deleted from history, we simply remove all the
  // cached verdicts of the same origin. This is a pretty aggressive deletion.
  // We might revisit this logic later to decide if we want to only delete the
  // cached verdict whose cache expression matches this URL.
  for (const history::URLRow& row : deleted_rows) {
    if (!row.url().SchemeIsHTTPOrHTTPS())
      continue;

    GURL url_key = GetHostNameWithHTTPScheme(row.url());
    stored_verdict_count_password_on_focus_ =
        GetStoredVerdictCount(
            LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) -
        GetVerdictCountForURL(
            url_key, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE);
    stored_verdict_count_password_entry_ =
        GetStoredVerdictCount(
            LoginReputationClientRequest::PASSWORD_REUSE_EVENT) -
        GetVerdictCountForURL(
            url_key, LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
    content_settings_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, base::Time(),
        base::Time::Max(),
        base::BindRepeating(&OriginMatchPrimaryPattern, url_key));
  }
}

int PasswordProtectionService::GetVerdictCountForURL(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  std::unique_ptr<base::DictionaryValue> cache_dictionary =
      base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
          url, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
          nullptr));
  if (!cache_dictionary || cache_dictionary->empty())
    return 0;

  int verdict_cnt = 0;
  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    base::Value* password_on_focus_dict = nullptr;
    password_on_focus_dict =
        cache_dictionary->FindKey(kPasswordOnFocusCacheKey);
    verdict_cnt +=
        password_on_focus_dict ? password_on_focus_dict->DictSize() : 0;
  } else {
    for (const auto& item : cache_dictionary->DictItems()) {
      if (item.first == kPasswordOnFocusCacheKey)
        continue;
      verdict_cnt += item.second.DictSize();
    }
  }
  return verdict_cnt;
}

bool PasswordProtectionService::RemoveExpiredVerdicts(
    LoginReputationClientRequest::TriggerType trigger_type,
    base::DictionaryValue* cache_dictionary) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  if (!cache_dictionary || cache_dictionary->empty())
    return false;

  size_t verdicts_removed = 0;
  std::vector<std::string> empty_keys;
  for (auto item : cache_dictionary->DictItems()) {
    if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE &&
        item.first == std::string(kPasswordOnFocusCacheKey)) {
      size_t removed_cnt = RemoveExpiredEntries(&item.second);
      verdicts_removed += removed_cnt;
      stored_verdict_count_password_on_focus_ -= removed_cnt;
    } else {
      size_t removed_cnt = RemoveExpiredEntries(&item.second);
      verdicts_removed += removed_cnt;
      stored_verdict_count_password_entry_ -= removed_cnt;
    }

    if (item.second.DictSize() == 0U)
      empty_keys.push_back(item.first);
  }
  for (const auto& key : empty_keys)
    cache_dictionary->RemoveKey(key);

  return verdicts_removed > 0U;
}

size_t PasswordProtectionService::RemoveExpiredEntries(
    base::Value* verdict_dictionary) {
  std::vector<std::string> expired_keys;
  for (const auto& item : verdict_dictionary->DictItems()) {
    int verdict_received_time;
    LoginReputationClientResponse verdict;
    if (!PasswordProtectionService::ParseVerdictEntry(
            &item.second, &verdict_received_time, &verdict) ||
        PasswordProtectionService::IsCacheExpired(
            verdict_received_time, verdict.cache_duration_sec())) {
      expired_keys.push_back(item.first);
    }
  }

  for (const std::string& key : expired_keys)
    verdict_dictionary->RemoveKey(key);

  return expired_keys.size();
}

// static
bool PasswordProtectionService::ParseVerdictEntry(
    base::Value* verdict_entry,
    int* out_verdict_received_time,
    LoginReputationClientResponse* out_verdict) {
  std::string serialized_verdict_proto;
  if (!verdict_entry || !verdict_entry->is_dict() || !out_verdict)
    return false;
  base::Value* cache_creation_time_value =
      verdict_entry->FindKey(kCacheCreationTime);

  if (!cache_creation_time_value || !cache_creation_time_value->is_int())
    return false;
  *out_verdict_received_time = cache_creation_time_value->GetInt();

  base::Value* verdict_proto_value = verdict_entry->FindKey(kVerdictProto);
  if (!verdict_proto_value || !verdict_proto_value->is_string())
    return false;
  serialized_verdict_proto = verdict_proto_value->GetString();

  return base::Base64Decode(serialized_verdict_proto,
                            &serialized_verdict_proto) &&
         out_verdict->ParseFromString(serialized_verdict_proto);
}

bool PasswordProtectionService::PathVariantsMatchCacheExpression(
    const std::vector<std::string>& generated_paths,
    const std::string& cache_expression_path) {
  return base::ContainsValue(generated_paths, cache_expression_path);
}

bool PasswordProtectionService::IsCacheExpired(int cache_creation_time,
                                               int cache_duration) {
  // Note that we assume client's clock is accurate or almost accurate.
  return base::Time::Now().ToDoubleT() >
         static_cast<double>(cache_creation_time + cache_duration);
}

// Generate path variants of the given URL.
void PasswordProtectionService::GeneratePathVariantsWithoutQuery(
    const GURL& url,
    std::vector<std::string>* paths) {
  std::string canonical_path;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, nullptr, &canonical_path,
                                         nullptr);
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canonical_path,
                                                     std::string(), paths);
}

// Return the path of the cache expression. e.g.:
// "www.google.com"     -> ""
// "www.google.com/abc" -> "/abc"
// "foo.com/foo/bar/"  -> "/foo/bar/"
std::string PasswordProtectionService::GetCacheExpressionPath(
    const std::string& cache_expression) {
  DCHECK(!cache_expression.empty());
  size_t first_slash_pos = cache_expression.find_first_of("/");
  if (first_slash_pos == std::string::npos)
    return "";
  return cache_expression.substr(first_slash_pos);
}

// Convert a LoginReputationClientResponse proto into a DictionaryValue.
std::unique_ptr<base::DictionaryValue>
PasswordProtectionService::CreateDictionaryFromVerdict(
    const LoginReputationClientResponse* verdict,
    const base::Time& receive_time) {
  std::unique_ptr<base::DictionaryValue> result =
      std::make_unique<base::DictionaryValue>();
  result->SetInteger(kCacheCreationTime,
                     static_cast<int>(receive_time.ToDoubleT()));
  std::string serialized_proto(verdict->SerializeAsString());
  // Performs a base64 encoding on the serialized proto.
  base::Base64Encode(serialized_proto, &serialized_proto);
  result->SetString(kVerdictProto, serialized_proto);
  return result;
}

std::unique_ptr<PasswordProtectionNavigationThrottle>
PasswordProtectionService::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsRendererInitiated())
    return nullptr;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  for (scoped_refptr<PasswordProtectionRequest> request : pending_requests_) {
    if (request->web_contents() == web_contents &&
        request->trigger_type() ==
            safe_browsing::LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
        IsSupportedPasswordTypeForModalWarning(
            request->reused_password_type())) {
      return std::make_unique<PasswordProtectionNavigationThrottle>(
          navigation_handle, request, /*is_warning_showing=*/false);
    }
  }

  for (scoped_refptr<PasswordProtectionRequest> request : warning_requests_) {
    if (request->web_contents() == web_contents) {
      return std::make_unique<PasswordProtectionNavigationThrottle>(
          navigation_handle, request, /*is_warning_showing=*/true);
    }
  }
  return nullptr;
}

void PasswordProtectionService::RemoveWarningRequestsByWebContents(
    content::WebContents* web_contents) {
  for (auto it = warning_requests_.begin(); it != warning_requests_.end();) {
    if (it->get()->web_contents() == web_contents)
      it = warning_requests_.erase(it);
    else
      ++it;
  }
}

bool PasswordProtectionService::IsModalWarningShowingInWebContents(
    content::WebContents* web_contents) {
  for (const auto& request : warning_requests_) {
    if (request->web_contents() == web_contents)
      return true;
  }
  return false;
}

bool PasswordProtectionService::IsWarningEnabled() {
  return GetPasswordProtectionWarningTriggerPref() == PHISHING_REUSE;
}

bool PasswordProtectionService::IsEventLoggingEnabled() {
  return !IsIncognito() &&
         GetSyncAccountType() != PasswordReuseEvent::NOT_SIGNED_IN;
}

// static
ReusedPasswordType
PasswordProtectionService::GetPasswordProtectionReusedPasswordType(
    password_manager::metrics_util::PasswordType password_type) {
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      return PasswordReuseEvent::SAVED_PASSWORD;
    case PasswordType::SYNC_PASSWORD:
      return PasswordReuseEvent::SIGN_IN_PASSWORD;
    case PasswordType::OTHER_GAIA_PASSWORD:
      return PasswordReuseEvent::OTHER_GAIA_PASSWORD;
    case PasswordType::ENTERPRISE_PASSWORD:
      return PasswordReuseEvent::ENTERPRISE_PASSWORD;
    case PasswordType::PASSWORD_TYPE_COUNT:
      break;
  }
  NOTREACHED();
  return PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN;
}

bool PasswordProtectionService::IsSupportedPasswordTypeForPinging(
    ReusedPasswordType reused_password_type) const {
  switch (reused_password_type) {
    case PasswordReuseEvent::SAVED_PASSWORD:
      return true;
    case PasswordReuseEvent::SIGN_IN_PASSWORD:
      return GetSyncAccountType() != PasswordReuseEvent::NOT_SIGNED_IN;
    case PasswordReuseEvent::OTHER_GAIA_PASSWORD:
      return false;
    case PasswordReuseEvent::ENTERPRISE_PASSWORD:
      return true;
    case PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN:
      break;
  }
  NOTREACHED();
  return false;
}

bool PasswordProtectionService::IsSupportedPasswordTypeForModalWarning(
    ReusedPasswordType reused_password_type) const {
  return reused_password_type == PasswordReuseEvent::SIGN_IN_PASSWORD ||
         reused_password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD;
}

}  // namespace safe_browsing
