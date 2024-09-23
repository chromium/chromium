// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/verdict_cache_manager.h"

#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"

namespace safe_browsing {

namespace {

// Keys for storing password protection verdict into a base::Value::Dict.
const char kCacheCreationTime[] = "cache_creation_time";
const char kVerdictProto[] = "verdict_proto";
const char kRealTimeThreatInfoProto[] = "rt_threat_info_proto";
const char kPasswordOnFocusCacheKey[] = "password_on_focus_cache_key";
const char kRealTimeUrlCacheKey[] = "real_time_url_cache_key";
const char kCsdTypeCacheKey[] = "client_side_detection_type_cache_key";

// Command-line flag for caching an artificial unsafe verdict for URL real-time
// lookups.
const char kUnsafeUrlFlag[] = "mark_as_real_time_phishing";

// The maximum number of entries to be removed in a single cleanup. Removing too
// many entries all at once could cause jank.
const int kMaxRemovedEntriesCount = 1000;

// The interval between the construction and the first cleanup is performed.
const int kCleanUpIntervalInitSecond = 120;

// The interval between every cleanup task.
const int kCleanUpIntervalSecond = 1800;

// The longest duration that a cache can be stored. If a cache is stored
// longer than the upper bound, it will be evicted.
const int kCacheDurationUpperBoundSecond = 7 * 24 * 60 * 60;  // 7 days

// The length of a randomly generated page load token.
const int kPageLoadTokenBytes = 32;

// The expiration time of a page load token.
const int kPageLoadTokenExpireMinute = 10;

// A helper class to include all match params. It is used as a centralized
// place to determine if the current cache entry should be considered as a
// match.
struct MatchParams {
  MatchParams() = default;
  bool ShouldMatch() {
    return !is_only_exact_match_allowed || (is_exact_host && is_exact_path);
  }
  // Indicates whether the current cache entry and the url have the same host.
  bool is_exact_host = false;
  // Indicates whether the current cache entry and the url have the same path.
  bool is_exact_path = false;
  // Indicates whether the current cache entry is only applicable for exact
  // match.
  bool is_only_exact_match_allowed = true;
};

// Given a URL of either http or https scheme, return its http://hostname.
// e.g., "https://www.foo.com:80/bar/test.cgi" -> "http://www.foo.com".
GURL GetHostNameWithHTTPScheme(const GURL& url) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  std::string result(url::kHttpScheme);
  result.append(url::kStandardSchemeSeparator).append(url.host());
  return GURL(result);
}
// e.g, ("www.foo.com", "/bar/test.cgi") -> "http://www.foo.com/bar/test/cgi"
GURL GetUrlWithHostAndPath(const std::string& host, const std::string& path) {
  std::string result(url::kHttpScheme);
  result.append(url::kStandardSchemeSeparator).append(host).append(path);
  return GURL(result);
}

// e.g, "www.foo.com/bar/test/cgi" -> "http://www.foo.com"
GURL GetHostNameFromCacheExpression(const std::string& cache_expression) {
  std::string cache_expression_url(url::kHttpScheme);
  cache_expression_url.append(url::kStandardSchemeSeparator)
      .append(cache_expression);
  return GetHostNameWithHTTPScheme(GURL(cache_expression_url));
}

// Convert a Proto object into a base::Value::Dict.
template <class T>
base::Value::Dict CreateDictionaryFromVerdict(const T& verdict,
                                              const base::Time& receive_time,
                                              const char* proto_name) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);
  base::Value::Dict result;
  result.Set(kCacheCreationTime,
             static_cast<int>(receive_time.InSecondsFSinceUnixEpoch()));
  std::string serialized_proto(verdict.SerializeAsString());
  // Performs a base64 encoding on the serialized proto.
  serialized_proto = base::Base64Encode(serialized_proto);
  result.Set(proto_name, serialized_proto);
  return result;
}

template <class T>
base::Value::Dict CreateDictionaryFromVerdict(
    const T& verdict,
    const base::Time& receive_time,
    const char* proto_name,
    const safe_browsing::ClientSideDetectionType csd_type) {
  base::Value::Dict result =
      CreateDictionaryFromVerdict(verdict, receive_time, proto_name);
  result.Set(kCsdTypeCacheKey, static_cast<int>(csd_type));
  return result;
}

// Generate path variants of the given URL.
void GeneratePathVariantsWithoutQuery(const GURL& url,
                                      std::vector<std::string>* paths) {
  std::string canonical_path;
  V4ProtocolManagerUtil::CanonicalizeUrl(
      url, /*canonicalized_hostname=*/nullptr, &canonical_path,
      /*canonicalized_query=*/nullptr);
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canonical_path,
                                                     std::string(), paths);
}

template <class T>
bool ParseVerdictEntry(base::Value* verdict_entry,
                       int* out_verdict_received_time,
                       T* out_verdict,
                       const char* proto_name) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);

  if (!verdict_entry || !verdict_entry->is_dict() || !out_verdict) {
    return false;
  }

  const base::Value::Dict& dict = verdict_entry->GetDict();
  std::optional<int> cache_creation_time = dict.FindInt(kCacheCreationTime);

  if (!cache_creation_time) {
    return false;
  }
  *out_verdict_received_time = cache_creation_time.value();

  const std::string* verdict_proto = dict.FindString(proto_name);
  if (!verdict_proto) {
    return false;
  }

  std::string serialized_proto;
  return base::Base64Decode(*verdict_proto, &serialized_proto) &&
         out_verdict->ParseFromString(serialized_proto);
}

