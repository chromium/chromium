// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_split.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using base::Time;

namespace {

// Record a GetHash result.
void RecordGetHashResult(safe_browsing::V4OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4GetHash.Result", result,
      safe_browsing::V4OperationResult::OPERATION_RESULT_MAX);
}

// Record a backoff error count
void RecordBackoffErrorCountResult(size_t count) {
  base::UmaHistogramCounts100("SafeBrowsing.V4GetHash.Result.BackoffErrorCount",
                              count);
}

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // A match in the response had an unexpected THREAT_ENTRY_TYPE.
  UNEXPECTED_THREAT_ENTRY_TYPE_ERROR = 1,

  // A match in the response had an unexpected THREAT_TYPE.
  UNEXPECTED_THREAT_TYPE_ERROR = 2,

  // A match in the response had an unexpected PLATFORM_TYPE.
  UNEXPECTED_PLATFORM_TYPE_ERROR = 3,

  // A match in the response contained no metadata where metadata was
  // expected.
  NO_METADATA_ERROR = 4,

  // A match in the response contained a ThreatType that was inconsistent
  // with the other matches.
  INCONSISTENT_THREAT_TYPE_ERROR = 5,

  // A match in the response contained a metadata, but the metadata is invalid.
  UNEXPECTED_METADATA_VALUE_ERROR = 6,

  // A match in the response had no information in the threat field.
  NO_THREAT_ERROR = 7,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_RESULT_TYPE_MAX = 8,
};

// Record parsing errors of a GetHash result.
void RecordParseGetHashResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4GetHash.Parse.Result", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

// Enumerate full hash cache hits/misses for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum V4FullHashCacheResultType {
  // Full hashes for which there is no cache hit.
  FULL_HASH_CACHE_MISS = 0,

  // Full hashes with a cache hit.
  FULL_HASH_CACHE_HIT = 1,

  // Full hashes with a negative cache hit.
  FULL_HASH_NEGATIVE_CACHE_HIT = 2,

  // Memory space for histograms is determined by the max. ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  FULL_HASH_CACHE_RESULT_MAX
};

// Record a full hash cache hit result.
void RecordV4FullHashCacheResult(V4FullHashCacheResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4GetHash.CacheHit.Result",
                            result_type, FULL_HASH_CACHE_RESULT_MAX);
}

// Enumerate GetHash hits/misses for histogramming purposes. DO NOT CHANGE THE
// ORDERING OF THESE VALUES.
enum V4GetHashCheckResultType {
  // Successful responses which returned no full hashes.
  GET_HASH_CHECK_EMPTY = 0,

  // Successful responses for which one or more of the full hashes matched.
  GET_HASH_CHECK_HIT = 1,

  // Successful responses which weren't empty but have no matches.
  GET_HASH_CHECK_MISS = 2,

  // Memory space for histograms is determined by the max. ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  GET_HASH_CHECK_RESULT_MAX
};

// Record a GetHash hit result.
void RecordV4GetHashCheckResult(V4GetHashCheckResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4GetHash.Check.Result", result_type,
                            GET_HASH_CHECK_RESULT_MAX);
}

const char kPermission[] = "permission";
const char kPhaPatternType[] = "pha_pattern_type";
const char kMalwareThreatType[] = "malware_threat_type";
const char kSePatternType[] = "se_pattern_type";
const char kLanding[] = "LANDING";
const char kDistribution[] = "DISTRIBUTION";
const char kSocialEngineeringAds[] = "SOCIAL_ENGINEERING_ADS";
const char kSocialEngineeringLanding[] = "SOCIAL_ENGINEERING_LANDING";
const char kPhishing[] = "PHISHING";

}  // namespace

