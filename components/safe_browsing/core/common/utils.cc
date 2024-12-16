// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/utils.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#endif

namespace safe_browsing {

namespace {

// The bearer token prefix in authorization header. Used when various Safe
// Browsing requests are GAIA-keyed by attaching oauth2 tokens as bearer tokens.
const char kAuthHeaderBearer[] = "Bearer ";

// Represents the HTTP response code when it has not explicitly been set.
const int kUnsetHttpResponseCode = 0;

class CookieWriteLogger : public network::mojom::CookieAccessObserver {
 public:
  explicit CookieWriteLogger(
      safe_browsing::SafeBrowsingAuthenticatedEndpoint endpoint)
      : endpoint_(endpoint) {}

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(
      std::vector<network::mojom::CookieAccessDetailsPtr> details) override {
    for (const network::mojom::CookieAccessDetailsPtr& access : details) {
      if (access->type != network::mojom::CookieAccessDetails::Type::kChange) {
        continue;
      }

      base::UmaHistogramEnumeration(
          "SafeBrowsing.AuthenticatedCookieResetEndpoint", endpoint_);
    }
  }

  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 listener) override {
    mojo::MakeSelfOwnedReceiver(std::make_unique<CookieWriteLogger>(endpoint_),
                                std::move(listener));
  }

 private:
  safe_browsing::SafeBrowsingAuthenticatedEndpoint endpoint_;
};

}  // namespace

std::string ShortURLForReporting(const GURL& url) {
  std::string spec(url.spec());
  if (url.SchemeIs(url::kDataScheme)) {
    size_t comma_pos = spec.find(',');
    if (comma_pos != std::string::npos && comma_pos != spec.size() - 1) {
      std::string hash_value = crypto::SHA256HashString(spec);
      spec.erase(comma_pos + 1);
      spec += base::HexEncode(hash_value);
    }
  }
  return spec;
}

ChromeUserPopulation::ProfileManagementStatus GetProfileManagementStatus(
    const policy::BrowserPolicyConnector* bpc) {
#if BUILDFLAG(IS_WIN)
  if (base::IsManagedDevice())
    return ChromeUserPopulation::ENTERPRISE_MANAGED;
  else
    return ChromeUserPopulation::NOT_MANAGED;
#elif BUILDFLAG(IS_CHROMEOS)
  if (!bpc || !bpc->IsDeviceEnterpriseManaged())
    return ChromeUserPopulation::NOT_MANAGED;
  return ChromeUserPopulation::ENTERPRISE_MANAGED;
#else
  return ChromeUserPopulation::UNAVAILABLE;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
}

void SetDelayInPref(PrefService* prefs,
                    const char* pref_name,
                    const base::TimeDelta& delay) {
  base::Time next_event = base::Time::Now() + delay;
  int64_t seconds_since_epoch =
      next_event.ToDeltaSinceWindowsEpoch().InSeconds();
  prefs->SetInt64(pref_name, seconds_since_epoch);
}

base::TimeDelta GetDelayFromPref(PrefService* prefs, const char* pref_name) {
  const base::TimeDelta zero_delay;
  if (!prefs->HasPrefPath(pref_name))
    return zero_delay;

  int64_t seconds_since_epoch = prefs->GetInt64(pref_name);
  if (seconds_since_epoch <= 0)
    return zero_delay;

  base::Time next_event = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(seconds_since_epoch));
  base::Time now = base::Time::Now();
  if (now > next_event)
    return zero_delay;
  else
    return next_event - now;
}

bool CanGetReputationOfUrl(const GURL& url) {
  // net::IsLocalhost(url) includes: "//localhost/", "//127.0.0.1/"
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || net::IsLocalhost(url)) {
    return false;
  }
  const std::string hostname = url.host();
  // There is no reason to send URLs with very short or single-label hosts.
  // The Safe Browsing server does not check them.
  if (hostname.size() < 4 || base::ranges::count(hostname, '.') < 1) {
    return false;
  }

  net::IPAddress ip_address;
  if (url.HostIsIPAddress() && ip_address.AssignFromIPLiteral(hostname) &&
      !ip_address.IsPubliclyRoutable()) {
    // Includes: "//192.168.1.1/", "//172.16.2.2/", "//10.1.1.1/"
    return false;
  }

  return true;
}

void LogAuthenticatedCookieResets(network::ResourceRequest& resource_request,
                                  SafeBrowsingAuthenticatedEndpoint endpoint) {
  resource_request.trusted_params = network::ResourceRequest::TrustedParams();
  mojo::MakeSelfOwnedReceiver(std::make_unique<CookieWriteLogger>(endpoint),
                              resource_request.trusted_params->cookie_observer
                                  .InitWithNewPipeAndPassReceiver());
}

void SetAccessTokenAndClearCookieInResourceRequest(
    network::ResourceRequest* resource_request,
    const std::string& access_token) {
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({kAuthHeaderBearer, access_token}));
  if (base::FeatureList::IsEnabled(kSafeBrowsingRemoveCookiesInAuthRequests)) {
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  }
}

void RecordHttpResponseOrErrorCode(const char* metric_name,
                                   int net_error,
                                   int response_code) {
  base::UmaHistogramSparse(
      metric_name,
      net_error == net::OK || net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE
          ? response_code
          : net_error);
}

bool ErrorIsRetriable(int net_error, int http_error) {
  return (net_error == net::ERR_INTERNET_DISCONNECTED ||
          net_error == net::ERR_NETWORK_CHANGED) &&
         (http_error == kUnsetHttpResponseCode || http_error == net::HTTP_OK);
}

std::string GetExtraMetricsSuffix(
    security_interstitials::UnsafeResource unsafe_resource) {
  switch (unsafe_resource.threat_source) {
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return "from_device_v4";
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      return "from_client_side_detection";
    case safe_browsing::ThreatSource::URL_REAL_TIME_CHECK:
      return "from_real_time_check";
    case safe_browsing::ThreatSource::NATIVE_PVER5_REAL_TIME:
      return "from_hash_prefix_real_time_check_v5";
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      return "from_android_safebrowsing_real_time";
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING:
      return "from_android_safebrowsing";
    case safe_browsing::ThreatSource::UNKNOWN:
      break;
  }
  NOTREACHED();
}

}  // namespace safe_browsing
