// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 protocol.
//
// The V4GetHashProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get full hash matches from the SB server for the given set of
// hash prefixes.
//
// Design doc: go/design-doc-v4-full-hash-manager

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/proto/webui.pb.h"

class GURL;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

class V4GetHashProtocolManagerFuzzer;

// The matching hash prefixes and corresponding stores, for each full hash
// generated for a given URL.
typedef std::unordered_map<FullHash, StoreAndHashPrefixes>
    FullHashToStoreAndHashPrefixesMap;

// ----------------------------------------------------------------

// All information about a particular full hash i.e. negative TTL, store for
// which it is valid, and metadata associated with that store.
struct FullHashInfo {
 public:
  FullHash full_hash;

  // The list for which this full hash is applicable.
  ListIdentifier list_id;

  // The expiration time of the full hash for a particular store.
  base::Time positive_expiry;

  // Any metadata for this full hash for a particular store.
  ThreatMetadata metadata;

  FullHashInfo(const FullHash& full_hash,
               const ListIdentifier& list_id,
               const base::Time& positive_expiry);
  FullHashInfo(const FullHashInfo& other);
  ~FullHashInfo();

  bool operator==(const FullHashInfo& other) const;
  bool operator!=(const FullHashInfo& other) const;

 private:
  FullHashInfo();
};

// Caches individual response from GETHASH response.
struct CachedHashPrefixInfo {
  // The negative TTL for the hash prefix that leads to this
  // CachedHashPrefixInfo. The client should not send any more requests for that
  // hash prefix until this time.
  base::Time negative_expiry;

  // The list of all full hashes (and related info) that start with a
  // particular hash prefix and are known to be unsafe.
  std::vector<FullHashInfo> full_hash_infos;

  CachedHashPrefixInfo();
  CachedHashPrefixInfo(const CachedHashPrefixInfo& other);
  ~CachedHashPrefixInfo();
};

// Cached full hashes received from the server for the corresponding hash
// prefixes.
typedef std::unordered_map<HashPrefix, CachedHashPrefixInfo> FullHashCache;

// FullHashCallback is invoked when GetFullHashes completes. The parameter is
// the vector of full hash results. If empty, indicates that there were no
// matches, and that the resource is safe.
typedef base::Callback<void(const std::vector<FullHashInfo>&)> FullHashCallback;

// Information needed to update the cache and call the callback to post the
// results.
struct FullHashCallbackInfo {
  FullHashCallbackInfo();
  FullHashCallbackInfo(const std::vector<FullHashInfo>& cached_full_hash_infos,
                       const std::vector<HashPrefix>& prefixes_requested,
                       std::unique_ptr<network::SimpleURLLoader> loader,
                       const FullHashToStoreAndHashPrefixesMap&
                           full_hash_to_store_and_hash_prefixes,
                       const FullHashCallback& callback,
                       const base::Time& network_start_time);
  ~FullHashCallbackInfo();

  // The FullHashInfo objects retrieved from cache. These are merged with the
  // results received from the server before invoking the callback.
  std::vector<FullHashInfo> cached_full_hash_infos;

  // The callback method to call after collecting the full hashes for given
  // hash prefixes.
  FullHashCallback callback;

  // The loader that will return the response from the server. This is stored
  // here as a unique pointer to be able to reason about its lifetime easily.
  std::unique_ptr<network::SimpleURLLoader> loader;

  // The generated full hashes and the corresponding prefixes and the stores in
  // which to look for a full hash match.
  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;

  // Used to measure how long did it take to fetch the full hash response from
  // the server.
  base::Time network_start_time;

  // The prefixes that were requested from the server.
  std::vector<HashPrefix> prefixes_requested;
};

// ----------------------------------------------------------------

class V4GetHashProtocolManagerFactory;

class V4GetHashProtocolManager {
 public:
  // Invoked when GetFullHashesWithApis completes.
  // Parameters:
  //   - The API threat metadata for the given URL.
  typedef base::Callback<void(const ThreatMetadata& md)>
      ThreatMetadataForApiCallback;

  virtual ~V4GetHashProtocolManager();