namespace safe_browsing {

// The default V4GetHashProtocolManagerFactory.
class V4GetHashProtocolManagerFactoryImpl
    : public V4GetHashProtocolManagerFactory {
 public:
  V4GetHashProtocolManagerFactoryImpl() {}

  V4GetHashProtocolManagerFactoryImpl(
      const V4GetHashProtocolManagerFactoryImpl&) = delete;
  V4GetHashProtocolManagerFactoryImpl& operator=(
      const V4GetHashProtocolManagerFactoryImpl&) = delete;

  ~V4GetHashProtocolManagerFactoryImpl() override {}
  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) override {
    return base::WrapUnique(new V4GetHashProtocolManager(
        url_loader_factory, stores_to_check, config));
  }
};

// ----------------------------------------------------------------

CachedHashPrefixInfo::CachedHashPrefixInfo() {}

CachedHashPrefixInfo::CachedHashPrefixInfo(const CachedHashPrefixInfo& other) =
    default;

CachedHashPrefixInfo::~CachedHashPrefixInfo() {}

// ----------------------------------------------------------------

FullHashCallbackInfo::FullHashCallbackInfo() {}

FullHashCallbackInfo::FullHashCallbackInfo(
    const std::vector<FullHashInfo>& cached_full_hash_infos,
    const std::vector<HashPrefixStr>& prefixes_requested,
    std::unique_ptr<network::SimpleURLLoader> loader,
    const FullHashToStoreAndHashPrefixesMap&
        full_hash_to_store_and_hash_prefixes,
    FullHashCallback callback,
    const base::Time& network_start_time)
    : cached_full_hash_infos(cached_full_hash_infos),
      callback(std::move(callback)),
      loader(std::move(loader)),
      full_hash_to_store_and_hash_prefixes(
          full_hash_to_store_and_hash_prefixes),
      network_start_time(network_start_time),
      prefixes_requested(prefixes_requested) {}

FullHashCallbackInfo::~FullHashCallbackInfo() {}

// ----------------------------------------------------------------

FullHashInfo::FullHashInfo(const FullHashStr& full_hash,
                           const ListIdentifier& list_id,
                           const base::Time& positive_expiry)
    : full_hash(full_hash),
      list_id(list_id),
      positive_expiry(positive_expiry) {}

FullHashInfo::FullHashInfo(const FullHashInfo& other) = default;

FullHashInfo::~FullHashInfo() {}

bool FullHashInfo::operator==(const FullHashInfo& other) const {
  return full_hash == other.full_hash && list_id == other.list_id &&
         positive_expiry == other.positive_expiry && metadata == other.metadata;
}

bool FullHashInfo::operator!=(const FullHashInfo& other) const {
  return !operator==(other);
}

// V4GetHashProtocolManager implementation --------------------------------

// static
V4GetHashProtocolManagerFactory* V4GetHashProtocolManager::factory_ = nullptr;

// static
std::unique_ptr<V4GetHashProtocolManager> V4GetHashProtocolManager::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config) {
  if (!factory_)
    factory_ = new V4GetHashProtocolManagerFactoryImpl();
  return factory_->CreateProtocolManager(url_loader_factory, stores_to_check,
                                         config);
}

// static
void V4GetHashProtocolManager::RegisterFactory(
    std::unique_ptr<V4GetHashProtocolManagerFactory> factory) {
  delete factory_;
  factory_ = factory.release();
}

V4GetHashProtocolManager::V4GetHashProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config)
    : gethash_error_count_(0),
      gethash_back_off_mult_(1),
      next_gethash_time_(Time::FromSecondsSinceUnixEpoch(0)),
      config_(config),
      url_loader_factory_(url_loader_factory),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(!stores_to_check.empty());
  std::set<PlatformType> platform_types;
  std::set<ThreatEntryType> threat_entry_types;
  std::set<ThreatType> threat_types;
  for (const ListIdentifier& store : stores_to_check) {
    platform_types.insert(store.platform_type());
    threat_entry_types.insert(store.threat_entry_type());
    threat_types.insert(store.threat_type());
  }
  platform_types_.assign(platform_types.begin(), platform_types.end());
  threat_entry_types_.assign(threat_entry_types.begin(),
                             threat_entry_types.end());
  threat_types_.assign(threat_types.begin(), threat_types.end());
}

