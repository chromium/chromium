// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/google/core/common/google_util.h"
#include "components/variations/variations_http_header_provider.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace variations {

namespace {

const char* kSuffixesToSetHeadersFor[] = {
    ".android.com",
    ".doubleclick.com",
    ".doubleclick.net",
    ".ggpht.com",
    ".googleadservices.com",
    ".googleapis.com",
    ".googlesyndication.com",
    ".googleusercontent.com",
    ".googlevideo.com",
    ".gstatic.com",
    ".litepages.googlezip.net",
    ".ytimg.com",
};

// Exact hostnames in lowercase to set headers for.
const char* kHostsToSetHeadersFor[] = {
    "googleweblight.com",
};

// The result of checking if a URL should have variations headers appended.
// This enum is used to record UMA histogram values, and should not be
// reordered.
enum URLValidationResult {
  INVALID_URL,
  NOT_HTTPS,
  NOT_GOOGLE_DOMAIN,
  SHOULD_APPEND,
  NEITHER_HTTP_HTTPS,
  IS_GOOGLE_NOT_HTTPS,
  URL_VALIDATION_RESULT_SIZE,
};

// Checks whether headers should be appended to the |url|, based on the domain
// of |url|. |url| is assumed to be valid, and to have an http/https scheme.
bool IsGoogleDomain(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    return true;
  }
  if (google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  // Some domains don't have international TLD extensions, so testing for them
  // is very straight forward.
  const std::string host = url.host();
  for (size_t i = 0; i < base::size(kSuffixesToSetHeadersFor); ++i) {
    if (base::EndsWith(host, kSuffixesToSetHeadersFor[i],
                       base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }
  for (size_t i = 0; i < base::size(kHostsToSetHeadersFor); ++i) {
    if (base::LowerCaseEqualsASCII(host, kHostsToSetHeadersFor[i]))
      return true;
  }

  return false;
}

void LogUrlValidationHistogram(URLValidationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.Headers.URLValidationResult", result,
                            URL_VALIDATION_RESULT_SIZE);
}

// Removes variations headers for requests when a redirect to a non-Google URL
// occurs. This function is used as the callback parameter for
// SimpleURLLoader::SetOnRedirectCallback() when
// CreateSimpleURLLoaderWithVariationsHeaders() creates a SimpleURLLoader
// object.
void RemoveVariationsHeader(const net::RedirectInfo& redirect_info,
                            const network::ResourceResponseHead& response_head,
                            std::vector<std::string>* to_be_removed_headers) {
  if (!ShouldAppendVariationHeaders(redirect_info.new_url))
    to_be_removed_headers->push_back(kClientDataHeader);
}

}  // namespace

const char kClientDataHeader[] = "X-Client-Data";

bool AppendVariationHeaders(const GURL& url,
                            InIncognito incognito,
                            SignedIn signed_in,
                            net::HttpRequestHeaders* headers) {
  // Note the criteria for attaching client experiment headers:
  // 1. We only transmit to Google owned domains which can evaluate experiments.
  //    1a. These include hosts which have a standard postfix such as:
  //         *.doubleclick.net or *.googlesyndication.com or
  //         exactly www.googleadservices.com or
  //         international TLD domains *.google.<TLD> or *.youtube.<TLD>.
  // 2. Only transmit for non-Incognito profiles.
  // 3. For the X-Client-Data header, only include non-empty variation IDs.
  if ((incognito == InIncognito::kYes) || !ShouldAppendVariationHeaders(url))
    return false;

  const std::string variation_ids_header =
      VariationsHttpHeaderProvider::GetInstance()->GetClientDataHeader(
          signed_in == SignedIn::kYes);
  if (!variation_ids_header.empty()) {
    // Note that prior to M33 this header was named X-Chrome-Variations.
    headers->SetHeaderIfMissing(kClientDataHeader, variation_ids_header);
    return true;
  }
  return false;
}

bool AppendVariationHeadersUnknownSignedIn(const GURL& url,
                                           InIncognito incognito,
                                           net::HttpRequestHeaders* headers) {
  // Note: It's OK to pass SignedIn::kNo if it's unknown, as it does not affect
  // transmission of experiments coming from the variations server.
  return AppendVariationHeaders(url, incognito, SignedIn::kNo, headers);
}

void StripVariationHeaderIfNeeded(const GURL& new_location,
                                  net::URLRequest* request) {
  if (!ShouldAppendVariationHeaders(new_location))
    request->RemoveRequestHeaderByName(kClientDataHeader);
}

std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeaders(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    SignedIn signed_in,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  bool variation_headers_added = AppendVariationHeaders(
      request->url, incognito, signed_in, &request->headers);
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(request), annotation_tag);
  if (variation_headers_added) {
    simple_url_loader->SetOnRedirectCallback(
        base::BindRepeating(&RemoveVariationsHeader));
  }
  return simple_url_loader;
}

std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeadersUnknownSignedIn(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return CreateSimpleURLLoaderWithVariationsHeaders(
      std::move(request), incognito, SignedIn::kNo, annotation_tag);
}

bool ShouldAppendVariationHeaders(const GURL& url) {
  if (!url.is_valid()) {
    LogUrlValidationHistogram(INVALID_URL);
    return false;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    LogUrlValidationHistogram(NEITHER_HTTP_HTTPS);
    return false;
  }
  if (!IsGoogleDomain(url)) {
    LogUrlValidationHistogram(NOT_GOOGLE_DOMAIN);
    return false;
  }
  // We check https here, rather than before the IsGoogleDomain() check, to know
  // how many Google domains are being rejected by the change to https only.
  if (!url.SchemeIs("https")) {
    LogUrlValidationHistogram(IS_GOOGLE_NOT_HTTPS);
    return false;
  }
  LogUrlValidationHistogram(SHOULD_APPEND);
  return true;
}

}  // namespace variations
