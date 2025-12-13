// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/google/core/common/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/build_derived_values.h"
#include "chrome/common/request_header_integrity/internal/google_header_names.h"
#include "chrome/common/request_header_integrity/internal/integrity_seed_internal.h"
#endif

#if !defined(CHANNEL_NAME_HEADER_NAME)
#define CHANNEL_NAME_HEADER_NAME "X-Placeholder-1"
#endif

#if !defined(LASTCHANGE_YEAR_HEADER_NAME)
#define LASTCHANGE_YEAR_HEADER_NAME "X-Placeholder-2"
#endif

#if !defined(VALIDATE_HEADER_NAME)
#define VALIDATE_HEADER_NAME "X-Placeholder-3"
#endif

#if !defined(COPYRIGHT_HEADER_NAME)
#define COPYRIGHT_HEADER_NAME "X-Placeholder-4"
#endif

#if !defined(CHROME_COPYRIGHT)
#define CHROME_COPYRIGHT "X-COPYRIGHT"
#endif

#if !defined(LASTCHANGE_YEAR)
#define LASTCHANGE_YEAR "1969"
#endif

namespace request_header_integrity {

namespace {

BASE_FEATURE(kRequestHeaderIntegrity, base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Seed for header integrity (empty for unbranded builds).
constexpr char kIntegritySeed[] = "";
#endif

// Returns extended, stable, beta, dev, or canary if a channel is available,
// otherwise the empty string.
std::string GetChannelName() {
  std::string channel_name =
      chrome::GetChannelName(chrome::WithExtendedStable(true));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (channel_name.empty()) {
    // For branded builds, stable is represented as the empty string.
    channel_name = "stable";
  }
#endif

  if (base::ToLowerASCII(channel_name) == "unknown") {
    return "";
  }

  return channel_name;
}

void AddRequestIntegrityHeaders(net::HttpRequestHeaders* headers) {
  const std::string digest =
      base::Base64Encode(base::SHA1Hash(base::as_byte_span(
          std::string(kIntegritySeed) + google_apis::GetAPIKey() +
          embedder_support::GetUserAgent())));
  const std::string channel_name = GetChannelName();
  if (!channel_name.empty()) {
    headers->SetHeader(CHANNEL_NAME_HEADER_NAME, channel_name);
  }
  headers->SetHeader(LASTCHANGE_YEAR_HEADER_NAME, LASTCHANGE_YEAR);
  headers->SetHeader(VALIDATE_HEADER_NAME, digest);
  headers->SetHeader(COPYRIGHT_HEADER_NAME, CHROME_COPYRIGHT);
}

void AddRequestIntegrityHeaderNamesToVector(std::vector<std::string>* vector) {
  vector->push_back(CHANNEL_NAME_HEADER_NAME);
  vector->push_back(LASTCHANGE_YEAR_HEADER_NAME);
  vector->push_back(VALIDATE_HEADER_NAME);
  vector->push_back(COPYRIGHT_HEADER_NAME);
}

}  // namespace

RequestHeaderIntegrityURLLoaderThrottle::
    RequestHeaderIntegrityURLLoaderThrottle() = default;

RequestHeaderIntegrityURLLoaderThrottle::
    ~RequestHeaderIntegrityURLLoaderThrottle() = default;

void RequestHeaderIntegrityURLLoaderThrottle::DetachFromCurrentSequence() {}

void RequestHeaderIntegrityURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    return;
  }

  AddRequestIntegrityHeaders(&(request->cors_exempt_headers));
}

void RequestHeaderIntegrityURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (google_util::IsGoogleAssociatedDomainUrl(redirect_info->new_url)) {
    AddRequestIntegrityHeaders(modified_cors_exempt_request_headers);
  } else {
    AddRequestIntegrityHeaderNamesToVector(to_be_removed_request_headers);
  }
}

// static
bool RequestHeaderIntegrityURLLoaderThrottle::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kRequestHeaderIntegrity);
}

// static
void RequestHeaderIntegrityURLLoaderThrottle::UpdateCorsExemptHeaders(
    network::mojom::NetworkContextParams* params) {
  AddRequestIntegrityHeaderNamesToVector(&(params->cors_exempt_header_list));
}

}  // namespace request_header_integrity