// Return the path of the cache expression. e.g.:
// "www.google.com"     -> ""
// "www.google.com/abc" -> "/abc"
// "foo.com/foo/bar/"  -> "/foo/bar/"
std::string GetCacheExpressionPath(const std::string& cache_expression) {
  DCHECK(!cache_expression.empty());
  size_t first_slash_pos = cache_expression.find_first_of("/");
  if (first_slash_pos == std::string::npos) {
    return "";
  }
  return cache_expression.substr(first_slash_pos);
}

// Returns the number of path segments in |cache_expression_path|.
// For example, return 0 for "/", since there is no path after the leading
// slash; return 3 for "/abc/def/gh.html".
size_t GetPathDepth(const std::string& cache_expression_path) {
  return base::SplitString(std::string_view(cache_expression_path), "/",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
      .size();
}

size_t GetHostDepth(const std::string& hostname) {
  return base::SplitString(std::string_view(hostname), ".",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
      .size();
}

bool PathVariantsMatchCacheExpression(
    const std::vector<std::string>& generated_paths,
    const std::string& cache_expression_path) {
  return base::Contains(generated_paths, cache_expression_path);
}

bool IsCacheExpired(int cache_creation_time, int cache_duration) {
  // Note that we assume client's clock is accurate or almost accurate.
  return base::Time::Now().InSecondsFSinceUnixEpoch() >
         static_cast<double>(cache_creation_time + cache_duration);
}

bool IsCacheOlderThanUpperBound(int cache_creation_time) {
  return base::Time::Now().InSecondsFSinceUnixEpoch() >
         static_cast<double>(cache_creation_time +
                             kCacheDurationUpperBoundSecond);
}

template <class T>
size_t RemoveExpiredEntries(base::Value::Dict& verdict_dictionary,
                            const char* proto_name) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);
  std::vector<std::string> expired_keys;
  for (auto item : verdict_dictionary) {
    int verdict_received_time;
    T verdict;
    if (!ParseVerdictEntry<T>(&item.second, &verdict_received_time, &verdict,
                              proto_name) ||
        IsCacheExpired(verdict_received_time, verdict.cache_duration_sec()) ||
        IsCacheOlderThanUpperBound(verdict_received_time)) {
      expired_keys.push_back(item.first);
    }
  }

  for (const std::string& key : expired_keys) {
    verdict_dictionary.Remove(key);
  }

  return expired_keys.size();
}

std::string GetKeyOfTypeFromTriggerType(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type) {
  return trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE
             ? kPasswordOnFocusCacheKey
             : base::NumberToString(
                   static_cast<std::underlying_type_t<
                       ReusedPasswordAccountType::AccountType>>(
                       password_type.account_type()));
}

// If the verdict doesn't have |cache_expression_match_type| field, always
// interpret it as exact match only.
template <typename T>
bool IsOnlyExactMatchAllowed(T verdict) {
  NOTREACHED_IN_MIGRATION();
  return true;
}
template <>
bool IsOnlyExactMatchAllowed<RTLookupResponse::ThreatInfo>(
    RTLookupResponse::ThreatInfo verdict) {
  return verdict.cache_expression_match_type() ==
         RTLookupResponse::ThreatInfo::EXACT_MATCH;
}
// Always do fuzzy matching for password protection verdicts.
template <>
bool IsOnlyExactMatchAllowed<LoginReputationClientResponse>(
    LoginReputationClientResponse verdict) {
  return false;
}

template <typename T>
std::string GetCacheExpression(T verdict) {
  NOTREACHED_IN_MIGRATION();
  return "";
}

template <>
std::string GetCacheExpression<RTLookupResponse::ThreatInfo>(
    RTLookupResponse::ThreatInfo verdict) {
  return verdict.cache_expression_using_match_type();
}

template <>
std::string GetCacheExpression<LoginReputationClientResponse>(
    LoginReputationClientResponse verdict) {
  return verdict.cache_expression();
}