V4GetHashProtocolManager::~V4GetHashProtocolManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void V4GetHashProtocolManager::GetFullHashes(
    const FullHashToStoreAndHashPrefixesMap
        full_hash_to_store_and_hash_prefixes,
    const std::vector<std::string>& list_client_states,
    FullHashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!full_hash_to_store_and_hash_prefixes.empty());

  std::vector<HashPrefixStr> prefixes_to_request;
  std::vector<FullHashInfo> cached_full_hash_infos;
  GetFullHashCachedResults(full_hash_to_store_and_hash_prefixes, Time::Now(),
                           &prefixes_to_request, &cached_full_hash_infos);

  base::UmaHistogramBoolean("SafeBrowsing.V4GetHash.CacheFullyHit",
                            prefixes_to_request.empty());
  if (prefixes_to_request.empty()) {
    // 100% cache hits (positive or negative) so we can call the callback right
    // away.
    std::move(callback).Run(cached_full_hash_infos);
    return;
  }

  // We need to wait the minimum waiting duration, and if we are in backoff,
  // we need to check if we're past the next allowed time. If we are, we can
  // proceed with the request. If not, we are required to return empty results
  // (i.e. just use the results from cache and potentially report an unsafe
  // resource as safe).
  if (clock_->Now() <= next_gethash_time_) {
    if (gethash_error_count_) {
      RecordGetHashResult(V4OperationResult::BACKOFF_ERROR);
      backoff_error_count_++;
    } else {
      RecordGetHashResult(V4OperationResult::MIN_WAIT_DURATION_ERROR);
    }
    std::move(callback).Run(cached_full_hash_infos);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_v4_get_hash", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing detects that a URL might be dangerous based on "
            "its local database, it sends a partial hash of that URL to Google "
            "to verify it before showing a warning to the user. This partial "
            "hash does not expose the URL to Google."
          trigger:
            "When a resource URL matches the local hash-prefix database of "
            "potential threats (malware, phishing etc), and the full-hash "
            "result is not already cached, this will be sent."
          data:
             "The 32-bit hash prefix of any potentially bad URLs. The URLs "
             "themselves are not sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing by unchecking 'Protect you and "
            "your device from dangerous sites' in Chromium settings under "
            "Privacy. The feature is enabled by default."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string req_base64 =
      GetHashRequest(prefixes_to_request, list_client_states);
  GetHashUrlAndHeaders(req_base64, &resource_request->url,
                       &resource_request->headers);

  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* loader = owned_loader.get();
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&V4GetHashProtocolManager::OnURLLoaderComplete,
                     base::Unretained(this), loader));

  pending_hash_requests_[loader] = std::make_unique<FullHashCallbackInfo>(
      cached_full_hash_infos, prefixes_to_request, std::move(owned_loader),
      full_hash_to_store_and_hash_prefixes, std::move(callback), clock_->Now());
  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.V4GetHash.CountOfPrefixes",
                           prefixes_to_request.size());
}

void V4GetHashProtocolManager::GetFullHashesWithApis(
    const GURL& url,
    const std::vector<std::string>& list_client_states,
    ThreatMetadataForApiCallback api_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme));

  std::vector<FullHashStr> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(url.DeprecatedGetOriginAsURL(),
                                         &full_hashes);

  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;
  for (const FullHashStr& full_hash : full_hashes) {
    HashPrefixStr prefix;
    bool result =
        V4ProtocolManagerUtil::FullHashToSmallestHashPrefix(full_hash, &prefix);
    DCHECK(result);
    full_hash_to_store_and_hash_prefixes[full_hash].emplace_back(
        GetChromeUrlApiId(), prefix);
  }

  GetFullHashes(full_hash_to_store_and_hash_prefixes, list_client_states,
                base::BindOnce(&V4GetHashProtocolManager::OnFullHashForApi,
                               base::Unretained(this), std::move(api_callback),
                               full_hashes));
}

