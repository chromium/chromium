// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace variations {
class VariationsService;
}

class PrefService;

namespace safe_browsing {

class SafeBrowsingTokenFetcher;
class ReferrerChainProvider;

// This class implements the real time lookup feature for a given user/profile.
// It is separated from the base class for logic that is related to consumer
// users.(See: go/chrome-protego-enterprise-dd)
class RealTimeUrlLookupService : public RealTimeUrlLookupServiceBase {
 public:
  // A callback via which the client of this component indicates whether they
  // are configured to support token fetches.
  using ClientConfiguredForTokenFetchesCallback =
      base::RepeatingCallback<bool(bool user_has_enabled_enhanced_protection)>;

  // |cache_manager|, |sync_service|, and |pref_service| may be null in tests.
  // |token_fetcher| may also be null, but in that case the passed-in
  // |client_token_config_callback| should return false to ensure that access
  // token fetches are not actually invoked.
  RealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      const ClientConfiguredForTokenFetchesCallback&
          client_token_config_callback,
      bool is_off_the_record,
      variations::VariationsService* variations_service,
      ReferrerChainProvider* referrer_chain_provider,
      WebUIDelegate* delegate);

  RealTimeUrlLookupService(const RealTimeUrlLookupService&) = delete;
  RealTimeUrlLookupService& operator=(const RealTimeUrlLookupService&) = delete;

  ~RealTimeUrlLookupService() override;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;
  bool CanIncludeSubframeUrlInReferrerChain() const override;
  bool CanCheckSafeBrowsingDb() const override;
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override;
  void Shutdown() override;
  bool CanSendRTSampleRequest() const override;
  std::string GetUserEmail() const override;
  std::string GetBrowserDMTokenString() const override;
  std::string GetProfileDMTokenString() const override;
  std::unique_ptr<enterprise_connectors::ClientMetadata> GetClientMetadata()
      const override;
  std::string GetMetricSuffix() const override;
  bool CanCheckUrl(const GURL& url) override;

#if defined(UNIT_TEST)
  void set_bypass_probability_for_tests(
      bool bypass_protego_probability_for_tests) {
    bypass_protego_probability_for_tests_ =
        bypass_protego_probability_for_tests;
  }
#endif

 private:
  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
  bool CanPerformFullURLLookupWithToken() const override;
  int GetReferrerUserGestureLimit() const override;
  bool CanSendPageLoadToken() const override;
  void GetAccessToken(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id) override;
  std::optional<std::string> GetDMTokenString() const override;
  bool ShouldIncludeCredentials() const override;
  void OnResponseUnauthorized(const std::string& invalid_access_token) override;
  std::optional<base::Time> GetMinAllowedTimestampForReferrerChains()
      const override;
  void MaybeLogLastProtegoPingTimeToPrefs(bool sent_with_token) override;
  void MaybeLogProtegoPingCookieHistograms(bool request_had_cookie,
                                           bool was_first_request,
                                           bool sent_with_token) override;

  // Called when prefs that affect real time URL lookup are changed.
  void OnPrefChanged();

  // Called when the access token is obtained from |token_fetcher_|.
  void OnGetAccessToken(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::TimeTicks get_token_start_time,
      SessionID tab_id,
      const std::string& access_token);

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Observes changes to kSafeBrowsingEnhanced and
  // kUrlKeyedAnonymizedDataCollectionEnabled;
  PrefChangeRegistrar pref_change_registrar_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // The callback via which the client of this component indicates whether they
  // are configured to support token fetches.
  ClientConfiguredForTokenFetchesCallback client_token_config_callback_;

  // A boolean indicates whether the profile associated with this
  // |url_lookup_service| is an off the record profile.
  bool is_off_the_record_;

  // The time that real time URL lookup is enabled. Not set if it is already
  // enabled at startup.
  std::optional<base::Time> url_lookup_enabled_timestamp_ = std::nullopt;

  // Unowned. For checking whether real-time checks can be enabled in a given
  // location.
  raw_ptr<variations::VariationsService, DanglingUntriaged> variations_;

  // Bypasses the check for probability when sending Protego sample pings.
  // Only for unit tests.
  bool bypass_protego_probability_for_tests_ = false;

  // True if Shutdown() has already been called, or started running. This allows
  // us to skip unnecessary calls to SendRequest().
  bool shutting_down_ = false;

  friend class RealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupService> weak_factory_{this};

};  // class RealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_URL_LOOKUP_SERVICE_H_