template <class T>
std::optional<base::Value> GetMostMatchingCachedVerdictEntryWithPathMatching(
    const GURL& url,
    const std::string& type_key,
    scoped_refptr<HostContentSettingsMap> content_settings,
    const ContentSettingsType contents_setting_type,
    const char* proto_name,
    MatchParams match_params) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);

  std::optional<base::Value> result;

  GURL hostname = GetHostNameWithHTTPScheme(url);
  base::Value cache_dictionary_value = content_settings->GetWebsiteSetting(
      hostname, GURL(), contents_setting_type, nullptr);

  if (!cache_dictionary_value.is_dict()) {
    return std::nullopt;
  }

  base::Value::Dict* verdict_dictionary =
      cache_dictionary_value.GetDict().FindDict(type_key);

  if (!verdict_dictionary) {
    return std::nullopt;
  }

  std::vector<std::string> paths;
  GeneratePathVariantsWithoutQuery(url, &paths);

  std::string root_path;
  V4ProtocolManagerUtil::CanonicalizeUrl(
      url, /*canonicalized_hostname*/ nullptr, &root_path,
      /*canonicalized_query*/ nullptr);

  int max_path_depth = -1;
  for (const auto [key, value] : *verdict_dictionary) {
    int verdict_received_time;
    T verdict;
    // Ignore any entry that we cannot parse. These invalid entries will be
    // cleaned up during shutdown.
    if (!ParseVerdictEntry<T>(&value, &verdict_received_time, &verdict,
                              proto_name)) {
      continue;
    }
    // Since verdict content settings are keyed by origin, we only need to
    // compare the path part of the cache_expression and the given url.
    std::string cache_expression_path =
        GetCacheExpressionPath(GetCacheExpression(verdict));

    match_params.is_only_exact_match_allowed = IsOnlyExactMatchAllowed(verdict);
    match_params.is_exact_path = (root_path == cache_expression_path);
    // Finds the most specific match.
    int path_depth = static_cast<int>(GetPathDepth(cache_expression_path));
    if (path_depth > max_path_depth &&
        PathVariantsMatchCacheExpression(paths, cache_expression_path) &&
        match_params.ShouldMatch() &&
        !IsCacheExpired(verdict_received_time, verdict.cache_duration_sec())) {
      max_path_depth = path_depth;
      result = std::move(value);
    }
  }

  return result;
}

template <class T>
std::optional<base::Value>
GetMostMatchingCachedVerdictEntryWithHostAndPathMatching(
    const GURL& url,
    const std::string& type_key,
    scoped_refptr<HostContentSettingsMap> content_settings,
    const ContentSettingsType contents_setting_type,
    const char* proto_name) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);
  std::optional<base::Value> most_matching_verdict;
  MatchParams match_params;

  std::string root_host, root_path;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &root_host, &root_path,
                                         /*canonicalized_query*/ nullptr);
  std::vector<std::string> host_variants;
  V4ProtocolManagerUtil::GenerateHostVariantsToCheck(root_host, &host_variants);
  int max_path_depth = -1;
  for (const auto& host : host_variants) {
    int depth = static_cast<int>(GetHostDepth(host));
    GURL url_to_check = GetUrlWithHostAndPath(host, root_path);
    match_params.is_exact_host = (root_host == host);
    std::optional<base::Value> verdict =
        GetMostMatchingCachedVerdictEntryWithPathMatching<T>(
            url_to_check, type_key, content_settings, contents_setting_type,
            proto_name, match_params);
    if (depth > max_path_depth && verdict && verdict->is_dict()) {
      max_path_depth = depth;
      most_matching_verdict = std::move(verdict);
    }
  }

  return most_matching_verdict;
}

template <class T>
typename T::VerdictType GetVerdictTypeFromMostMatchedCachedVerdict(
    const char* proto_name,
    std::optional<base::Value> verdict_entry,
    T* out_response) {
  DCHECK(proto_name == kVerdictProto || proto_name == kRealTimeThreatInfoProto);

  if (!verdict_entry || !verdict_entry->is_dict()) {
    return T::VERDICT_TYPE_UNSPECIFIED;
  }

  const std::string* verdict_proto_value =
      verdict_entry->GetDict().FindString(proto_name);
  if (verdict_proto_value) {
    std::string serialized_proto = *verdict_proto_value;

    if (base::Base64Decode(serialized_proto, &serialized_proto) &&
        out_response->ParseFromString(serialized_proto)) {
      return out_response->verdict_type();
    } else {
      return T::VERDICT_TYPE_UNSPECIFIED;
    }
  } else {
    return T::VERDICT_TYPE_UNSPECIFIED;
  }
}

bool HasPageLoadTokenExpired(int64_t token_time_msec) {
  return base::Time::Now() -
             base::Time::FromMillisecondsSinceUnixEpoch(token_time_msec) >
         base::Minutes(kPageLoadTokenExpireMinute);
}

}  // namespace

VerdictCacheManager::VerdictCacheManager(
    history::HistoryService* history_service,
    scoped_refptr<HostContentSettingsMap> content_settings,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingSyncObserver> sync_observer)
    : stored_verdict_count_password_on_focus_(std::nullopt),
      stored_verdict_count_password_entry_(std::nullopt),
      stored_verdict_count_real_time_url_check_(std::nullopt),
      content_settings_(content_settings),
      sync_observer_(std::move(sync_observer)) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (!content_settings->IsOffTheRecord()) {
    ScheduleNextCleanUpAfterInterval(base::Seconds(kCleanUpIntervalInitSecond));
  }
  // pref_service can be null in tests.
  if (pref_service) {
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnhanced,
        base::BindRepeating(&VerdictCacheManager::CleanUpAllPageLoadTokens,
                            weak_factory_.GetWeakPtr(),
                            ClearReason::kSafeBrowsingStateChanged));
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnabled,
        base::BindRepeating(&VerdictCacheManager::CleanUpAllPageLoadTokens,
                            weak_factory_.GetWeakPtr(),
                            ClearReason::kSafeBrowsingStateChanged));
  }
  // sync_observer_ can be null in some embedders that don't support sync.
  if (sync_observer_) {
    sync_observer_->ObserveHistorySyncStateChanged(base::BindRepeating(
        &VerdictCacheManager::CleanUpAllPageLoadTokens,
        weak_factory_.GetWeakPtr(), ClearReason::kSyncStateChanged));
  }
  CacheArtificialUnsafeRealTimeUrlVerdictFromSwitch();
  CacheArtificialUnsafePhishGuardVerdictFromSwitch();
  CacheArtificialUnsafeHashRealTimeLookupVerdictFromSwitch();
}

