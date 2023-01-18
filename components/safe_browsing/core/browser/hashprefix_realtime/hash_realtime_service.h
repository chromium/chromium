// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

using HPRTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<V5::SearchHashesRequest>)>;

using HPRTLookupResponseCallback =
    base::OnceCallback<void(bool, absl::optional<SBThreatType>)>;

// This class implements the backoff logic and lookup request for hash-prefix
// real-time lookups. For testing purposes, the request is currently sent to the
// Safe Browsing server directly. In the future, it will be sent to a proxy via
// OHTTP.
// TODO(1407283): Update "For testing purposes..." portion of description.
class HashRealTimeService : public KeyedService {
 public:
  explicit HashRealTimeService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  HashRealTimeService(const HashRealTimeService&) = delete;
  HashRealTimeService& operator=(const HashRealTimeService&) = delete;

  ~HashRealTimeService() override;

  // Returns whether the |url| is eligible for hash-prefix real-time checks.
  // It's never eligible if the |request_destination| is not mainframe.
  static bool CanCheckUrl(
      const GURL& url,
      network::mojom::RequestDestination request_destination);

  // Returns true if the lookups are currently in backoff mode due to too many
  // prior errors. If this happens, the checking falls back to hash-based
  // database method.
  virtual bool IsInBackoffMode() const;

  // Start the lookup for |url|, and call |response_callback| on
  // |callback_task_runner| when response is received.
  virtual void StartLookup(
      const GURL& url,
      HPRTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 private:
  friend class HashRealTimeServiceTest;
  constexpr static int kLeastSeverity = std::numeric_limits<int>::max();
  using PendingHPRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, HPRTLookupResponseCallback>;

  // Returns the traffic annotation tag that is attached in the simple URL
  // loader.
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const;

  // Get a resource request with URL, load_flags, credentials mode, and method
  // set.
  std::unique_ptr<network::ResourceRequest> GetResourceRequest(
      std::unique_ptr<V5::SearchHashesRequest> request) const;

  // Called when the response from the Safe Browsing V5 remote endpoint is
  // received. This is responsible for parsing the response, determining if
  // there were errors and updating backoff if relevant, determining the most
  // severe threat type, and calling the callback.
  //  - |url| is used to match the full hashes in the response with the URL's
  //    full hashes.
  //  - |url_loader| is the loader that was used to send the request.
  //  - |response_callback_task_runner| is the callback the original caller
  //    passed through that will be called when the method completes.
  //  - |response_body| is the unparsed response from the server.
  void OnURLLoaderComplete(
      const GURL& url,
      network::SimpleURLLoader* url_loader,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      std::unique_ptr<std::string> response_body);

  // Determines the most severe threat type based on |result_full_hashes|, which
  // contains the server response results. The |url| is required in order to
  // filter down |result_full_hashes| to ones that match the |url| full hashes.
  static SBThreatType DetermineSBThreatType(
      const GURL& url,
      const std::vector<V5::FullHash>& result_full_hashes);

  // Returns a number representing the severity of the threat type. The lower
  // the number, the more severe it is. Severity is used to narrow down to a
  // single threat type to report in cases where there are multiple.
  static int GetThreatSeverity(const V5::ThreatType& threat_type);

  // Returns true if the |threat_type| is more severe than the
  // |baseline_severity|. Returns false if it's less severe or has equal
  // severity.
  static bool IsThreatTypeMoreSevere(const V5::ThreatType& threat_type,
                                     int baseline_severity);

  // In addition to attempting to parse the |response_body| as described in the
  // |ParseResponse| function comments, this updates the backoff state depending
  // on the lookup success.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, bool>
  ParseResponseAndUpdateBackoff(
      int net_error,
      int http_error,
      std::unique_ptr<std::string> response_body) const;

  // Tries to parse the |response_body| into a |SearchHashesResponse|, and
  // returns either the response proto or a bool representing whether the error
  // encountered was retriable.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, bool> ParseResponse(
      int net_error,
      int http_error,
      std::unique_ptr<std::string> response_body) const;

  // Removes any |FullHashDetail| within the |response| that has invalid
  // |ThreatType| or |ThreatAttribute| enums. This is for forward compatibility,
  // for when the API starts returning new threat types or attributes that the
  // client's version of the code does not support.
  void RemoveFullHashDetailsWithInvalidEnums(
      std::unique_ptr<V5::SearchHashesResponse>& response) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // All requests that are sent but haven't received a response yet.
  PendingHPRTLookupRequests pending_requests_;

  // Helper object that manages backoff state.
  std::unique_ptr<BackoffOperator> backoff_operator_;

  base::WeakPtrFactory<HashRealTimeService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