void V4GetHashProtocolManager::GetFullHashCachedResults(
    const FullHashToStoreAndHashPrefixesMap&
        full_hash_to_store_and_hash_prefixes,
    const Time& now,
    std::vector<HashPrefixStr>* prefixes_to_request,
    std::vector<FullHashInfo>* cached_full_hash_infos) {
  DCHECK(!full_hash_to_store_and_hash_prefixes.empty());
  DCHECK(prefixes_to_request->empty());
  DCHECK(cached_full_hash_infos->empty());

  // Caching behavior is documented here:
  // https://developers.google.com/safe-browsing/v4/caching#about-caching
  //
  // The cache operates as follows:
  // Lookup:
  //     Case 1: The prefix is in the cache.
  //         Case a: The full hash is in the cache.
  //             Case i : The positive full hash result has not expired.
  //                      The result is unsafe and we do not need to send a new
  //                      request.
  //             Case ii: The positive full hash result has expired.
  //                      We need to send a request for full hashes.
  //         Case b: The full hash is not in the cache.
  //             Case i : The negative cache entry has not expired.
  //                      The result is still safe and we do not need to send a
  //                      new request.
  //             Case ii: The negative cache entry has expired.
  //                      We need to send a request for full hashes.
  //     Case 2: The prefix is not in the cache.
  //             We need to send a request for full hashes.
  //
  // Note on eviction:
  //   CachedHashPrefixInfo entries can be removed from the cache only when
  //   the negative cache expire time and the cache expire time of all full
  //   hash results for that prefix have expired.
  //   Individual full hash results can be removed from the prefix's
  //   cache entry if they expire AND their expire time is after the negative
  //   cache expire time.

  std::unordered_set<HashPrefixStr> unique_prefixes_to_request;
  for (const auto& it : full_hash_to_store_and_hash_prefixes) {
    const FullHashStr& full_hash = it.first;
    const StoreAndHashPrefixes& matched = it.second;
    for (const StoreAndHashPrefix& matched_it : matched) {
      const ListIdentifier& list_id = matched_it.list_id;
      const HashPrefixStr& prefix = matched_it.hash_prefix;
      auto prefix_entry = full_hash_cache_.find(prefix);
      if (prefix_entry != full_hash_cache_.end()) {
        // Case 1.
        const CachedHashPrefixInfo& cached_prefix_info = prefix_entry->second;
        bool found_full_hash = false;
        for (const FullHashInfo& full_hash_info :
             cached_prefix_info.full_hash_infos) {
          if (full_hash_info.full_hash == full_hash &&
              full_hash_info.list_id == list_id) {
            // Case a.
            found_full_hash = true;
            number_of_hits_++;
            if (full_hash_info.positive_expiry > now) {
              // Case i.
              cached_full_hash_infos->push_back(full_hash_info);
              RecordV4FullHashCacheResult(FULL_HASH_CACHE_HIT);
            } else {
              // Case ii.
              unique_prefixes_to_request.insert(prefix);
              RecordV4FullHashCacheResult(FULL_HASH_CACHE_MISS);
            }
            break;
          }
        }

        if (!found_full_hash) {
          // Case b.
          if (cached_prefix_info.negative_expiry > now) {
            // Case i.
            RecordV4FullHashCacheResult(FULL_HASH_NEGATIVE_CACHE_HIT);
          } else {
            // Case ii.
            unique_prefixes_to_request.insert(prefix);
            RecordV4FullHashCacheResult(FULL_HASH_CACHE_MISS);
          }
        }
      } else {
        // Case 2.
        unique_prefixes_to_request.insert(prefix);
        RecordV4FullHashCacheResult(FULL_HASH_CACHE_MISS);
      }
    }
  }

  prefixes_to_request->insert(prefixes_to_request->begin(),
                              unique_prefixes_to_request.begin(),
                              unique_prefixes_to_request.end());
}

