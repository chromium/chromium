// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/google/core/common/google_util.h"
#include "components/variations/net/omnibox_http_headers.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include "base/command_line.h"
#include "components/variations/net/variations_flags.h"
#include "net/base/url_util.h"
#endif  // BUILDFLAG(IS_IOS)

namespace variations {

// The name string for the header for variations information.
// Note that prior to M33 this header was named X-Chrome-Variations.
const char kClientDataHeader[] = "X-Client-Data";

namespace {

// The result of checking whether a request to a URL should have variations
// headers appended to it.
//
// This enum is used to record UMA histogram values, and should not be
// reordered.
enum class URLValidationResult {
  kNotValidInvalidUrl = 0,
  // kNotValidNotHttps = 1,  // Deprecated.
  kNotValidNotGoogleDomain = 2,
  kShouldAppend = 3,
  kNotValidNeitherHttpHttps = 4,
  kNotValidIsGoogleNotHttps = 5,
  kMaxValue = kNotValidIsGoogleNotHttps,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum RequestContextCategory {
  // First-party contexts.
  kBrowserInitiated = 0,
  kInternalChromePageInitiated = 1,
  kGooglePageInitiated = 2,
  kGoogleSubFrameOnGooglePageInitiated = 3,
  // Third-party contexts.
  kNonGooglePageInitiated = 4,
  // Deprecated because the histogram Variations.Headers.DomainOwner stores
  // more finely-grained information about this case.
  // kNoTrustedParams = 5,
  kNoIsolationInfo = 6,
  kGoogleSubFrameOnNonGooglePageInitiated = 7,
  // Deprecated because this category wasn't necessary in the first place. It's
  // covered by kNonGooglePageInitiated.
  // kNonGooglePageInitiatedFromFrameOrigin = 8,
  // The next RequestContextCategory should use 9.
  kMaxValue = kGoogleSubFrameOnNonGooglePageInitiated,
};

void LogRequestContextHistogram(RequestContextCategory result) {
  base::UmaHistogramEnumeration("Variations.Headers.RequestContextCategory",
                                result);
}

// Returns a URLValidationResult for |url|. A valid URL for headers has the
// following qualities: (i) it is well-formed, (ii) its scheme is HTTPS, and
// (iii) it has a Google-associated domain.
// On iOS, it is possible to pass the headers to localhost request if a flag
// is passed. This is needed for tests as EmbeddedTestServer is only
// accessible using 127.0.0.1. See crrev.com/c/3507791 for details.
URLValidationResult GetUrlValidationResult(const GURL& url) {
  if (!url.is_valid())
    return URLValidationResult::kNotValidInvalidUrl;

  if (!url.SchemeIsHTTPOrHTTPS())
    return URLValidationResult::kNotValidNeitherHttpHttps;

#if BUILDFLAG(IS_IOS)
  if (net::IsLocalhost(url) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAppendVariationsHeadersToLocalhostForTesting)) {
    return URLValidationResult::kShouldAppend;
  }
#endif  // BUILDFLAG(IS_IOS)

  if (!google_util::IsGoogleAssociatedDomainUrl(url))
    return URLValidationResult::kNotValidNotGoogleDomain;

  // HTTPS is checked here, rather than before the IsGoogleAssociatedDomainUrl()
  // check, to know how many Google domains are rejected by the change to append
  // headers to only HTTPS requests.
  if (!url.SchemeIs(url::kHttpsScheme))
    return URLValidationResult::kNotValidIsGoogleNotHttps;

