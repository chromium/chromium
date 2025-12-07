// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}

namespace policy {
class ManagementService;
}

namespace enterprise_connectors {
class ConnectorsServiceBase;
}

class PrefService;

namespace safe_browsing {

class ReferrerChainProvider;

// TODO(crbug.com/406211981): Migrate unit tests to components and remove the
// comment below.
// Note: Unit tests for this class are in chrome/browser/safe_browsing.

// This class implements the real time lookup feature for a given user/profile.
// It is separated from the base class for logic that is related to enterprise
// users.(See: go/chrome-protego-enterprise-dd)
class ChromeEnterpriseRealTimeUrlLookupService
    : public RealTimeUrlLookupServiceBase {
 public:
  ChromeEnterpriseRealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      enterprise_connectors::ConnectorsServiceBase* connectors_service,
      ReferrerChainProvider* referrer_chain_provider,
      PrefService* pref_service,
      WebUIDelegate* webui_delegate,
      signin::IdentityManager* identity_manager,
      policy::ManagementService* management_service,
      bool is_off_the_record,
      bool is_guest_session,
      base::RepeatingCallback<std::string()> get_profile_email_callback,
      base::RepeatingCallback<std::string(const GURL&)>
          get_content_area_account_email_callback,
      base::RepeatingCallback<bool()> is_profile_affiliated_callback,
      bool is_command_line_switch_supported);

  ChromeEnterpriseRealTimeUrlLookupService(
      const ChromeEnterpriseRealTimeUrlLookupService&) = delete;
  ChromeEnterpriseRealTimeUrlLookupService& operator=(
      const ChromeEnterpriseRealTimeUrlLookupService&) = delete;

  ~ChromeEnterpriseRealTimeUrlLookupService() override;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;
  bool CanIncludeSubframeUrlInReferrerChain() const override;
  bool CanCheckSafeBrowsingDb() const override;
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override;
  bool CanSendRTSampleRequest() const override;
  std::string GetUserEmail() const override;
  std::string GetBrowserDMTokenString() const override;
  std::string GetProfileDMTokenString() const override;
  std::unique_ptr<enterprise_connectors::ClientMetadata> GetClientMetadata()
      const override;
  std::string GetContentAreaAccountEmail(const GURL& tab_url) const override;
  std::string GetMetricSuffix() const override;
  bool ShouldOverrideKnownSafeUrlDecision(const GURL& url) const override;
  bool CanCheckUrl(const GURL& url) override;

 private:
  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
  bool CanPerformFullURLLookupWithToken() const override;
  int GetReferrerUserGestureLimit() const override;
  bool CanSendPageLoadToken() const override;

  std::optional<std::string> GetDMTokenString() const override;
  bool ShouldIncludeCredentials() const override;
  std::optional<base::Time> GetMinAllowedTimestampForReferrerChains()
      const override;

  // Unowned pointer to ConnectorsService, used to get a DM token.
  raw_ptr<enterprise_connectors::ConnectorsServiceBase, DanglingUntriaged>
      connectors_service_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Unowned object used for accessing the user's Google identity.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Unowned object for accessing the profile's management state.
  raw_ptr<policy::ManagementService> management_service_;

  // Indicates if the service is bound to an off the record browsing session.
  bool is_off_the_record_;

  // Indicates if the service is bound to a guest browsing session.
  bool is_guest_session_;

  // Callback for accessing the profile's email.
  base::RepeatingCallback<std::string()> get_profile_email_callback_;

  // Callback for accessing the content area's email. This is used for active
  // Gaia filtering. The argument is the tab URL.
  base::RepeatingCallback<std::string(const GURL&)>
      get_content_area_account_email_callback_;

  // Callback returning whether the profile and browser are managed by the same
  // organization.
  base::RepeatingCallback<bool()> is_profile_affiliated_callback_;

  // Whether the service can use the policy command line switch.
  bool is_command_line_switch_supported_;

  friend class ChromeEnterpriseRealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<ChromeEnterpriseRealTimeUrlLookupService> weak_factory_{
      this};

};  // class ChromeEnterpriseRealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