std::string V4GetHashProtocolManager::GetHashRequest(
    const std::vector<HashPrefixStr>& prefixes_to_request,
    const std::vector<std::string>& list_client_states) {
  DCHECK(!prefixes_to_request.empty());

  FindFullHashesRequest req;

  V4ProtocolManagerUtil::SetClientInfoFromConfig(req.mutable_client(), config_);

  for (const auto& client_state : list_client_states) {
    req.add_client_states(client_state);
  }

  ThreatInfo* info = req.mutable_threat_info();
  for (const PlatformType p : platform_types_) {
    info->add_platform_types(p);
  }
  for (const ThreatEntryType tet : threat_entry_types_) {
    info->add_threat_entry_types(tet);
  }
  for (const ThreatType tt : threat_types_) {
    info->add_threat_types(tt);
  }
  for (const HashPrefixStr& prefix : prefixes_to_request) {
    info->add_threat_entries()->set_hash(prefix);
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64UrlEncode(req_data, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &req_base64);
  return req_base64;
}

void V4GetHashProtocolManager::GetHashUrlAndHeaders(
    const std::string& req_base64,
    GURL* gurl,
    net::HttpRequestHeaders* headers) const {
  V4ProtocolManagerUtil::GetRequestUrlAndHeaders(req_base64, "fullHashes:find",
                                                 config_, gurl, headers);
}

void V4GetHashProtocolManager::HandleGetHashError(const Time& now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &gethash_error_count_, &gethash_back_off_mult_);
  next_gethash_time_ = now + next;
}

void V4GetHashProtocolManager::OnFullHashForApi(
    ThreatMetadataForApiCallback api_callback,
    const std::vector<FullHashStr>& full_hashes,
    const std::vector<FullHashInfo>& full_hash_infos) {
  ThreatMetadata md;
  for (const FullHashInfo& full_hash_info : full_hash_infos) {
    DCHECK_EQ(GetChromeUrlApiId(), full_hash_info.list_id);
    DCHECK(base::Contains(full_hashes, full_hash_info.full_hash));
    md.api_permissions.insert(full_hash_info.metadata.api_permissions.begin(),
                              full_hash_info.metadata.api_permissions.end());
  }

  std::move(api_callback).Run(md);
}

bool V4GetHashProtocolManager::ParseHashResponse(
    const std::string& response_data,
    std::vector<FullHashInfo>* full_hash_infos,
    Time* negative_cache_expire) {
  FindFullHashesResponse response;

  if (!response.ParseFromString(response_data)) {
    RecordParseGetHashResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  // negative_cache_duration should always be set.
  DCHECK(response.has_negative_cache_duration());

  // Seconds resolution is good enough so we ignore the nanos field.
  *negative_cache_expire =
      clock_->Now() +
      base::Seconds(response.negative_cache_duration().seconds());

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    next_gethash_time_ =
        clock_->Now() +
        base::Seconds(response.minimum_wait_duration().seconds());
  }

  for (const ThreatMatch& match : response.matches()) {
    if (!match.has_platform_type()) {
      RecordParseGetHashResult(UNEXPECTED_PLATFORM_TYPE_ERROR);
      return false;
    }
    if (!match.has_threat_entry_type()) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_ENTRY_TYPE_ERROR);
      return false;
    }
    if (!match.has_threat_type()) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
      return false;
    }
    if (!match.has_threat()) {
      RecordParseGetHashResult(NO_THREAT_ERROR);
      return false;
    }

    ListIdentifier list_id(match.platform_type(), match.threat_entry_type(),
                           match.threat_type());
    if (!base::Contains(platform_types_, list_id.platform_type()) ||
        !base::Contains(threat_entry_types_, list_id.threat_entry_type()) ||
        !base::Contains(threat_types_, list_id.threat_type())) {
      // The server may send a ThreatMatch response for lists that we didn't ask
      // for so ignore those ThreatMatch responses.
      continue;
    }

    base::Time positive_expiry;
    if (match.has_cache_duration()) {
      // Seconds resolution is good enough so we ignore the nanos field.
      positive_expiry =
          clock_->Now() + base::Seconds(match.cache_duration().seconds());
    } else {
      positive_expiry = clock_->Now() - base::Seconds(1);
    }
    FullHashInfo full_hash_info(match.threat().hash(), list_id,
                                positive_expiry);
    ParseMetadata(match, &full_hash_info.metadata);
    TRACE_EVENT2("safe_browsing", "V4GetHashProtocolManager::ParseHashResponse",
                 "threat_type", full_hash_info.list_id.threat_type(),
                 "metadata", full_hash_info.metadata.ToTracedValue());
    full_hash_infos->push_back(full_hash_info);
  }
  return true;
}