void VerdictCacheManager::Shutdown() {
  CleanUpExpiredVerdicts();
  history_service_observation_.Reset();
  pref_change_registrar_.RemoveAll();
  sync_observer_.reset();

  // Clear references to other KeyedServices.
  content_settings_ = nullptr;

  is_shut_down_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

VerdictCacheManager::~VerdictCacheManager() = default;

void VerdictCacheManager::CachePhishGuardVerdict(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    const LoginReputationClientResponse& verdict,
    const base::Time& receive_time) {
  if (is_shut_down_) {
    return;
  }
  DCHECK(content_settings_);
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);

  GURL hostname = GetHostNameFromCacheExpression(GetCacheExpression(verdict));

  base::Value cache_dictionary_value = content_settings_->GetWebsiteSetting(
      hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION, nullptr);

  base::Value::Dict cache_dictionary =
      cache_dictionary_value.is_dict()
          ? std::move(cache_dictionary_value.GetDict())
          : base::Value::Dict();

  base::Value::Dict verdict_entry(
      CreateDictionaryFromVerdict<LoginReputationClientResponse>(
          verdict, receive_time, kVerdictProto));

  std::string type_key =
      GetKeyOfTypeFromTriggerType(trigger_type, password_type);
  base::Value::Dict* verdict_dictionary = cache_dictionary.FindDict(type_key);
  if (!verdict_dictionary) {
    verdict_dictionary =
        cache_dictionary.Set(type_key, base::Value::Dict())->GetIfDict();
  }

  // Increases stored verdict count if we haven't seen this cache expression
  // before.
  if (!verdict_dictionary->contains(GetCacheExpression(verdict))) {
    std::optional<size_t>* stored_verdict_count =
        trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE
            ? &stored_verdict_count_password_on_focus_
            : &stored_verdict_count_password_entry_;
    *stored_verdict_count = GetStoredPhishGuardVerdictCount(trigger_type) + 1;
  }

  // If same cache_expression is already in this verdict_dictionary, we simply
  // override it.
  verdict_dictionary->Set(GetCacheExpression(verdict),
                          std::move(verdict_entry));
  content_settings_->SetWebsiteSettingDefaultScope(
      hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
      base::Value(std::move(cache_dictionary)));
}

LoginReputationClientResponse::VerdictType
VerdictCacheManager::GetCachedPhishGuardVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    LoginReputationClientResponse* out_response) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  if (is_shut_down_) {
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  }

  std::string type_key =
      GetKeyOfTypeFromTriggerType(trigger_type, password_type);
  std::optional<base::Value> most_matching_verdict =
      GetMostMatchingCachedVerdictEntryWithHostAndPathMatching<
          LoginReputationClientResponse>(
          url, type_key, content_settings_,
          ContentSettingsType::PASSWORD_PROTECTION, kVerdictProto);

  return GetVerdictTypeFromMostMatchedCachedVerdict<
      LoginReputationClientResponse>(
      kVerdictProto, std::move(most_matching_verdict), out_response);
}

size_t VerdictCacheManager::GetStoredPhishGuardVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  if (is_shut_down_) {
    return 0;
  }
  DCHECK(content_settings_);
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  std::optional<size_t>* stored_verdict_count =
      trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE
          ? &stored_verdict_count_password_on_focus_
          : &stored_verdict_count_password_entry_;
  // If we have already computed this, return its value.
  if (stored_verdict_count->has_value()) {
    return stored_verdict_count->value();
  }

  stored_verdict_count_password_on_focus_ = 0;
  stored_verdict_count_password_entry_ = 0;
  for (const ContentSettingPatternSource& source :
       content_settings_->GetSettingsForOneType(
           ContentSettingsType::PASSWORD_PROTECTION)) {
    for (auto item : source.setting_value.GetDict()) {
      if (item.first == std::string_view(kPasswordOnFocusCacheKey)) {
        stored_verdict_count_password_on_focus_.value() +=
            item.second.GetDict().size();
      } else {
        stored_verdict_count_password_entry_.value() +=
            item.second.GetDict().size();
      }
    }
  }
  return stored_verdict_count->value();
}

size_t VerdictCacheManager::GetStoredRealTimeUrlCheckVerdictCount() {
  if (is_shut_down_) {
    return 0;
  }
  // If we have already computed this, return its value.
  if (stored_verdict_count_real_time_url_check_.has_value()) {
    return stored_verdict_count_real_time_url_check_.value();
  }

  stored_verdict_count_real_time_url_check_ = 0;
  for (const ContentSettingPatternSource& source :
       content_settings_->GetSettingsForOneType(
           ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA)) {
    for (auto item : source.setting_value.GetDict()) {
      if (item.first == std::string_view(kRealTimeUrlCacheKey)) {
        stored_verdict_count_real_time_url_check_.value() +=
            item.second.GetDict().size();
      }
    }
  }
  return stored_verdict_count_real_time_url_check_.value();
}

