// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
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

class PrefService;

namespace safe_browsing {

// Suffix for metrics when there is no URL lookup service.
constexpr char kNoRealTimeURLLookupService[] = ".None";

using RTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupRequest>, std::string)>;

using RTLookupResponseCallback =
    base::OnceCallback<void(bool, bool, std::unique_ptr<RTLookupResponse>)>;

using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;

class VerdictCacheManager;
class ReferrerChainProvider;

// This base class implements the backoff and cache logic for real time URL
// lookup feature.
class RealTimeUrlLookupServiceBase : public KeyedService {
 public:
  explicit RealTimeUrlLookupServiceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      ReferrerChainProvider* referrer_chain_provider,
      PrefService* pref_service);

  RealTimeUrlLookupServiceBase(const RealTimeUrlLookupServiceBase&) = delete;
  RealTimeUrlLookupServiceBase& operator=(const RealTimeUrlLookupServiceBase&) =
      delete;

  ~RealTimeUrlLookupServiceBase() override;

  // Returns true if |url|'s scheme can be checked.
  static bool CanCheckUrl(const GURL& url);

  // Returns the SBThreatType for a combination of
  // RTLookupResponse::ThreatInfo::ThreatType and
  // RTLookupResponse::ThreatInfo::VerdictType
  static SBThreatType GetSBThreatTypeForRTThreatType(
      RTLookupResponse::ThreatInfo::ThreatType rt_threat_type,
      RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type);

  // Returns true if the real time lookups are currently in backoff mode due to
  // too many prior errors. If this happens, the checking falls back to
  // local hash-based method.
  bool IsInBackoffMode() const;

  // Start the full URL lookup for |url|, call |request_callback| on
  // |callback_task_runner| when request is sent, call |response_callback| on
  // |callback_task_runner| when response is received.
  // Note that |request_callback| is not called if there's a valid entry in the
  // cache for |url|.
  // |last_committed_url| and |is_mainframe| are for obtaining page load token
  // for the request.
  // This function is overridden in unit tests.
  virtual void StartLookup(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // Similar to the function StartLookup above,
  // but to send Protego sampled request specifically.
  virtual void SendSampledRequest(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // Helper function to return a weak pointer.
  base::WeakPtr<RealTimeUrlLookupServiceBase> GetWeakPtr();

  // Returns true if real time URL lookup is enabled. The check is based on
  // pref settings of the associated profile, whether the profile is an off the
  // record profile and the finch flag.
  virtual bool CanPerformFullURLLookup() const = 0;

  // Returns true if this profile has opted-in to check subresource URLs.
  virtual bool CanCheckSubresourceURL() const = 0;

  // Returns whether safe browsing database can be checked when real time URL
  // check is enabled.
  virtual bool CanCheckSafeBrowsingDb() const = 0;

  // Returns whether safe browsing high confidence allowlist can be checked when
  // real time URL check is enabled. This should only be used when
  // CanCheckSafeBrowsingDb() returns true.
  virtual bool CanCheckSafeBrowsingHighConfidenceAllowlist() const = 0;

  // Checks if a sample ping can be sent to Safe Browsing.
  virtual bool CanSendRTSampleRequest() const = 0;

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Suffix for logging metrics.
  virtual std::string GetMetricSuffix() const = 0;

 protected:
  // Fragments, usernames and passwords are removed, because fragments are only
  // used for local navigations and usernames/passwords are too privacy
  // sensitive.
  static GURL SanitizeURL(const GURL& url);

  // Called to send the request to the Safe Browsing backend over the network.
  // It also attached an auth header if |access_token_string| is non-empty.
  void SendRequest(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      const std::string& access_token_string,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      bool is_sampled_report);

 private:
  using PendingRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, RTLookupResponseCallback>;

  // Removes URLs that were recorded before |min_allowed_timestamp|. If
  // |should_remove_subresource_url| is true, also removes subresource URLs.
  static void SanitizeReferrerChainEntries(ReferrerChain* referrer_chain,
                                           double min_allowed_timestamp,
                                           bool should_remove_subresource_url);

  // Returns the endpoint that the URL lookup will be sent to.
  virtual GURL GetRealTimeLookupUrl() const = 0;

  // Returns the traffic annotation tag that is attached in the simple URL
  // loader.
  virtual net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const = 0;

  // Returns true if real time URL lookup with GAIA token is enabled.
  virtual bool CanPerformFullURLLookupWithToken() const = 0;

  // Returns the user gesture limit of the referrer chain.
  virtual int GetReferrerUserGestureLimit() const = 0;

  // Returns true if page load tokens can be attached to requests.
  virtual bool CanSendPageLoadToken() const = 0;

  // Gets access token, called if |CanPerformFullURLLookupWithToken| returns
  // true.
  virtual void GetAccessToken(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) = 0;

  // Called when the response from the server is unauthorized, so child classes
  // can add extra handling when this happens.
  virtual void OnResponseUnauthorized(const std::string& invalid_access_token);

  // Gets a dm token string to be set in a request proto.
  virtual absl::optional<std::string> GetDMTokenString() const = 0;

  // Returns whether real time URL requests should include credentials.
  virtual bool ShouldIncludeCredentials() const = 0;

  // Gets the minimum timestamp allowed for referrer chains.
  virtual double GetMinAllowedTimestampForReferrerChains() const = 0;

  // Called to get cache from |cache_manager|. Returns the cached response if
  // there's a cache hit; nullptr otherwise.
  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url);

  // Called to post a task to store the response in |cache_manager|.
  void MayBeCacheRealTimeUrlVerdict(RTLookupResponse response);

  // Maybe logs protego ping times to preferences. The base class provides this
  // as an empty implementation that subclasses can implement. This method gets
  // called as a part of `SendRequest()`. If |sent_with_token| is true, updates
  // the last ping time of the with-token ping time. Otherwise, updates the last
  // ping time of the without-token ping time.
  virtual void MaybeLogLastProtegoPingTimeToPrefs(bool sent_with_token) {}

  // Get a resource request with URL, load_flags and method set.
  std::unique_ptr<network::ResourceRequest> GetResourceRequest();

  void SendRequestInternal(
      std::unique_ptr<network::ResourceRequest> resource_request,
      const std::string& req_data,
      absl::optional<std::string> access_token_string,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      ChromeUserPopulation::UserPopulation user_population,
      bool is_sampled_report);

  // Called when the response from the real-time lookup remote endpoint is
  // received. |url_loader| is the unowned loader that was used to send the
  // request. |request_start_time| is the time when the request was sent.
  // |response_body| is the response received. |access_token_string| is used for
  // calling |OnResponseUnauthorized| in case the response code is
  // HTTP_UNAUTHORIZED.
  void OnURLLoaderComplete(
      absl::optional<std::string> access_token_string,
      network::SimpleURLLoader* url_loader,
      ChromeUserPopulation::UserPopulation user_population,
      base::TimeTicks request_start_time,
      bool is_sampled_report,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      std::unique_ptr<std::string> response_body);

  // Fills in fields in |RTLookupRequest|.
  std::unique_ptr<RTLookupRequest> FillRequestProto(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      bool is_sampled_report);

  SEQUENCE_CHECKER(sequence_checker_);

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting and storing real time url check cache.
  raw_ptr<VerdictCacheManager, DanglingUntriaged> cache_manager_;

  // All requests that are sent but haven't received a response yet.
  PendingRTLookupRequests pending_requests_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Used to populate the ChromeUserPopulation field in requests.
  base::RepeatingCallback<ChromeUserPopulation()> get_user_population_callback_;

  // Unowned object used to retrieve referrer chains.
  raw_ptr<ReferrerChainProvider, DanglingUntriaged> referrer_chain_provider_;

  // Helper object that manages backoff state.
  std::unique_ptr<BackoffOperator> backoff_operator_;

  friend class RealTimeUrlLookupServiceTest;
  friend class ChromeEnterpriseRealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupServiceBase> weak_factory_{this};

};  // class RealTimeUrlLookupServiceBase

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