// static
void V4GetHashProtocolManager::ParseMetadata(const ThreatMatch& match,
                                             ThreatMetadata* metadata) {
  // Different threat types will handle the metadata differently.
  if (match.threat_type() == API_ABUSE) {
    if (!match.has_platform_type()) {
      RecordParseGetHashResult(UNEXPECTED_PLATFORM_TYPE_ERROR);
      return;
    }

    if (!match.has_threat_entry_metadata()) {
      RecordParseGetHashResult(NO_METADATA_ERROR);
      return;
    }
    // For API Abuse, store a list of the returned permissions.
    for (const ThreatEntryMetadata::MetadataEntry& m :
         match.threat_entry_metadata().entries()) {
      if (m.key() != kPermission) {
        RecordParseGetHashResult(UNEXPECTED_METADATA_VALUE_ERROR);
        return;
      }
      metadata->api_permissions.insert(m.value());
    }
  } else if (match.threat_type() == MALWARE_THREAT ||
             match.threat_type() == POTENTIALLY_HARMFUL_APPLICATION) {
    for (const ThreatEntryMetadata::MetadataEntry& m :
         match.threat_entry_metadata().entries()) {
      if (m.key() == kPhaPatternType || m.key() == kMalwareThreatType) {
        if (m.value() == kLanding) {
          metadata->threat_pattern_type = ThreatPatternType::MALWARE_LANDING;
          break;
        } else if (m.value() == kDistribution) {
          metadata->threat_pattern_type =
              ThreatPatternType::MALWARE_DISTRIBUTION;
          break;
        } else {
          RecordParseGetHashResult(UNEXPECTED_METADATA_VALUE_ERROR);
          return;
        }
      }
    }
  } else if (match.threat_type() == SOCIAL_ENGINEERING) {
    for (const ThreatEntryMetadata::MetadataEntry& m :
         match.threat_entry_metadata().entries()) {
      if (m.key() == kSePatternType) {
        if (m.value() == kSocialEngineeringAds) {
          metadata->threat_pattern_type =
              ThreatPatternType::SOCIAL_ENGINEERING_ADS;
          break;
        } else if (m.value() == kSocialEngineeringLanding) {
          metadata->threat_pattern_type =
              ThreatPatternType::SOCIAL_ENGINEERING_LANDING;
          break;
        } else if (m.value() == kPhishing) {
          metadata->threat_pattern_type = ThreatPatternType::PHISHING;
          break;
        } else {
          RecordParseGetHashResult(UNEXPECTED_METADATA_VALUE_ERROR);
          return;
        }
      }
    }
  } else if (match.threat_type() == SUBRESOURCE_FILTER) {
    for (const ThreatEntryMetadata::MetadataEntry& m :
         match.threat_entry_metadata().entries()) {
      // Anything other than "warn" is interpreted as enforce, which should be
      // more common (and therefore leaves us open to shorten it in the future).
      auto get_enforcement = [](const std::string& value) {
        return value == "warn" ? SubresourceFilterLevel::WARN
                               : SubresourceFilterLevel::ENFORCE;
      };
      if (m.key() == "sf_absv") {
        metadata->subresource_filter_match[SubresourceFilterType::ABUSIVE] =
            get_enforcement(m.value());
      } else if (m.key() == "sf_bas") {
        metadata->subresource_filter_match[SubresourceFilterType::BETTER_ADS] =
            get_enforcement(m.value());
      }
    }
  } else if (match.has_threat_entry_metadata() &&
             match.threat_entry_metadata().entries_size() > 1) {
    RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
  }
}