void VerdictCacheManager::CacheRealTimeUrlVerdict(
    const RTLookupResponse& verdict,
    const base::Time& receive_time) {
  if (is_shut_down_) {
    return;
  }
  std::vector<std::string> visited_cache_expressions;
  safe_browsing::ClientSideDetectionType csd_type =
      verdict.client_side_detection_type();

  for (const auto& threat_info : verdict.threat_info()) {
    // If |cache_expression_match_type| is unspecified, ignore this entry.
    if (threat_info.cache_expression_match_type() ==
        RTLookupResponse::ThreatInfo::MATCH_TYPE_UNSPECIFIED) {
      continue;
    }
    std::string cache_expression = GetCacheExpression(threat_info);
    // For the same cache_expression, threat_info is in decreasing order of
    // severity. To avoid lower severity threat being overridden by higher one,
    // only store threat info that is first seen for a cache expression.
    if (base::Contains(visited_cache_expressions, cache_expression)) {
      continue;
    }

    GURL hostname = GetHostNameFromCacheExpression(cache_expression);
    base::Value cache_dictionary_value = content_settings_->GetWebsiteSetting(
        hostname, GURL(), ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
        nullptr);

    base::Value::Dict cache_dictionary =
        cache_dictionary_value.is_dict()
            ? std::move(cache_dictionary_value.GetDict())
            : base::Value::Dict();

    base::Value::Dict* verdict_dictionary =
        cache_dictionary.FindDict(kRealTimeUrlCacheKey);
    if (!verdict_dictionary) {
      verdict_dictionary =
          cache_dictionary.Set(kRealTimeUrlCacheKey, base::Value::Dict())
              ->GetIfDict();
    }

    base::Value::Dict threat_info_entry =
        CreateDictionaryFromVerdict<RTLookupResponse::ThreatInfo>(
            threat_info, receive_time, kRealTimeThreatInfoProto, csd_type);
    // Increases stored verdict count if we haven't seen this cache expression
    // before.
    if (!verdict_dictionary->contains(cache_expression)) {
      stored_verdict_count_real_time_url_check_ =
          GetStoredRealTimeUrlCheckVerdictCount() + 1;
    }

    verdict_dictionary->Set(cache_expression, std::move(threat_info_entry));
    visited_cache_expressions.push_back(cache_expression);

    content_settings_->SetWebsiteSettingDefaultScope(
        hostname, GURL(), ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
        base::Value(std::move(cache_dictionary)));
  }
  base::UmaHistogramCounts10000(
      "SafeBrowsing.RT.CacheManager.RealTimeVerdictCount",
      GetStoredRealTimeUrlCheckVerdictCount());
}

RTLookupResponse::ThreatInfo::VerdictType
VerdictCacheManager::GetCachedRealTimeUrlVerdict(
    const GURL& url,
    RTLookupResponse::ThreatInfo* out_threat_info) {
  if (is_shut_down_) {
    return RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED;
  }

  std::optional<base::Value> most_matching_verdict =
      GetMostMatchingCachedVerdictEntryWithHostAndPathMatching<
          RTLookupResponse::ThreatInfo>(
          url, kRealTimeUrlCacheKey, content_settings_,
          ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
          kRealTimeThreatInfoProto);

  return GetVerdictTypeFromMostMatchedCachedVerdict<
      RTLookupResponse::ThreatInfo>(kRealTimeThreatInfoProto,
                                    std::move(most_matching_verdict),
                                    out_threat_info);
}

safe_browsing::ClientSideDetectionType
VerdictCacheManager::GetCachedRealTimeUrlClientSideDetectionType(
    const GURL& url) {
  if (is_shut_down_) {
    return safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;
  }
  std::optional<base::Value> most_matching_verdict =
      GetMostMatchingCachedVerdictEntryWithHostAndPathMatching<
          RTLookupResponse::ThreatInfo>(
          url, kRealTimeUrlCacheKey, content_settings_,
          ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
          kRealTimeThreatInfoProto);

  if (!most_matching_verdict || !most_matching_verdict->is_dict()) {
    return safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;
  }

  const std::optional<int> cache_client_side_detection_type =
      most_matching_verdict->GetDict().FindInt(kCsdTypeCacheKey);
  if (cache_client_side_detection_type) {
    return static_cast<safe_browsing::ClientSideDetectionType>(
        cache_client_side_detection_type.value());
  } else {
    return safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;
  }
}

ChromeUserPopulation::PageLoadToken VerdictCacheManager::CreatePageLoadToken(
    const GURL& url) {
  std::string hostname = url.host();
  ChromeUserPopulation::PageLoadToken token;
  token.set_token_source(
      ChromeUserPopulation::PageLoadToken::CLIENT_GENERATION);
  token.set_token_time_msec(base::Time::Now().InMillisecondsSinceUnixEpoch());
  token.set_token_value(base::RandBytesAsString(kPageLoadTokenBytes));

  page_load_token_map_[hostname] = token;

  return token;
}

ChromeUserPopulation::PageLoadToken VerdictCacheManager::GetPageLoadToken(
    const GURL& url) {
  std::string hostname = url.host();
  if (!base::Contains(page_load_token_map_, hostname)) {
    return ChromeUserPopulation::PageLoadToken();
  }

  ChromeUserPopulation::PageLoadToken token = page_load_token_map_[hostname];
  bool has_expired = HasPageLoadTokenExpired(token.token_time_msec());
  base::UmaHistogramLongTimes(
      "SafeBrowsing.PageLoadToken.Duration",
      base::Time::Now() -
          base::Time::FromMillisecondsSinceUnixEpoch(token.token_time_msec()));
  base::UmaHistogramBoolean("SafeBrowsing.PageLoadToken.HasExpired",
                            has_expired);
  return has_expired ? ChromeUserPopulation::PageLoadToken() : token;
}