  return URLValidationResult::kShouldAppend;
}

// Returns true if the request to |url| should include a variations header.
// Also, logs the result of validating |url| in histograms, one of which ends in
// |suffix|.
bool ShouldAppendVariationsHeader(const GURL& url, const std::string& suffix) {
  URLValidationResult result = GetUrlValidationResult(url);
  base::UmaHistogramEnumeration(
      "Variations.Headers.URLValidationResult." + suffix, result);
  return result == URLValidationResult::kShouldAppend;
}

// Returns true if the request is sent from a Google web property, i.e. from a
// first-party context.
//
// The context is determined using |owner| and |resource_request|. |owner| is
// used for subframe-initiated subresource requests from the renderer. Note that
// for these kinds of requests, ResourceRequest::TrustedParams is not populated.
bool IsFirstPartyContext(Owner owner,
                         const network::ResourceRequest& resource_request) {
  if (!resource_request.request_initiator) {
    // The absence of |request_initiator| means that the request was initiated
    // by the browser, e.g. a request from the browser to Autofill upon form
    // detection.
    LogRequestContextHistogram(RequestContextCategory::kBrowserInitiated);
    return true;
  }

  const GURL request_initiator_url =
      resource_request.request_initiator->GetURL();
  if (request_initiator_url.SchemeIs("chrome-search") ||
      request_initiator_url.SchemeIs("chrome")) {
    // A scheme matching the above patterns means that the request was
    // initiated by an internal page, e.g. a request from
    // chrome://newtab/ for App Launcher resources.
    LogRequestContextHistogram(kInternalChromePageInitiated);
    return true;
  }
  if (GetUrlValidationResult(request_initiator_url) !=
      URLValidationResult::kShouldAppend) {
    // The request was initiated by a non-Google-associated page, e.g. a request
    // from https://www.bbc.com/.
    LogRequestContextHistogram(kNonGooglePageInitiated);
    return false;
  }
  if (resource_request.is_outermost_main_frame) {
    // The request is from a Google-associated page--not a subframe--e.g. a
    // request from https://calendar.google.com/.
    LogRequestContextHistogram(kGooglePageInitiated);
    return true;
  }
  // |is_outermost_main_frame| is false, so the request was initiated by a
  // subframe (or embedded main frame like a fenced frame), and we need to
  // determine whether the top-level page in which the frame is embedded is a
  // Google-owned web property.
  //
  // If TrustedParams is populated, then we can use it to determine the request
  // context. If not, e.g. for subresource requests, we use |owner|.
  if (resource_request.trusted_params) {
    const net::IsolationInfo* isolation_info =
        &resource_request.trusted_params->isolation_info;

    if (isolation_info->IsEmpty()) {
      // TODO(crbug.com/40135370): If TrustedParams are present, it appears that
      // IsolationInfo is too. Maybe deprecate kNoIsolationInfo if this bucket
      // is never used.
      LogRequestContextHistogram(kNoIsolationInfo);
      // Without IsolationInfo, we cannot be certain that the request is from a
      // first-party context.
      return false;
    }
    if (GetUrlValidationResult(isolation_info->top_frame_origin()->GetURL()) ==
        URLValidationResult::kShouldAppend) {
      // The request is from a Google-associated subframe on a Google-associated
      // page, e.g. a request from a Docs subframe on https://drive.google.com/.
      LogRequestContextHistogram(kGoogleSubFrameOnGooglePageInitiated);
      return true;
    }
    // The request is from a Google-associated subframe on a non-Google-
    // associated page, e.g. a request to DoubleClick from an ad's subframe on
    // https://www.lexico.com/.
    LogRequestContextHistogram(kGoogleSubFrameOnNonGooglePageInitiated);
    return false;
  }
  base::UmaHistogramEnumeration("Variations.Headers.DomainOwner", owner);

  if (owner == Owner::kGoogle) {
    LogRequestContextHistogram(kGoogleSubFrameOnGooglePageInitiated);
    return true;
  }
  LogRequestContextHistogram(kGoogleSubFrameOnNonGooglePageInitiated);
  return false;
}

// Returns GoogleWebVisibility::FIRST_PARTY if the request is from a first-party
// context; otherwise, returns GoogleWebVisibility::ANY.
variations::mojom::GoogleWebVisibility GetVisibilityKey(
    Owner owner,
    const network::ResourceRequest& resource_request) {
  return IsFirstPartyContext(owner, resource_request)
             ? variations::mojom::GoogleWebVisibility::FIRST_PARTY
             : variations::mojom::GoogleWebVisibility::ANY;
}

// Returns a variations header from |variations_headers|.
std::string SelectVariationsHeader(
    variations::mojom::VariationsHeadersPtr variations_headers,
    Owner owner,
    const network::ResourceRequest& resource_request) {
  return variations_headers->headers_map.at(
      GetVisibilityKey(owner, resource_request));
}

class VariationsHeaderHelper {
 public:
  // Constructor for browser-initiated requests.
  //
  // If the signed-in status is unknown, SignedIn::kNo can be passed as it does
  // not affect transmission of experiments from the variations server.
  VariationsHeaderHelper(SignedIn signed_in,
                         network::ResourceRequest* resource_request)
      : VariationsHeaderHelper(CreateVariationsHeader(signed_in,
                                                      Owner::kUnknown,
                                                      *resource_request),
                               resource_request) {}

  // Constructor for when the appropriate header has been determined.
  VariationsHeaderHelper(std::string variations_header,
                         network::ResourceRequest* resource_request)
      : resource_request_(resource_request) {
    DCHECK(resource_request_);
    variations_header_ = std::move(variations_header);
  }

  VariationsHeaderHelper(const VariationsHeaderHelper&) = delete;
  VariationsHeaderHelper& operator=(const VariationsHeaderHelper&) = delete;