  // Create an instance of the safe browsing v4 protocol manager.
  static std::unique_ptr<V4GetHashProtocolManager> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config);

  // Makes the passed |factory| the factory used to instantiate
  // a V4GetHashProtocolManager. Useful for tests.
  static void RegisterFactory(
      std::unique_ptr<V4GetHashProtocolManagerFactory> factory);

  // Empties the cache.
  void ClearCache();

  // Retrieve the full hash for a set of prefixes, and invoke the callback
  // argument when the results are retrieved. The callback may be invoked
  // synchronously. |list_client_states| is needed for reporting the current
  // state of the lists on the client; it does not affect the response from the
  // server.
  virtual void GetFullHashes(const FullHashToStoreAndHashPrefixesMap
                                 full_hash_to_matching_hash_prefixes,
                             const std::vector<std::string>& list_client_states,
                             FullHashCallback callback);

  // Retrieve the full hash and API metadata for the origin of |url|, and invoke
  // the callback argument when the results are retrieved. The callback may be
  // invoked synchronously.
  virtual void GetFullHashesWithApis(
      const GURL& url,
      const std::vector<std::string>& list_client_states,
      ThreatMetadataForApiCallback api_callback);

  // Callback when the request completes
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  // Populates the protobuf with the FullHashCache data.
  void CollectFullHashCacheInfo(FullHashCacheInfo* full_hash_cache_info);

 protected:
  // Constructs a V4GetHashProtocolManager that issues network requests using
  // |url_loader_factory|.
  V4GetHashProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config);

 private:
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestGetHashRequest);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestParseHashResponse);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseWrongThreatEntryType);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashThreatPatternType);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseSubresourceFilterMetadata);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseNonPermissionMetadata);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseInconsistentThreatTypes);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingOK);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestResultsNotCachedForNegativeCacheDuration);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingParallelRequests);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, GetCachedResults);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestUpdatesAreMerged);
  friend class V4GetHashProtocolManagerTest;
  friend class V4GetHashProtocolManagerFuzzer;
  friend class V4GetHashProtocolManagerFactoryImpl;

  FullHashCache* full_hash_cache_for_tests() { return &full_hash_cache_; }

  void OnURLLoaderCompleteInternal(network::SimpleURLLoader* url_loader,
                                   int net_error,
                                   int response_code,
                                   const std::string& data);

  // Looks up the cached results for full hashes in
  // |full_hash_to_store_and_hash_prefixes|. Fills |prefixes_to_request| with
  // the prefixes that need to be requested. Fills |cached_full_hash_infos|
  // with the cached results.
  // Note: It is valid for both |prefixes_to_request| and
  // |cached_full_hash_infos| to be empty after this function finishes.
  void GetFullHashCachedResults(
      const FullHashToStoreAndHashPrefixesMap&
          full_hash_to_store_and_hash_prefixes,
      const base::Time& now,
      std::vector<HashPrefix>* prefixes_to_request,
      std::vector<FullHashInfo>* cached_full_hash_infos);

  // Fills a FindFullHashesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  // |prefixes_to_request| is the list of hash prefixes to get full hashes for.
  // |list_client_states| is the client_state of each of the lists being synced.
  std::string GetHashRequest(
      const std::vector<HashPrefix>& prefixes_to_request,
      const std::vector<std::string>& list_client_states);

  void GetHashUrlAndHeaders(const std::string& request_base64,
                            GURL* gurl,
                            net::HttpRequestHeaders* headers) const;

  // Updates internal state for each GetHash response error, assuming that
  // the current time is |now|.
  void HandleGetHashError(const base::Time& now);

  // Merges the results from the cache and the results from the server. The
  // response from the server may include information for full hashes from
  // stores other than those required by this client so it filters out those
  // results that the client did not ask for.
  void MergeResults(const FullHashToStoreAndHashPrefixesMap&
                        full_hash_to_store_and_hash_prefixes,
                    const std::vector<FullHashInfo>& full_hash_infos,
                    std::vector<FullHashInfo>* merged_full_hash_infos);

  // Calls |api_callback| with an object of ThreatMetadata that contains
  // permission API metadata for full hashes in those |full_hash_infos| that
  // have a full hash in |full_hashes|.
  void OnFullHashForApi(const ThreatMetadataForApiCallback& api_callback,
                        const std::vector<FullHash>& full_hashes,
                        const std::vector<FullHashInfo>& full_hash_infos);

  // Parses a FindFullHashesResponse protocol buffer and fills the results in
  // |full_hash_infos| and |negative_cache_expire|. |response_data| is a
  // serialized FindFullHashes protocol buffer. |negative_cache_expire| is the
  // cache expiry time of the hash prefixes that were requested. Returns true if
  // parsing is successful; false otherwise.
  bool ParseHashResponse(const std::string& response_data,
                         std::vector<FullHashInfo>* full_hash_infos,
                         base::Time* negative_cache_expire);

  // Parses the store specific |metadata| information from |match|. Logs errors
  // to UMA if the metadata information was not parsed correctly or was
  // inconsistent with what's expected from that corresponding store.
  static void ParseMetadata(const ThreatMatch& match, ThreatMetadata* metadata);

  // Resets the gethash error counter and multiplier.
  void ResetGetHashErrors();

  // Overrides the clock used to check the time.
  void SetClockForTests(base::Clock* clock);

  // Updates the state of the full hash cache upon receiving a valid response
  // from the server.
  void UpdateCache(const std::vector<HashPrefix>& prefixes_requested,
                   const std::vector<FullHashInfo>& full_hash_infos,
                   const base::Time& negative_cache_expire);

 protected:
  // A cache of full hash results.
  FullHashCache full_hash_cache_;

 private:
  // Map of GetHash requests to parameters which created it.
  using PendingHashRequests =
      std::unordered_map<const network::SimpleURLLoader*,
                         std::unique_ptr<FullHashCallbackInfo>>;

  // The factory that controls the creation of V4GetHashProtocolManager.
  // This is used by tests.
  static V4GetHashProtocolManagerFactory* factory_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t gethash_error_count_;

  // Multiplier for the backoff error after the second.
  size_t gethash_back_off_mult_;

  PendingHashRequests pending_hash_requests_;

  // For v4, the next gethash time is set to the backoff time is the last
  // response was an error, or the minimum wait time if the last response was
  // successful.
  base::Time next_gethash_time_;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Records number of cache hits since the beginning of this session.
  int number_of_hits_ = 0;

  // The clock used to vend times.
  base::Clock* clock_;

  // The following sets represent the combination of lists that we would always
  // request from the server, irrespective of which list we found the hash
  // prefix match in.
  std::vector<PlatformType> platform_types_;
  std::vector<ThreatEntryType> threat_entry_types_;
  std::vector<ThreatType> threat_types_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManager);
};

// Interface of a factory to create V4GetHashProtocolManager.  Useful for tests.
class V4GetHashProtocolManagerFactory {
 public:
  V4GetHashProtocolManagerFactory() {}
  virtual ~V4GetHashProtocolManagerFactory() {}
  virtual std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManagerFactory);
};

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& os, const FullHashInfo& id);
#endif

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_