void VerdictCacheManager::CacheHashPrefixRealTimeLookupResults(
    const std::vector<std::string>& requested_hash_prefixes,
    const std::vector<V5::FullHash>& response_full_hashes,
    const V5::Duration& cache_duration) {
  hash_realtime_cache_->CacheSearchHashesResponse(
      requested_hash_prefixes, response_full_hashes, cache_duration);
}

std::unordered_map<std::string, std::vector<V5::FullHash>>
VerdictCacheManager::GetCachedHashPrefixRealTimeLookupResults(
    const std::set<std::string>& hash_prefixes) {
  return hash_realtime_cache_->SearchCache(hash_prefixes);
}

void VerdictCacheManager::ScheduleNextCleanUpAfterInterval(
    base::TimeDelta interval) {
  cleanup_timer_.Stop();
  cleanup_timer_.Start(FROM_HERE, interval, this,
                       &VerdictCacheManager::CleanUpExpiredVerdicts);
}

void VerdictCacheManager::CleanUpExpiredVerdicts() {
  if (is_shut_down_) {
    return;
  }
  DCHECK(content_settings_);
  SCOPED_UMA_HISTOGRAM_TIMER("SafeBrowsing.RT.CacheManager.CleanUpTime");
  CleanUpExpiredPhishGuardVerdicts();
  CleanUpExpiredRealTimeUrlCheckVerdicts();
  CleanUpExpiredPageLoadTokens();
  CleanUpExpiredHashPrefixRealTimeLookupResults();
  ScheduleNextCleanUpAfterInterval(base::Seconds(kCleanUpIntervalSecond));
}

void VerdictCacheManager::CleanUpExpiredPhishGuardVerdicts() {
  if (GetStoredPhishGuardVerdictCount(
          LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) <= 0 &&
      GetStoredPhishGuardVerdictCount(
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT) <= 0) {
    return;
  }

  int removed_count = 0;
  for (ContentSettingPatternSource& source :
       content_settings_->GetSettingsForOneType(
           ContentSettingsType::PASSWORD_PROTECTION)) {
    // Find all verdicts associated with this origin.
    base::Value::Dict cache_dictionary =
        std::move(source.setting_value.GetDict());

    bool has_expired_password_on_focus_entry = RemoveExpiredPhishGuardVerdicts(
        LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, cache_dictionary);
    bool has_expired_password_reuse_entry = RemoveExpiredPhishGuardVerdicts(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT, cache_dictionary);

    if (!cache_dictionary.empty() && !has_expired_password_on_focus_entry &&
        !has_expired_password_reuse_entry) {
      continue;
    }

    // Set the website setting of this origin with the updated
    // |cache_dictionary|.
    content_settings_->SetWebsiteSettingCustomScope(
        source.primary_pattern, source.secondary_pattern,
        ContentSettingsType::PASSWORD_PROTECTION,
        cache_dictionary.empty() ? base::Value()
                                 : base::Value(std::move(cache_dictionary)));

    if ((++removed_count) == kMaxRemovedEntriesCount) {
      return;
    }
  }
}

void VerdictCacheManager::CleanUpExpiredRealTimeUrlCheckVerdicts() {
  if (GetStoredRealTimeUrlCheckVerdictCount() == 0) {
    return;
  }

  int removed_count = 0;
  for (ContentSettingPatternSource& source :
       content_settings_->GetSettingsForOneType(
           ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA)) {
    // Find all verdicts associated with this origin.
    base::Value::Dict cache_dictionary =
        std::move(source.setting_value.GetDict());
    bool has_expired_entry =
        RemoveExpiredRealTimeUrlCheckVerdicts(cache_dictionary);

    if (!cache_dictionary.empty() && !has_expired_entry) {
      continue;
    }

    // Set the website setting of this origin with the updated
    // |cache_dictionary|.
    content_settings_->SetWebsiteSettingCustomScope(
        source.primary_pattern, source.secondary_pattern,
        ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
        cache_dictionary.empty() ? base::Value()
                                 : base::Value(std::move(cache_dictionary)));

    if ((++removed_count) == kMaxRemovedEntriesCount) {
      return;
    }
  }
}

void VerdictCacheManager::CleanUpExpiredPageLoadTokens() {
  base::EraseIf(page_load_token_map_, [&](const auto& hostname_token_pair) {
    ChromeUserPopulation::PageLoadToken token = hostname_token_pair.second;
    return HasPageLoadTokenExpired(token.token_time_msec());
  });
  base::UmaHistogramCounts10000("SafeBrowsing.PageLoadToken.TokenCount",
                                page_load_token_map_.size());
}

void VerdictCacheManager::CleanUpAllPageLoadTokens(ClearReason reason) {
  base::UmaHistogramEnumeration("SafeBrowsing.PageLoadToken.ClearReason",
                                reason);
  page_load_token_map_.clear();
}