  bool AppendHeaderIfNeeded(const GURL& url, InIncognito incognito) {
    AppendOmniboxOnDeviceSuggestionsHeaderIfNeeded(url, resource_request_);

    // Note the criteria for attaching client experiment headers:
    // 1. We only transmit to Google owned domains which can evaluate
    // experiments.
    //    1a. These include hosts which have a standard postfix such as:
    //         *.doubleclick.net or *.googlesyndication.com or
    //         exactly www.googleadservices.com or
    //         international TLD domains *.google.<TLD> or *.youtube.<TLD>.
    // 2. Only transmit for non-Incognito profiles.
    // 3. For the X-Client-Data header, only include non-empty variation IDs.
    if ((incognito == InIncognito::kYes) ||
        !ShouldAppendVariationsHeader(url, "Append"))
      return false;

    if (variations_header_.empty())
      return false;

    // Set the variations header to cors_exempt_headers rather than headers to
    // be exempted from CORS checks, and to avoid exposing the header to service
    // workers.
    resource_request_->cors_exempt_headers.SetHeaderIfMissing(
        kClientDataHeader, variations_header_);
    return true;
  }

 private:
  // Returns a variations header containing IDs appropriate for |signed_in|.
  //
  // Can be used only by code running in the browser process, which is where
  // the populated VariationsIdsProvider exists.
  static std::string CreateVariationsHeader(
      SignedIn signed_in,
      Owner owner,
      const network::ResourceRequest& resource_request) {
    variations::mojom::VariationsHeadersPtr variations_headers =
        VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
            signed_in == SignedIn::kYes);

    if (variations_headers.is_null())
      return "";
    return variations_headers->headers_map.at(
        GetVisibilityKey(owner, resource_request));
  }

  raw_ptr<network::ResourceRequest> resource_request_;
  std::string variations_header_;
};

}  // namespace

bool AppendVariationsHeader(const GURL& url,
                            InIncognito incognito,
                            SignedIn signed_in,
                            network::ResourceRequest* request) {
  // TODO(crbug.com/40135370): Consider passing the Owner if we can get it.
  // However, we really only care about having the owner for requests initiated
  // on the renderer side.
  return VariationsHeaderHelper(signed_in, request)
      .AppendHeaderIfNeeded(url, incognito);
}

bool AppendVariationsHeaderWithCustomValue(
    const GURL& url,
    InIncognito incognito,
    variations::mojom::VariationsHeadersPtr variations_headers,
    Owner owner,
    network::ResourceRequest* request) {
  const std::string& header =
      SelectVariationsHeader(std::move(variations_headers), owner, *request);
  return VariationsHeaderHelper(header, request)
      .AppendHeaderIfNeeded(url, incognito);
}

bool AppendVariationsHeaderUnknownSignedIn(const GURL& url,
                                           InIncognito incognito,
                                           network::ResourceRequest* request) {
  // TODO(crbug.com/40135370): Consider passing the Owner if we can get it.
  // However, we really only care about having the owner for requests initiated
  // on the renderer side.
  return VariationsHeaderHelper(SignedIn::kNo, request)
      .AppendHeaderIfNeeded(url, incognito);
}

void RemoveVariationsHeaderIfNeeded(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  if (!ShouldAppendVariationsHeader(redirect_info.new_url, "Remove"))
    to_be_removed_headers->push_back(kClientDataHeader);
}

std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeader(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    SignedIn signed_in,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  bool variations_headers_added =
      AppendVariationsHeader(request->url, incognito, signed_in, request.get());
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(request), annotation_tag);
  if (variations_headers_added) {
    simple_url_loader->SetOnRedirectCallback(base::BindRepeating(
        [](const GURL& url_before_redirect,
           const net::RedirectInfo& redirect_info,
           const network::mojom::URLResponseHead& response_head,
           std::vector<std::string>* to_be_removed_headers) {
          RemoveVariationsHeaderIfNeeded(redirect_info, response_head,
                                         to_be_removed_headers);
        }));
  }
  return simple_url_loader;
}

std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return CreateSimpleURLLoaderWithVariationsHeader(
      std::move(request), incognito, SignedIn::kNo, annotation_tag);
}

bool HasVariationsHeader(const network::ResourceRequest& request) {
  std::string unused_header;
  return GetVariationsHeader(request, &unused_header);
}

bool GetVariationsHeader(const network::ResourceRequest& request,
                         std::string* out) {
  std::optional<std::string> header_value =
      request.cors_exempt_headers.GetHeader(kClientDataHeader);
  if (header_value) {
    out->swap(header_value.value());
  }
  return header_value.has_value();
}

bool ShouldAppendVariationsHeaderForTesting(
    const GURL& url,
    const std::string& histogram_suffix) {
  return ShouldAppendVariationsHeader(url, histogram_suffix);
}

void UpdateCorsExemptHeaderForVariations(
    network::mojom::NetworkContextParams* params) {
  params->cors_exempt_header_list.push_back(kClientDataHeader);

  if (base::FeatureList::IsEnabled(kReportOmniboxOnDeviceSuggestionsHeader)) {
    params->cors_exempt_header_list.push_back(
        kOmniboxOnDeviceSuggestionsHeader);
  }
}

}  // namespace variations
