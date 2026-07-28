// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_GET_HASH_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_GET_HASH_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V5 protocol.
// The V5GetHashProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_config.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "net/base/backoff_entry.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class V5SearchHashesCache;

class V5GetHashProtocolManager : public KeyedService {
 public:
  // Enumerate outcomes of a GetHash request for logging purposes.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(V5GetHashOperationOutcome)
  enum class OperationOutcome {
    kSuccess = 0,
    kNetworkError = 1,
    kHttpError = 2,
    kParseError = 3,
    kBackoffError = 4,
    kLocalCacheHit = 5,
    kRetriableError = 6,
    kMaxValue = kRetriableError,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5GetHashOperationOutcome)

  // Holds the threat type and metadata for a full hash match.
  struct ThreatTypeAndMetadata {
    ThreatTypeAndMetadata(SBThreatType threat_type, ThreatMetadata metadata)
        : threat_type(threat_type), metadata(metadata) {}

    // The type of threat identified.
    SBThreatType threat_type;
    // Metadata associated with the threat.
    ThreatMetadata metadata;
  };

  // Callback when GetFullHashes completes.
  // Passes the most severe threat type and the associated threat metadata.
  using FullHashCallback =
      base::OnceCallback<void(SBThreatType threat_type,
                              const ThreatMetadata& metadata)>;

  // Constructs a V5GetHashProtocolManager that issues network requests using
  // `url_loader_factory`.
  //  - `url_loader_factory`: The factory to use for creating URLLoaders.
  //  - `config`: The protocol configuration (used for client info).
  //  - `cache`: The cache to store and retrieve full hash results.
  V5GetHashProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config,
      V5SearchHashesCache* cache);

  V5GetHashProtocolManager(const V5GetHashProtocolManager&) = delete;
  V5GetHashProtocolManager& operator=(const V5GetHashProtocolManager&) = delete;

  ~V5GetHashProtocolManager() override;

  // Retrieve the full hash for a set of prefixes, and invoke the callback
  // argument when the results are retrieved.
  // `full_hash_to_threat_types` maps the full hash string to the expected list
  // of threat types.
  // `callback` is the callback that will be run with the threat type and threat
  // metadata once the check completes.
  virtual void GetFullHashes(
      const std::map<FullHashStr, std::vector<SBThreatType>>&
          full_hash_to_threat_types,
      FullHashCallback callback);

  // KeyedService:
  void Shutdown() override;

  // Returns a WeakPtr to this instance.
  base::WeakPtr<V5GetHashProtocolManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Determines the most severe threat type and metadata from a list of matches.
  // `matches` is the list of full hash matches returned by the server or cache.
  // `full_hash_to_threat_types` maps requested full hashes to the threat types
  // they are being checked against.
  // Returns a ThreatTypeAndMetadata containing the most severe threat type and
  // metadata.
  ThreatTypeAndMetadata DetermineMostSevereThreatForLocalChecks(
      const std::vector<V5::FullHash>& matches,
      const std::map<FullHashStr, std::vector<SBThreatType>>&
          full_hash_to_threat_types) const;

  // Callback when the network request completes.
  //  - `url_loader`: The loader that completed.
  //  - `full_hash_to_threat_types`: The map of requested full hashes to threat
  //    types.
  //  - `requested_prefixes`: The list of prefixes that were requested.
  //  - `cached_full_hashes`: The full hashes that were already in the cache.
  //  - `callback`: The callback to invoke with the results.
  //  - `request_start_time`: The time when the network request was started.
  //  - `response_body`: The response body received from the server.
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::map<FullHashStr, std::vector<SBThreatType>>
                               full_hash_to_threat_types,
                           std::vector<std::string> requested_prefixes,
                           std::vector<V5::FullHash> cached_full_hashes,
                           FullHashCallback callback,
                           base::TimeTicks request_start_time,
                           std::optional<std::string> response_body);

  // Logs the `outcome` to UMA and runs the `callback` with the provided
  // `threat_type` and `metadata`.
  //  - `callback` is the callback to run with the lookup results.
  //  - `outcome` is the operation outcome to log to UMA.
  //  - `threat_type` is the threat type to return to the callback.
  //  - `metadata` is the threat metadata to return to the callback.
  void CompleteLookup(FullHashCallback callback,
                      OperationOutcome outcome,
                      SBThreatType threat_type,
                      const ThreatMetadata& metadata);

  // Parses the response from the Safe Browsing server and updates the backoff
  // entry.
  //  - `net_error` is the network error code from the URL loader.
  //  - `response_code` is the HTTP response code.
  //  - `response_body` is the raw response body.
  //  - `requested_prefixes` are the hash prefixes that were requested from the
  //    server.
  // Returns a `ParseResultSuccess` containing the parsed proto, or
  // `OperationOutcome` representing the parse failure reason if parsing failed.
  base::expected<v5_search_hashes_util::ParseResultSuccess, OperationOutcome>
  ParseResponseAndUpdateBackoff(
      int net_error,
      int response_code,
      const std::string& response_body,
      const std::vector<std::string>& requested_prefixes);

  // Handles the result of a request by updating the backoff entry and, on
  // success, logging the count of skipped requests if backoff recovery
  // occurred.
  void HandleBackoffResult(bool succeeded);

  // In-flight loaders, owned by the protocol manager to ensure they are safely
  // aborted during teardown/Shutdown.
  std::set<std::unique_ptr<network::SimpleURLLoader>, base::UniquePtrComparator>
      pending_loaders_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The config of the client making Pver5 requests.
  const V4ProtocolConfig config_;

  // The shared cache of V5 full hashes.
  raw_ptr<V5SearchHashesCache> cache_;

  // Enforces exponential backoff on requests.
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

  // Number of GetHash attempts skipped due to backoff within the same backoff
  // time window.
  size_t num_requests_skipped_for_backoff_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V5GetHashProtocolManager> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_GET_HASH_PROTOCOL_MANAGER_H_
