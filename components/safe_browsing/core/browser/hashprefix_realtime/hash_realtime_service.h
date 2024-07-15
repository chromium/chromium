// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_

#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom-forward.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
class HttpResponseHeaders;
}

namespace safe_browsing {

using HPRTLookupResponseCallback =
    base::OnceCallback<void(bool, std::optional<SBThreatType>)>;

class OhttpKeyService;
class VerdictCacheManager;

// This class implements the backoff logic, cache logic, and lookup request for
// hash-prefix real-time lookups. For testing purposes, the request is currently
// sent to the Safe Browsing server directly. If the
// SafeBrowsingHashRealTimeOverOhttp flag is enabled, the request will be sent
// to a proxy via OHTTP
// (https://www.ietf.org/archive/id/draft-thomson-http-oblivious-01.html) in
// order to anonymize the source of the requests.
class HashRealTimeService : public KeyedService {
 public:
  // Interface via which a client of this class can surface relevant events in
  // WebUI. All methods must be called on the UI thread.
  class WebUIDelegate {
   public:
    virtual ~WebUIDelegate() = default;

    // Adds the new ping to the set of HPRT lookup pings. The ping consists of:
    //  - |inner_request|: the contents of the encrypted request sent to Safe
    //    Browsing through the relay.
    //  - |ohttp_key|: the key used to encrypt the request.
    //  - |relay_url_spec|: the URL of the relay used to forward the encrypted
    //    request to Safe Browsing.
    // Returns a token that can be used in |AddToHPRTLookupResponses| to
    // correlate a ping and response. If the token is not populated, the
    // response should not be logged.
    virtual std::optional<int> AddToHPRTLookupPings(
        V5::SearchHashesRequest* inner_request,
        std::string relay_url_spec,
        std::string ohttp_key) = 0;

    // Adds the new response to the set of HPRT lookup pings.
    virtual void AddToHPRTLookupResponses(
        int token,
        V5::SearchHashesResponse* response) = 0;
  };
  HashRealTimeService(
      base::RepeatingCallback<network::mojom::NetworkContext*()>
          get_network_context,
      VerdictCacheManager* cache_manager,
      OhttpKeyService* ohttp_key_service,
      WebUIDelegate* webui_delegate);

  HashRealTimeService(const HashRealTimeService&) = delete;
  HashRealTimeService& operator=(const HashRealTimeService&) = delete;

  ~HashRealTimeService() override;

  // Returns whether the |url| is eligible for hash-prefix real-time checks.
  static bool CanCheckUrl(const GURL& url);

