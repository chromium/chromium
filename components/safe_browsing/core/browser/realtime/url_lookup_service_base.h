// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/sessions/core/session_id.h"
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
  // Interface via which a client of this class can surface relevant events in
  // WebUI. All methods must be called on the UI thread.
  class WebUIDelegate {
   public:
    virtual ~WebUIDelegate() = default;

    // Adds the new ping to the set of URT lookup pings. Returns a token that
    // can be used in |AddToURTLookupResponses| to correlate a ping and
    // response.
    virtual int AddToURTLookupPings(const RTLookupRequest request,
                                    const std::string oauth_token) = 0;

    // Adds the new response to the set of URT lookup pings.
    virtual void AddToURTLookupResponses(int webui_token,
                                         const RTLookupResponse response) = 0;
  };

  explicit RealTimeUrlLookupServiceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      ReferrerChainProvider* referrer_chain_provider,
      PrefService* pref_service,
      WebUIDelegate* webui_delegate);

  RealTimeUrlLookupServiceBase(const RealTimeUrlLookupServiceBase&) = delete;
  RealTimeUrlLookupServiceBase& operator=(const RealTimeUrlLookupServiceBase&) =
      delete;

  ~RealTimeUrlLookupServiceBase() override;

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

  // Start the full URL lookup for |url| and call |response_callback|
  // on |callback_task_runner| when response is received.
  // This function is overridden in unit tests.
  virtual void StartLookup(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id);

  // Similar to the function StartLookup above,
  // but to send Protego sampled request specifically.
  virtual void SendSampledRequest(
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id);

  // Helper function to return a weak pointer.
  base::WeakPtr<RealTimeUrlLookupServiceBase> GetWeakPtr();

  // Returns true if real time URL lookup is enabled. The check is based on
  // pref settings of the associated profile, whether the profile is an off the
  // record profile and the finch flag.
  virtual bool CanPerformFullURLLookup() const = 0;

  // Returns true if this profile has opted-in to include subframe URLs in
  // referrer chain.
  virtual bool CanIncludeSubframeUrlInReferrerChain() const = 0;

  // Returns whether safe browsing database can be checked when real time URL
  // check is enabled.
  virtual bool CanCheckSafeBrowsingDb() const = 0;

  // Returns whether safe browsing high confidence allowlist can be checked when
  // real time URL check is enabled. This should only be used when
  // CanCheckSafeBrowsingDb() returns true.
  virtual bool CanCheckSafeBrowsingHighConfidenceAllowlist() const = 0;

  // Checks if a sample ping can be sent to Safe Browsing.
  virtual bool CanSendRTSampleRequest() const = 0;

  // Returns an email address to be attached to lookup requests, or an empty
  // string if none is available.
  virtual std::string GetUserEmail() const = 0;

  // Returns DM Token for the managed browser.
  virtual std::string GetBrowserDMTokenString() const = 0;

  // Returns DM Token for the managed profile.
  virtual std::string GetProfileDMTokenString() const = 0;

  // Returns the client metadata (browser, profile) information. May be
  // nullptr if ClientMetadata is unavailable.
  virtual std::unique_ptr<enterprise_connectors::ClientMetadata>
  GetClientMetadata() const = 0;

  // Returns true if `url`'s scheme can be checked, or if it should be checked
  // anyway because of "EnterpriseRealTimeUrlCheckMode".
  virtual bool CanCheckUrl(const GURL& url) = 0;

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Suffix for logging metrics.
  virtual std::string GetMetricSuffix() const = 0;

  // Fragments, usernames and passwords are removed, because fragments are only
  // used for local navigations and usernames/passwords are too privacy
  // sensitive.  This method is for internal use only but is made public for
  // testing.
  static GURL SanitizeURL(const GURL& url);

 protected:
  // Called to send the request to the Safe Browsing backend over the network.
  // It also attached an auth header if |access_token_string| is non-empty.
  // A request may not get sent if one is already pending for |url|.
  void MaybeSendRequest(
      const GURL& url,
      const std::string& access_token_string,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      bool is_sampled_report,
      SessionID tab_id);

 private:
  class PendingRTLookupRequestData {
   public:
    explicit PendingRTLookupRequestData(
        std::unique_ptr<network::SimpleURLLoader> loader);
    PendingRTLookupRequestData(const PendingRTLookupRequestData&) = delete;
    PendingRTLookupRequestData(PendingRTLookupRequestData&&);
    PendingRTLookupRequestData& operator=(const PendingRTLookupRequestData&) =
        delete;
    PendingRTLookupRequestData& operator=(PendingRTLookupRequestData&&);
    ~PendingRTLookupRequestData();

    // Adds the callback to the internal list if it is not null.
    void AddCallback(RTLookupResponseCallback callback);

    network::SimpleURLLoader* loader() { return loader_.get(); }
    bool has_callbacks() { return !callbacks_.empty(); }
    std::vector<RTLookupResponseCallback> take_callbacks() {
      return std::move(callbacks_);
    }

   private:
    std::unique_ptr<network::SimpleURLLoader> loader_;
    std::vector<RTLookupResponseCallback> callbacks_;
  };

  // The URL used as a key to this map is expected to have been sanitized
  // by a call to SanitizeURL().
  using PendingRTLookupRequests =
      base::flat_map<GURL, PendingRTLookupRequestData>;

  // Removes URLs that were recorded before |min_allowed_timestamp|. If
  // |should_remove_subresource_url| is true, also removes subresource URLs.
  static void SanitizeReferrerChainEntries(
      ReferrerChain* referrer_chain,
      std::optional<base::Time> min_allowed_timestamp,
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
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id) = 0;

  // Called when the response from the server is unauthorized, so child classes
  // can add extra handling when this happens.
  virtual void OnResponseUnauthorized(const std::string& invalid_access_token);

  // Gets a dm token string to be set in a request proto.
  virtual std::optional<std::string> GetDMTokenString() const = 0;

  // Returns whether real time URL requests should include credentials.
  virtual bool ShouldIncludeCredentials() const = 0;

  // Gets the minimum timestamp allowed for referrer chains. Returns nullopt if
  // there is no such restriction.
  virtual std::optional<base::Time> GetMinAllowedTimestampForReferrerChains()
      const = 0;

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

  // Maybe logs to histograms about whether the ping request had a cookie. The
  // base class provides this as an empty implementation that subclasses can
  // implement. `was_first_request` is whether the request was the first request
  // after service instantiation. `sent_with_token` is whether the ping had
  // a token, and is used to determine whether the user was signed in.
  virtual void MaybeLogProtegoPingCookieHistograms(bool request_had_cookie,
                                                   bool was_first_request,
                                                   bool sent_with_token) {}

  // Get a resource request with URL, load_flags and method set.
  std::unique_ptr<network::ResourceRequest> GetResourceRequest();

  void SendRequestInternal(
      const GURL& url,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const std::string& req_data,
      std::optional<std::string> access_token_string,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      ChromeUserPopulation::UserPopulation user_population,
      bool is_sampled_report,
      std::optional<int> webui_token);

  // Called when the response from the real-time lookup remote endpoint is
  // received. |url| is the URL that was looked up and can be used as a key into
  // the |pending_requests_| map. |access_token_string| is used for calling
  // |OnResponseUnauthorized| in case the response code is HTTP_UNAUTHORIZED.
  // |request_start_time| is the time when the request was sent.
  // |response_body| is the response received.
  void OnURLLoaderComplete(
      const GURL& url,
      std::optional<std::string> access_token_string,
      ChromeUserPopulation::UserPopulation user_population,
      base::TimeTicks request_start_time,
      bool is_sampled_report,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      std::optional<int> webui_token,
      std::unique_ptr<std::string> response_body);

  // Fills in fields in |RTLookupRequest|.  |url| is expected to be already
  // sanitized.
  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url,
                                                    bool is_sampled_report,
                                                    SessionID tab_id);

  // Logs |request| and |oauth_token| on any open
  // chrome://safe-browsing pages. Returns a token that can be passed
  // to `LogLookupResponseForToken` to associate a request and
  // response.
  std::optional<int> LogLookupRequest(const RTLookupRequest& request,
                                      const std::string& oauth_token);

  // Logs |response| on any open chrome://safe-browsing pages.
  void LogLookupResponseForToken(std::optional<int> token,
                                 const RTLookupResponse& response);

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

  // Tracks the start time of the first request after service instantiation, for
  // metrics.
  std::optional<base::TimeTicks> first_request_start_time_ = std::nullopt;

  // May be null on certain platforms that don't support chrome://safe-browsing
  // and in unit tests. If non-null, guaranteed to outlive this object by
  // contract.
  raw_ptr<WebUIDelegate> webui_delegate_ = nullptr;

  friend class RealTimeUrlLookupServiceTest;
  friend class ChromeEnterpriseRealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupServiceBase> weak_factory_{this};

};  // class RealTimeUrlLookupServiceBase

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
