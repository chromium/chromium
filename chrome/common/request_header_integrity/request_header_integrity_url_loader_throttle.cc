// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/request_header_integrity/chrome_companero_loader.h"
#include "chrome/common/request_header_integrity/platform_runtime_headers.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/google/core/common/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
// Fraction of requests for which the Platform Runtime apply result is recorded.
constexpr double kApplyResultSamplingRate = 0.01;
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

  if (base::EqualsCaseInsensitiveASCII(channel_name, "unknown")) {
    return "";
  }

  return channel_name;
}

void AddRequestIntegrityHeaderNamesToVector(
    std::vector<std::string>* vector,
    ChromeCompaneroLoader& companero_loader) {
  vector->push_back(CHANNEL_NAME_HEADER_NAME);
  vector->push_back(LASTCHANGE_YEAR_HEADER_NAME);
  vector->push_back(VALIDATE_HEADER_NAME);
  vector->push_back(COPYRIGHT_HEADER_NAME);
#if defined(INTEGRITY_DYNAMIC_HEADER_1)
  vector->push_back(INTEGRITY_DYNAMIC_HEADER_1);
#endif
#if defined(INTEGRITY_DYNAMIC_HEADER_2)
  vector->push_back(INTEGRITY_DYNAMIC_HEADER_2);
#endif
  auto header = companero_loader.GetHeaderNameAndValue();
  if (header) {
    if (std::ranges::find(*vector, header->name) == vector->end()) {
      vector->push_back(header->name);
    }
  }
}

void ApplyPlatformRuntimeHeaders(net::HttpRequestHeaders* headers) {
#if !BUILDFLAG(IS_ANDROID)
  // PlatformRuntimeHost does not run on Android, so no headers are ever
  // published there.
  [[maybe_unused]] const PlatformRuntimeApplyResult result =
      PlatformRuntimeHeaders::GetInstance().Apply(headers);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This runs for every request and redirect in every process, so only a
  // random subset is recorded to keep metrics off the hot path.
  if (base::ShouldRecordSubsampledMetric(kApplyResultSamplingRate)) {
    base::UmaHistogramEnumeration(
        "ComponentUpdater.PlatformRuntime.RequestHeaderApplyResult", result);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // !BUILDFLAG(IS_ANDROID)
}

using ResourceType = RequestHeaderIntegrityURLLoaderThrottle::ResourceType;

// Records whether the dynamic integrity header was attached to a request of
// `resource_type`.
void RecordDynamicHeaderPresent(ResourceType resource_type, bool present) {
  switch (resource_type) {
    case ResourceType::kMainResource:
      base::UmaHistogramBoolean(
          "Security.RequestHeaderIntegrity.DynamicHeaderPresent.MainResource",
          present);
      return;
    case ResourceType::kSubresource:
      base::UmaHistogramBoolean(
          "Security.RequestHeaderIntegrity.DynamicHeaderPresent.Subresource",
          present);
      return;
    case ResourceType::kPrefetch:
      base::UmaHistogramBoolean(
          "Security.RequestHeaderIntegrity.DynamicHeaderPresent.Prefetch",
          present);
      return;
  }
}

}  // namespace

RequestHeaderIntegrityURLLoaderThrottle::
    RequestHeaderIntegrityURLLoaderThrottle()
    : RequestHeaderIntegrityURLLoaderThrottle(
          ChromeCompaneroLoader::GetInstance()) {}

RequestHeaderIntegrityURLLoaderThrottle::
    RequestHeaderIntegrityURLLoaderThrottle(
        ChromeCompaneroLoader& companero_loader)
    : companero_loader_(companero_loader) {}

RequestHeaderIntegrityURLLoaderThrottle::
    ~RequestHeaderIntegrityURLLoaderThrottle() = default;

void RequestHeaderIntegrityURLLoaderThrottle::DetachFromCurrentSequence() {}

void RequestHeaderIntegrityURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // Captured even for non-Google URLs, since the request may later be
  // redirected to a Google-associated domain.
  resource_type_ =
      request->destination == network::mojom::RequestDestination::kDocument
          ? ResourceType::kMainResource
          : ResourceType::kSubresource;
  if (google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    AddRequestIntegrityHeaders(&(request->cors_exempt_headers),
                               *companero_loader_, resource_type_);
  }
  ApplyPlatformRuntimeHeaders(&(request->cors_exempt_headers));
}

void RequestHeaderIntegrityURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  if (google_util::IsGoogleAssociatedDomainUrl(redirect_info->new_url)) {
    AddRequestIntegrityHeaders(
        &headers_update_params->modified_cors_exempt_headers,
        *companero_loader_, resource_type_);
  } else {
    AddRequestIntegrityHeaderNamesToVector(
        &headers_update_params->removed_headers, *companero_loader_);
  }
  ApplyPlatformRuntimeHeaders(
      &headers_update_params->modified_cors_exempt_headers);
}

// static
bool RequestHeaderIntegrityURLLoaderThrottle::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kRequestHeaderIntegrity);
}

// static
void RequestHeaderIntegrityURLLoaderThrottle::UpdateCorsExemptHeaders(
    network::mojom::NetworkContextParams* params) {
  AddRequestIntegrityHeaderNamesToVector(&(params->cors_exempt_header_list),
                                         ChromeCompaneroLoader::GetInstance());
}

// static
void RequestHeaderIntegrityURLLoaderThrottle::AddRequestIntegrityHeaders(
    net::HttpRequestHeaders* headers,
    ChromeCompaneroLoader& companero_loader,
    std::optional<ResourceType> resource_type) {
  const std::string digest = base::Base64Encode(base::SHA1Hash(
      base::as_byte_span(base::StrCat({kIntegritySeed, google_apis::GetAPIKey(),
                                       embedder_support::GetUserAgent()}))));
  const std::string channel_name = GetChannelName();
  if (!channel_name.empty()) {
    headers->SetHeader(CHANNEL_NAME_HEADER_NAME, channel_name);
  }
  headers->SetHeader(LASTCHANGE_YEAR_HEADER_NAME, LASTCHANGE_YEAR);
  headers->SetHeader(VALIDATE_HEADER_NAME, digest);
  headers->SetHeader(COPYRIGHT_HEADER_NAME, CHROME_COPYRIGHT);

  auto companero_header = companero_loader.GetHeaderNameAndValue();
  if (companero_header) {
    headers->SetHeader(companero_header->name, companero_header->value);
  }
  if (resource_type) {
    RecordDynamicHeaderPresent(*resource_type, companero_header.has_value());
  }
}

// static
void RequestHeaderIntegrityURLLoaderThrottle::
    ModifyRequestIntegrityHeadersForPrefetch(
        const GURL& url,
        std::vector<std::string>& removed_headers,
        net::HttpRequestHeaders& cors_exempt_headers) {
  CHECK(IsFeatureEnabled());
  if (google_util::IsGoogleAssociatedDomainUrl(url)) {
    AddRequestIntegrityHeaders(&cors_exempt_headers,
                               ChromeCompaneroLoader::GetInstance(),
                               ResourceType::kPrefetch);
  } else {
    AddRequestIntegrityHeaderNamesToVector(
        &removed_headers, ChromeCompaneroLoader::GetInstance());
  }
  ApplyPlatformRuntimeHeaders(&cors_exempt_headers);
}

}  // namespace request_header_integrity
