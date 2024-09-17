// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing utility functions.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_UTILS_H_

#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace base {
class TimeDelta;
}  // namespace base

namespace network {
struct ResourceRequest;
}  // namespace network

namespace security_interstitials {
struct UnsafeResource;
}

class PrefService;

namespace safe_browsing {

// Shorten URL by replacing its contents with its SHA256 hash if it has data
// scheme.
std::string ShortURLForReporting(const GURL& url);

// Gets the |ProfileManagementStatus| for the current machine. The method
// currently works only on Windows and ChromeOS. The |bpc| parameter is used
// only on ChromeOS, and may be |nullptr|.
ChromeUserPopulation::ProfileManagementStatus GetProfileManagementStatus(
    const policy::BrowserPolicyConnector* bpc);

// Util for storing a future alarm time in a pref. |delay| is how much time into
// the future the alarm is set for. Calling GetDelayFromPref() later will return
// a shorter delay, or 0 if it's unset or passed..
void SetDelayInPref(PrefService* prefs,
                    const char* pref_name,
                    const base::TimeDelta& delay);
base::TimeDelta GetDelayFromPref(PrefService* prefs, const char* pref_name);

// Safe Browsing backend cannot get a reliable reputation of a URL if
// (1) URL is not valid
// (2) URL doesn't have http or https scheme
// (3) It maps to a local host.
// (4) Its hostname is an IP Address that is assigned from IP literal.
// (5) Its hostname is a dotless domain.
// (6) Its hostname is less than 4 characters.
bool CanGetReputationOfUrl(const GURL& url);

// List of callers of
// `SetAccessTokenAndClearCookieInResourceRequest`. This is used for
// logging the histogram SafeBrowsing.AuthenticatedCookieResetEndpoint.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafeBrowsingAuthenticatedEndpoint {
  kDeepScanning = 0,
  kDownloadProtection = 1,
  kExtensionTelemetry = 2,
  kClientSideDetection = 3,
  kPasswordProtection = 4,
  kThreatDetails = 5,
  kRealtimeUrlLookup = 6,
  kMaxValue = kRealtimeUrlLookup,
};

// When cookies are changed on this request, log the
// SafeBrowsing.AuthenticatedCookieResetEndpoint histogram.
void LogAuthenticatedCookieResets(network::ResourceRequest& resource_request,
                                  SafeBrowsingAuthenticatedEndpoint endpoint);

// Set |access_token| in |resource_request|. Remove cookies in the request
// since we only need one identifier.
void SetAccessTokenAndClearCookieInResourceRequest(
    network::ResourceRequest* resource_request,
    const std::string& access_token);

// Record HTTP response code when there's no error in fetching an HTTP
// request, and the error code, when there is.
// |metric_name| is the name of the UMA metric to record the response code or
// error code against, |net_error| represents the net error code of the HTTP
// request, and |response code| represents the HTTP response code received
// from the server.
void RecordHttpResponseOrErrorCode(const char* metric_name,
                                   int net_error,
                                   int response_code);

// If the network response for a request has errors, the corresponding service
// usually increments the backoff counter. However, some errors are not related
// to the network infrastructure and therefore don't require this. This function
// returns whether an error is considered retriable and doesn't need to
// increment backoff.
bool ErrorIsRetriable(int net_error, int http_error);

// We populate a parallel set of metrics to differentiate some threat sources.
std::string GetExtraMetricsSuffix(
    security_interstitials::UnsafeResource unsafe_resource);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_UTILS_H_