void VerdictCacheManager::CleanUpExpiredHashPrefixRealTimeLookupResults() {
  hash_realtime_cache_->ClearExpiredResults();
}

// Overridden from history::HistoryServiceObserver.
void VerdictCacheManager::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindRepeating(
                     &VerdictCacheManager::RemoveContentSettingsOnURLsDeleted,
                     GetWeakPtr(), deletion_info.IsAllHistory(),
                     deletion_info.deleted_rows()));
}

// Overridden from history::HistoryServiceObserver.
void VerdictCacheManager::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_service_observation_.IsObservingSource(history_service));
  history_service_observation_.Reset();
}

void VerdictCacheManager::OnCookiesDeleted() {
  CleanUpAllPageLoadTokens(ClearReason::kCookiesDeleted);
}

bool VerdictCacheManager::RemoveExpiredPhishGuardVerdicts(
    LoginReputationClientRequest::TriggerType trigger_type,
    base::Value::Dict& cache_dictionary) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  if (cache_dictionary.empty()) {
    return false;
  }

  size_t verdicts_removed = 0;
  std::vector<std::string> empty_keys;
  for (auto [key, value] : cache_dictionary) {
    if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE &&
        key == std::string(kPasswordOnFocusCacheKey)) {
      size_t removed_cnt = RemoveExpiredEntries<LoginReputationClientResponse>(
          value.GetDict(), kVerdictProto);
      verdicts_removed += removed_cnt;
      if (stored_verdict_count_password_on_focus_.has_value()) {
        stored_verdict_count_password_on_focus_.value() -= removed_cnt;
      }
    } else {
      size_t removed_cnt = RemoveExpiredEntries<LoginReputationClientResponse>(
          value.GetDict(), kVerdictProto);
      verdicts_removed += removed_cnt;
      if (stored_verdict_count_password_entry_.has_value()) {
        stored_verdict_count_password_entry_.value() -= removed_cnt;
      }
    }

    if (value.GetDict().size() == 0U) {
      empty_keys.push_back(key);
    }
  }
  for (const auto& key : empty_keys) {
    cache_dictionary.Remove(key);
  }

  return verdicts_removed > 0U;
}

bool VerdictCacheManager::RemoveExpiredRealTimeUrlCheckVerdicts(
    base::Value::Dict& cache_dictionary) {
  size_t verdicts_removed = 0;
  std::vector<std::string> empty_keys;
  for (auto [key, value] : cache_dictionary) {
    size_t removed_cnt = RemoveExpiredEntries<RTLookupResponse::ThreatInfo>(
        value.GetDict(), kRealTimeThreatInfoProto);
    verdicts_removed += removed_cnt;
    if (stored_verdict_count_real_time_url_check_.has_value()) {
      stored_verdict_count_real_time_url_check_.value() -= removed_cnt;
    }
    if (value.GetDict().size() == 0U) {
      empty_keys.push_back(key);
    }
  }
  for (const auto& key : empty_keys) {
    cache_dictionary.Remove(key);
  }

  return verdicts_removed > 0U;
}

void VerdictCacheManager::RemoveContentSettingsOnURLsDeleted(
    bool all_history,
    const history::URLRows& deleted_rows) {
  if (is_shut_down_) {
    return;
  }
  DCHECK(content_settings_);

  if (all_history) {
    content_settings_->ClearSettingsForOneType(
        ContentSettingsType::PASSWORD_PROTECTION);
    stored_verdict_count_password_on_focus_ = 0;
    stored_verdict_count_password_entry_ = 0;
    stored_verdict_count_real_time_url_check_ = 0;
    content_settings_->ClearSettingsForOneType(
        ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA);
    return;
  }

  // For now, if a URL is deleted from history, we simply remove all the
  // cached verdicts of the same origin. This is a pretty aggressive deletion.
  // We might revisit this logic later to decide if we want to only delete the
  // cached verdict whose cache expression matches this URL.
  for (const history::URLRow& row : deleted_rows) {
    if (!row.url().SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    GURL url_key = GetHostNameWithHTTPScheme(row.url());
    stored_verdict_count_password_on_focus_ =
        GetStoredPhishGuardVerdictCount(
            LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) -
        GetPhishGuardVerdictCountForURL(
            url_key, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE);
    stored_verdict_count_password_entry_ =
        GetStoredPhishGuardVerdictCount(
            LoginReputationClientRequest::PASSWORD_REUSE_EVENT) -
        GetPhishGuardVerdictCountForURL(
            url_key, LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
    stored_verdict_count_real_time_url_check_ =
        GetStoredRealTimeUrlCheckVerdictCount() -
        GetRealTimeUrlCheckVerdictCountForURL(url_key);
    content_settings_->SetWebsiteSettingDefaultScope(
        url_key, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
        base::Value());
    content_settings_->SetWebsiteSettingDefaultScope(
        url_key, GURL(), ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
        base::Value());
  }
}

size_t VerdictCacheManager::GetPhishGuardVerdictCountForURL(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  base::Value cache_dictionary_value = content_settings_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::PASSWORD_PROTECTION, nullptr);

  if (!cache_dictionary_value.is_dict()) {
    return 0;
  }

  int verdict_cnt = 0;
  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    base::Value::Dict* password_on_focus_dict =
        cache_dictionary_value.GetDict().FindDict(kPasswordOnFocusCacheKey);
    verdict_cnt += password_on_focus_dict ? password_on_focus_dict->size() : 0;
  } else {
    for (auto [key, value] : cache_dictionary_value.GetDict()) {
      if (key == kPasswordOnFocusCacheKey) {
        continue;
      }
      verdict_cnt += value.GetDict().size();
    }
  }
  return verdict_cnt;
}

