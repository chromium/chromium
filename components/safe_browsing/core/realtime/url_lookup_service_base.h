// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {
class SyncService;
}

class PrefService;

namespace safe_browsing {

using RTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupRequest>, std::string)>;

using RTLookupResponseCallback =
    base::OnceCallback<void(bool, bool, std::unique_ptr<RTLookupResponse>)>;

class VerdictCacheManager;

// This base class implements the backoff and cache logic for real time URL
// lookup feature.
class RealTimeUrlLookupServiceBase : public KeyedService {
 public:
  explicit RealTimeUrlLookupServiceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      const ChromeUserPopulation::ProfileManagementStatus&
          profile_management_status,
      bool is_under_advanced_protection,
      bool is_off_the_record);
  ~RealTimeUrlLookupServiceBase() override;

  // Returns true if |url|'s scheme can be checked.
  static bool CanCheckUrl(const GURL& url);

  // Returns the SBThreatType for a given
  // RTLookupResponse::ThreatInfo::ThreatType
  static SBThreatType GetSBThreatTypeForRTThreatType(
      RTLookupResponse::ThreatInfo::ThreatType rt_threat_type);

  // Returns true if the real time lookups are currently in backoff mode due to
  // too many prior errors. If this happens, the checking falls back to
  // local hash-based method.
  bool IsInBackoffMode() const;

  // Start the full URL lookup for |url|, call |request_callback| on the same
  // thread when request is sent, call |response_callback| on the same thread
  // when response is received.
  // Note that |request_callback| is not called if there's a valid entry in the
  // cache for |url|.
  // This function is overridden in unit tests.
  virtual void StartLookup(const GURL& url,
                           RTLookupRequestCallback request_callback,
                           RTLookupResponseCallback response_callback);

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

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 protected:
  // Fragments, usernames and passwords are removed, because fragments are only
  // used for local navigations and usernames/passwords are too privacy
  // sensitive.
  static GURL SanitizeURL(const GURL& url);

  // Called to send the request to the Safe Browsing backend over the network.
  // It also attached an auth header if |access_token_string| has a value.
  void SendRequest(const GURL& url,
                   base::Optional<std::string> access_token_string,
                   RTLookupRequestCallback request_callback,
                   RTLookupResponseCallback response_callback);

 private:
  using PendingRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, RTLookupResponseCallback>;

  // Returns the endpoint that the URL lookup will be sent to.
  static GURL GetRealTimeLookupUrl();

  // Returns the traffic annotation tag that is attached in the simple URL
  // loader.
  virtual net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const = 0;

  // Returns true if real time URL lookup with GAIA token is enabled.
  virtual bool CanPerformFullURLLookupWithToken() const = 0;

  // Gets access token, called if |CanPerformFullURLLookupWithToken| returns
  // true.
  virtual void GetAccessToken(const GURL& url,
                              RTLookupRequestCallback request_callback,
                              RTLookupResponseCallback response_callback) = 0;

  // Gets a dm token string to be set in a request proto.
  virtual base::Optional<std::string> GetDMTokenString() const = 0;

  // Suffix for logging metrics.
  virtual std::string GetMetricSuffix() const = 0;

  // Returns whether real time URL requests should include credentials.
  virtual bool ShouldIncludeCredentials() const = 0;

  // Returns the duration of the next backoff. Starts at
  // |kMinBackOffResetDurationInSeconds| and increases exponentially until
  // it reaches |kMaxBackOffResetDurationInSeconds|.
  size_t GetBackoffDurationInSeconds() const;

  // Called when the request to remote endpoint fails. May initiate or extend
  // backoff.
  void HandleLookupError();

  // Called when the request to remote endpoint succeeds. Resets error count and
  // ends backoff.
  void HandleLookupSuccess();

  // Resets the error count and ends backoff mode. Functionally same as
  // |HandleLookupSuccess| for now.
  void ResetFailures();

  // Called to get cache from |cache_manager|. Returns the cached response if
  // there's a cache hit; nullptr otherwise.
  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url);

  // Called to post a task to store the response keyed by the |url| in
  // |cache_manager|.
  void MayBeCacheRealTimeUrlVerdict(const GURL& url, RTLookupResponse response);

  // Get a resource request with URL, load_flags and method set.
  std::unique_ptr<network::ResourceRequest> GetResourceRequest();

  void SendRequestInternal(
      std::unique_ptr<network::ResourceRequest> resource_request,
      const std::string& req_data,
      const GURL& url,
      RTLookupResponseCallback response_callback);

  // Called when the response from the real-time lookup remote endpoint is
  // received. |url_loader| is the unowned loader that was used to send the
  // request. |request_start_time| is the time when the request was sent.
  // |response_body| is the response received. |url| is used for calling
  // |MayBeCacheRealTimeUrlVerdict|.
  void OnURLLoaderComplete(const GURL& url,
                           network::SimpleURLLoader* url_loader,
                           base::TimeTicks request_start_time,
                           std::unique_ptr<std::string> response_body);

  // Fills in fields in |RTLookupRequest|.
  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url);

  bool IsHistorySyncEnabled();

  // Count of consecutive failures to complete URL lookup requests. When it
  // reaches |kMaxFailuresToEnforceBackoff|, we enter the backoff mode. It gets
  // reset when we complete a lookup successfully or when the backoff reset
  // timer fires.
  size_t consecutive_failures_ = 0;

  // If true, represents that one or more real time lookups did complete
  // successfully since the last backoff or Chrome never entered the breakoff;
  // if false and Chrome re-enters backoff period, the backoff duration is
  // increased exponentially (capped at |kMaxBackOffResetDurationInSeconds|).
  bool did_successful_lookup_since_last_backoff_ = true;

  // The current duration of backoff. Increases exponentially until it reaches
  // |kMaxBackOffResetDurationInSeconds|.
  size_t next_backoff_duration_secs_ = 0;

  // If this timer is running, backoff is in effect.
  base::OneShotTimer backoff_timer_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting and storing real time url check cache.
  VerdictCacheManager* cache_manager_;

  // Unowned object used for checking sync status of the profile.
  syncer::SyncService* sync_service_;

  // Unowned object used for getting preference settings.
  PrefService* pref_service_;

  const ChromeUserPopulation::ProfileManagementStatus
      profile_management_status_;

  // Whether the profile is enrolled in  advanced protection.
  bool is_under_advanced_protection_;

  // A boolean indicates whether the profile associated with this
  // |url_lookup_service| is an off the record profile.
  bool is_off_the_record_;

  // All requests that are sent but haven't received a response yet.
  PendingRTLookupRequests pending_requests_;

  friend class RealTimeUrlLookupServiceTest;
  friend class ChromeEnterpriseRealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupServiceBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RealTimeUrlLookupServiceBase);

};  // class RealTimeUrlLookupServiceBase

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_BASE_H_