void V4GetHashProtocolManager::ResetGetHashErrors() {
  gethash_error_count_ = 0;
  gethash_back_off_mult_ = 1;
  backoff_error_count_ = 0;
  next_gethash_time_ = base::Time();
}

void V4GetHashProtocolManager::SetClockForTests(base::Clock* clock) {
  clock_ = clock;
}

void V4GetHashProtocolManager::UpdateCache(
    const std::vector<HashPrefixStr>& prefixes_requested,
    const std::vector<FullHashInfo>& full_hash_infos,
    const Time& negative_cache_expire) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If negative_cache_expire is null, don't cache the results since it's not
  // clear till what time they should be considered valid.
  if (negative_cache_expire.is_null()) {
    return;
  }

  for (const HashPrefixStr& prefix : prefixes_requested) {
    // Create or reset the cached result for this prefix.
    CachedHashPrefixInfo& chpi = full_hash_cache_[prefix];
    chpi.full_hash_infos.clear();
    chpi.negative_expiry = negative_cache_expire;

    for (const FullHashInfo& full_hash_info : full_hash_infos) {
      if (V4ProtocolManagerUtil::FullHashMatchesHashPrefix(
              full_hash_info.full_hash, prefix)) {
        chpi.full_hash_infos.push_back(full_hash_info);
      }
    }
  }
}

void V4GetHashProtocolManager::MergeResults(
    const FullHashToStoreAndHashPrefixesMap&
        full_hash_to_store_and_hash_prefixes,
    const std::vector<FullHashInfo>& full_hash_infos,
    std::vector<FullHashInfo>* merged_full_hash_infos) {
  bool get_hash_hit = false;
  for (const FullHashInfo& fhi : full_hash_infos) {
    auto it = full_hash_to_store_and_hash_prefixes.find(fhi.full_hash);
    if (full_hash_to_store_and_hash_prefixes.end() != it) {
      for (const StoreAndHashPrefix& sahp : it->second) {
        if (fhi.list_id == sahp.list_id) {
          merged_full_hash_infos->push_back(fhi);
          get_hash_hit = true;
          break;
        }
      }
    }
  }

  if (get_hash_hit) {
    RecordV4GetHashCheckResult(GET_HASH_CHECK_HIT);
  } else if (full_hash_infos.empty()) {
    RecordV4GetHashCheckResult(GET_HASH_CHECK_EMPTY);
  } else {
    RecordV4GetHashCheckResult(GET_HASH_CHECK_MISS);
  }
}

// SafeBrowsing request responses are handled here.
void V4GetHashProtocolManager::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  std::string data;
  if (response_body)
    data = *response_body;

  OnURLLoaderCompleteInternal(url_loader, url_loader->NetError(), response_code,
                              data);
}