size_t VerdictCacheManager::GetRealTimeUrlCheckVerdictCountForURL(
    const GURL& url) {
  base::Value cache_dictionary_value = content_settings_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA, nullptr);
  if (!cache_dictionary_value.is_dict()) {
    return 0;
  }
  base::Value* verdict_dictionary =
      cache_dictionary_value.GetDict().Find(kRealTimeUrlCacheKey);
  return verdict_dictionary && verdict_dictionary->is_dict()
             ? verdict_dictionary->GetDict().size()
             : 0;
}

void VerdictCacheManager::CacheArtificialUnsafeRealTimeUrlVerdictFromSwitch() {
  std::string phishing_url_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kUnsafeUrlFlag);
  CacheArtificialRealTimeUrlVerdict(phishing_url_string, /*is_unsafe=*/true);
}

void VerdictCacheManager::CacheArtificialRealTimeUrlVerdict(
    const std::string& url_string,
    bool is_unsafe) {
  if (url_string.empty()) {
    return;
  }

  GURL artificial_url(url_string);
  if (!artificial_url.is_valid()) {
    return;
  }

  has_artificial_cached_url_ = true;

  RTLookupResponse response;
  RTLookupResponse::ThreatInfo* threat_info = response.add_threat_info();
  if (is_unsafe) {
    threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
    threat_info->set_threat_type(
        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  } else {
    threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::SAFE);
  }
  threat_info->set_cache_duration_sec(3000);
  threat_info->set_cache_expression_using_match_type(
      artificial_url.GetContent());
  threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  RemoveContentSettingsOnURLsDeleted(/*all_history=*/false,
                                     {history::URLRow(artificial_url)});
  CacheRealTimeUrlVerdict(response, base::Time::Now());
}

void VerdictCacheManager::CacheArtificialUnsafePhishGuardVerdictFromSwitch() {
  std::string phishing_url_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kArtificialCachedPhishGuardVerdictFlag);
  if (phishing_url_string.empty()) {
    return;
  }

  GURL artificial_unsafe_url(phishing_url_string);
  if (!artificial_unsafe_url.is_valid()) {
    return;
  }

  has_artificial_cached_url_ = true;

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);

  LoginReputationClientResponse verdict;
  verdict.set_verdict_type(LoginReputationClientResponse::PHISHING);
  verdict.set_cache_expression(artificial_unsafe_url.GetContent());
  verdict.set_cache_duration_sec(3000);
  CachePhishGuardVerdict(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         reused_password_account_type, verdict,
                         base::Time::Now());
}

void VerdictCacheManager::CacheArtificialHashRealTimeLookupVerdict(
    const std::string& url_spec,
    bool is_unsafe) {
  if (url_spec.empty()) {
    return;
  }

  GURL artificial_unsafe_url(url_spec);
  if (!artificial_unsafe_url.is_valid()) {
    return;
  }

  has_artificial_cached_url_ = true;

  std::vector<FullHashStr> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(artificial_unsafe_url, &full_hashes);
  std::vector<std::string> hash_prefixes;
  for (const auto& full_hash : full_hashes) {
    auto hash_prefix = hash_realtime_utils::GetHashPrefix(full_hash);
    hash_prefixes.emplace_back(hash_prefix);
  }
  FullHashStr sample_full_hash = full_hashes[0];
  V5::FullHash full_hash_object;
  full_hash_object.set_full_hash(sample_full_hash);
  if (is_unsafe) {
    auto* details = full_hash_object.add_full_hash_details();
    details->set_threat_type(V5::ThreatType::SOCIAL_ENGINEERING);
  }
  V5::Duration cache_duration;
  cache_duration.set_seconds(3000);
  CacheHashPrefixRealTimeLookupResults(hash_prefixes, {full_hash_object},
                                       cache_duration);
}

void VerdictCacheManager::
    CacheArtificialUnsafeHashRealTimeLookupVerdictFromSwitch() {
  std::string phishing_url_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kArtificialCachedHashPrefixRealTimeVerdictFlag);
  CacheArtificialHashRealTimeLookupVerdict(phishing_url_string,
                                           /*is_unsafe=*/true);
}

void VerdictCacheManager::StopCleanUpTimerForTesting() {
  if (cleanup_timer_.IsRunning()) {
    cleanup_timer_.AbandonAndStop();
  }
}

void VerdictCacheManager::SetPageLoadTokenForTesting(
    const GURL& url,
    ChromeUserPopulation::PageLoadToken token) {
  std::string hostname = url.host();
  page_load_token_map_[hostname] = token;
}

// static
bool VerdictCacheManager::has_artificial_cached_url_ = false;
bool VerdictCacheManager::has_artificial_cached_url() {
  return has_artificial_cached_url_;
}
void VerdictCacheManager::ResetHasArtificialCachedUrlForTesting() {
  has_artificial_cached_url_ = false;
}

}  // namespace safe_browsing