  // Start the lookup for |url|, and call |response_callback| on
  // |callback_task_runner| when response is received.
  virtual void StartLookup(
      const GURL& url,
      HPRTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Helper function to return a weak pointer.
  base::WeakPtr<HashRealTimeService> GetWeakPtr();

 private:
  friend class HashRealTimeServiceTest;
  friend class HashRealTimeServiceDirectFetchTest;
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_MissingOhttpKey);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest, TestLookupFailure_NetError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_RetriableNetError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_NetErrorNameNotResolved);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_NetErrorConnectionClosed);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_NetErrorHttpCodeFailure);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_OuterResponseCodeError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_InnerResponseCodeError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_ParseResponse);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_IncorrectFullHashLength);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_MissingCacheDuration);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest, TestBackoffModeSet);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeSet_RetriableError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeNotSet_MissingOhttpKey);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeRespected_FullyCached);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeRespected_NotCached);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_OhttpClientDestructedEarly);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceDirectFetchTest,
                           TestLookupFailure_RetriableNetError);

  constexpr static int kLeastSeverity = std::numeric_limits<int>::max();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OperationOutcome {
    // The lookup was successful.
    kSuccess = 0,
    // Parsing the response to a string failed.
    kParseError = 1,
    // There was no cache duration in the parsed response.
    kNoCacheDurationError = 2,
    // At least one full hash in the parsed response had the wrong length.
    kIncorrectFullHashLengthError = 3,
    // There was a retriable error.
    kRetriableError = 4,
    // There was an error in the network stack.
    kNetworkError = 5,
    // There was an error in the HTTP response code.
    kHttpError = 6,
    // There is a bug in the code leading to a NOTREACHED branch.
    kNotReached = 7,
    // The result was found in the local cache.
    kResultInLocalCache = 8,
    // The service is in backoff mode and therefore cannot perform the request.
    kServiceInBackoffMode = 9,
    // Fetching the OHTTP key needed for the HPRT network request was
    // unsuccessful.
    kOhttpKeyFetchFailed = 10,
    // [Deprecated] There is no OHTTP key service.
    kDeprecatedNoOhttpKeyService = 11,
    kMaxValue = kDeprecatedNoOhttpKeyService,
  };

  // Used only for the return type of the function |DetermineSBThreatInfo|.
  struct SBThreatInfo {
    SBThreatInfo(SBThreatType threat_type, int num_full_hash_matches);
    SBThreatType threat_type;
    int num_full_hash_matches;
  };

  // The purpose of this class is to have a single funnel that all calls back
  // to the lookup initiator use.
  class LookupCompleter {
   public:
    LookupCompleter(
        HPRTLookupResponseCallback response_callback,
        scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner);
    ~LookupCompleter();

    // Sends |is_lookup_successful| and |sb_threat_type| back to the lookup
    // initiator.
    void CompleteLookup(bool is_lookup_successful,
                        std::optional<SBThreatType> sb_threat_type,
                        OperationOutcome operation_outcome);

   private:
    // Used to assert that |CompleteLookup| is only called once.
    bool is_call_complete_ = false;
    // The callback provided by the lookup initiator.
    HPRTLookupResponseCallback response_callback_;
    // Task runner to be used for |response_callback_|.
    scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner_;
  };

  // Helper function for |StartLookup|. Starts the lookup for |url| and calls
  // the callback within |lookup_completer| when a response is received.
  void StartLookupInternal(const GURL& url,
                           std::unique_ptr<LookupCompleter> lookup_completer);

  // Returns the traffic annotation tag that is attached in the Oblivious HTTP
  // request when an OHTTP request is sent.
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTagForOhttp() const;

  // Get the URL that will return a response containing full hashes.
  std::string GetResourceUrl(V5::SearchHashesRequest* request) const;

  // Callback for getting the OHTTP key. Most parameters are used by
  // |OnURLLoaderComplete|, see the description above |OnURLLoaderComplete| for
  // details. |key| is returned from |ohttp_key_service_|.
  void OnGetOhttpKey(std::unique_ptr<V5::SearchHashesRequest> request,
                     const GURL& url,
                     const std::vector<std::string>& hash_prefixes_in_request,
                     std::vector<V5::FullHash> result_full_hashes,
                     base::TimeTicks request_start_time,
                     std::unique_ptr<LookupCompleter> lookup_completer,
                     std::optional<std::string> key);

  // Callback for requests sent via OHTTP. Most parameters are used by
  // |OnURLLoaderComplete|, see the description above |OnURLLoaderComplete| for
  // details. |response_body|, |net_error|, |response_code|, |headers|, and
  // |ohttp_client_destructed_early| are returned from the OHTTP client.
  // |ohttp_key| is sent to the key service.
  void OnOhttpComplete(const GURL& url,
                       const std::vector<std::string>& hash_prefixes_in_request,
                       std::vector<V5::FullHash> result_full_hashes,
                       base::TimeTicks request_start_time,
                       std::unique_ptr<LookupCompleter> lookup_completer,
                       std::string ohttp_key,
                       std::optional<int> webui_delegate_token,
                       const std::optional<std::string>& response_body,
                       int net_error,
                       int response_code,
                       scoped_refptr<net::HttpResponseHeaders> headers,
                       bool ohttp_client_destructed_early);

  // Called when the response from the Safe Browsing V5 remote endpoint is
  // received. This is responsible for parsing the response, determining if
  // there were errors and updating backoff if relevant, caching the results,
  // determining the most severe threat type, and calling the callback.
  //  - |url| is used to match the full hashes in the response with the URL's
  //    full hashes.
  //  - |hash_prefixes_in_request| is used to cache the mapping of the requested
  //    hash prefixes to the results.
  //  - |result_full_hashes| starts out as the initial results from the cache.
  //    This method mutates this parameter to include the results from the
  //    server response as well, and then uses the combined results to determine
  //    the most severe threat type.
  //  - |request_start_time| represents when the request was sent, and is used
  //    for logging.
  //  - |lookup_completer| will be called when the method completes and will
  //    respond to the lookup initiator.
  //  - |response_body| is the unparsed response from the server.
  //  - |net_error| is the net error code from the server.
  //  - |response_code| is the HTTP status code from the server.
  //  - |webui_delegate_token| is used for matching HPRT lookup responses to
  //    pings on chrome://safe-browsing.
  //  - |ohttp_client_destructed_early| represents whether the OHTTP client
  //    used for making the request destructed before its normal callback was
  //    called. This is used only for logging purposes.
  void OnURLLoaderComplete(
      const GURL& url,
      const std::vector<std::string>& hash_prefixes_in_request,
      std::vector<V5::FullHash> result_full_hashes,
      base::TimeTicks request_start_time,
      std::unique_ptr<LookupCompleter> lookup_completer,
      std::unique_ptr<std::string> response_body,
      int net_error,
      int response_code,
      std::optional<int> webui_delegate_token,
      bool ohttp_client_destructed_early);

  // Determines the most severe threat type based on |result_full_hashes|, which
  // contains the merged caching and server response results. The |url| is
  // required in order to filter down |result_full_hashes| to ones that match
  // the |url| full hashes. It also returns the number of full hash matches for
  // logging purposes.
  static SBThreatInfo DetermineSBThreatInfo(
      const GURL& url,
      const std::vector<V5::FullHash>& result_full_hashes);

  // Returns a number representing the severity of the full hash detail. The
  // lower the number, the more severe it is. Severity is used to narrow down to
  // a single threat type to report in cases where there are multiple full hash
  // details.
  static int GetThreatSeverity(const V5::FullHash::FullHashDetail& detail);

  // Returns true if the |detail| is more severe than the
  // |baseline_severity|. Returns false if it's less severe or has equal
  // severity.
  static bool IsHashDetailMoreSevere(const V5::FullHash::FullHashDetail& detail,
                                     int baseline_severity);

  // In addition to attempting to parse the |response_body| as described in the
  // |ParseResponse| function comments, this updates the backoff state depending
  // on the lookup success.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationOutcome>
  ParseResponseAndUpdateBackoff(
      int net_error,
      int http_error,
      std::unique_ptr<std::string> response_body,
      const std::vector<std::string>& requested_hash_prefixes) const;

  // Tries to parse the |response_body| into a |SearchHashesResponse|, and
  // returns either the response proto or an |OperationOutcome| with details on
  // why the parsing was unsuccessful. |requested_hash_prefixes| is used for a
  // sanitization call into |RemoveUnmatchedFullHashes|.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationOutcome>
  ParseResponse(int net_error,
                int http_error,
                std::unique_ptr<std::string> response_body,
                const std::vector<std::string>& requested_hash_prefixes) const;

  // Removes any |FullHash| within the |response| whose hash prefix is not found
  // within |requested_hash_prefixes|. This is not expected to occur, but is
  // handled out of caution.
  void RemoveUnmatchedFullHashes(
      std::unique_ptr<V5::SearchHashesResponse>& response,
      const std::vector<std::string>& requested_hash_prefixes) const;

  // Removes any |FullHashDetail| within the |response| that has invalid
  // |ThreatType| or |ThreatAttribute| enums. This is for forward compatibility,
  // for when the API starts returning new threat types or attributes that the
  // client's version of the code does not support.
  void RemoveFullHashDetailsWithInvalidEnums(
      std::unique_ptr<V5::SearchHashesResponse>& response) const;

  // Returns the hash prefixes for the URL's lookup expressions.
  std::set<std::string> GetHashPrefixesSet(const GURL& url) const;

  // Searches the local cache for the input |hash_prefixes|.
  //  - |out_missing_hash_prefixes| is an output parameter with a list of which
  //    hash prefixes were not found in the cache and need to be requested.
  //  - |out_cached_full_hashes| is an output parameter with a list of unsafe
  //    full hashes that were found in the cache for any of the |hash_prefixes|.
  void SearchCache(std::set<std::string> hash_prefixes,
                   std::vector<std::string>* out_missing_hash_prefixes,
                   std::vector<V5::FullHash>* out_cached_full_hashes) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Fetches the NetworkContext that we use to send requests over OHTTP.
  base::RepeatingCallback<network::mojom::NetworkContext*()>
      get_network_context_;

  // Unowned object used for getting and storing cache entries.
  raw_ptr<VerdictCacheManager, DanglingUntriaged> cache_manager_;

  // Unowned object used for getting OHTTP key.
  raw_ptr<OhttpKeyService> ohttp_key_service_;

  // All pending receivers that are sent via OHTTP but haven't received a
  // response yet.
  mojo::UniqueReceiverSet<network::mojom::ObliviousHttpClient>
      ohttp_client_receivers_;

  // Helper object that manages backoff state.
  std::unique_ptr<BackoffOperator> backoff_operator_;

  // Indicates whether |Shutdown| has been called. If so, |StartLookup| returns
  // early.
  bool is_shutdown_ = false;

  // May be null on certain platforms that don't support
  // chrome://safe-browsing and in unit tests. If non-null, guaranteed to
  // outlive this object by contract.
  raw_ptr<WebUIDelegate> webui_delegate_;

  base::WeakPtrFactory<HashRealTimeService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