void V4GetHashProtocolManager::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    int net_error,
    int response_code,
    const std::string& data) {
  auto it = pending_hash_requests_.find(url_loader);
  CHECK(it != pending_hash_requests_.end(), base::NotFatalUntil::M130)
      << "Request not found";
  RecordHttpResponseOrErrorCode("SafeBrowsing.V4GetHash.Network.Result",
                                net_error, response_code);

  std::vector<FullHashInfo> full_hash_infos;
  Time negative_cache_expire;

  if (net_error == net::OK && response_code == net::HTTP_OK) {
    RecordGetHashResult(V4OperationResult::STATUS_200);
    if (gethash_error_count_)
      RecordBackoffErrorCountResult(backoff_error_count_);
    ResetGetHashErrors();
    if (!ParseHashResponse(data, &full_hash_infos, &negative_cache_expire)) {
      full_hash_infos.clear();
      RecordGetHashResult(V4OperationResult::PARSE_ERROR);
    }
  } else if (ErrorIsRetriable(net_error, response_code)) {
    if (net_error != net::OK) {
      RecordGetHashResult(V4OperationResult::RETRIABLE_NETWORK_ERROR);
    } else {
      RecordGetHashResult(V4OperationResult::RETRIABLE_HTTP_ERROR);
    }
  } else {
    HandleGetHashError(clock_->Now());

    DVLOG(1) << "SafeBrowsing GetEncodedFullHashes request for: "
             << url_loader->GetFinalURL() << " failed with error: " << net_error
             << " and response code: " << response_code;

    if (net_error != net::OK) {
      RecordGetHashResult(V4OperationResult::NETWORK_ERROR);
    } else {
      RecordGetHashResult(V4OperationResult::HTTP_ERROR);
    }
  }

  const std::unique_ptr<FullHashCallbackInfo>& fhci = it->second;
  UMA_HISTOGRAM_LONG_TIMES("SafeBrowsing.V4GetHash.Network.Time",
                           clock_->Now() - fhci->network_start_time);
  UpdateCache(fhci->prefixes_requested, full_hash_infos, negative_cache_expire);
  MergeResults(fhci->full_hash_to_store_and_hash_prefixes, full_hash_infos,
               &fhci->cached_full_hash_infos);

  std::move(fhci->callback).Run(fhci->cached_full_hash_infos);

  pending_hash_requests_.erase(it);
}

void V4GetHashProtocolManager::CollectFullHashCacheInfo(
    FullHashCacheInfo* full_hash_cache_info) {
  full_hash_cache_info->set_number_of_hits(number_of_hits_);

  for (const auto& it : full_hash_cache_) {
    FullHashCacheInfo::FullHashCache* full_hash_cache =
        full_hash_cache_info->add_full_hash_cache();
    full_hash_cache->set_hash_prefix(it.first);
    full_hash_cache->mutable_cached_hash_prefix_info()->set_negative_expiry(
        it.second.negative_expiry.InMillisecondsSinceUnixEpoch());

    for (const auto& full_hash_infos_it : it.second.full_hash_infos) {
      FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo*
          full_hash_info = full_hash_cache->mutable_cached_hash_prefix_info()
                               ->add_full_hash_info();
      full_hash_info->set_positive_expiry(
          full_hash_infos_it.positive_expiry.InMillisecondsSinceUnixEpoch());
      full_hash_info->set_full_hash(full_hash_infos_it.full_hash);

      full_hash_info->mutable_list_identifier()->set_platform_type(
          static_cast<int>(full_hash_infos_it.list_id.platform_type()));
      full_hash_info->mutable_list_identifier()->set_threat_entry_type(
          static_cast<int>(full_hash_infos_it.list_id.threat_entry_type()));
      full_hash_info->mutable_list_identifier()->set_threat_type(
          static_cast<int>(full_hash_infos_it.list_id.threat_type()));
    }
  }
}

#ifndef DEBUG
std::ostream& operator<<(std::ostream& os, const FullHashInfo& fhi) {
  os << "{full_hash: " << fhi.full_hash << "; list_id: " << fhi.list_id
     << "; positive_expiry: " << fhi.positive_expiry
     << "; metadata.api_permissions.size(): "
     << fhi.metadata.api_permissions.size() << "}";
  return os;
}
#endif

}  // namespace safe_browsing
