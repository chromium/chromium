// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"

#include "base/base64url.h"
#include "chrome/browser/browser_process.h"
#include "components/lens/lens_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/omnibox_proto/search_context.pb.h"
#include "url/gurl.h"

namespace lens {
namespace {
// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";

// Query parameter for denoting a search companion request.
inline constexpr char kSearchCompanionParameterKey[] = "gsc";
inline constexpr char kSearchCompanionParameterValue[] = "1";

// Query parameter for denoting an ambient request source.
inline constexpr char kAmbientParameterKey[] = "masfc";
inline constexpr char kAmbientParameterValue[] = "c";

// Query parameter for the search session id.
inline constexpr char kSearchSessionIdParameterKey[] = "gsessionid";

// Query parameter for the request id.
inline constexpr char kRequestIdParameterKey[] = "vsrid";

// Query parameter for the mode.
inline constexpr char kModeParameterKey[] = "udm";
// Query parameter values for the mode.
inline constexpr char kUnimodalModeParameterValue[] = "26";
inline constexpr char kMultimodalModeParameterValue[] = "24";

// Query parameter for the language code.
inline constexpr char kLanguageCodeParameterKey[] = "hl";

// Query parameter for the search context.
inline constexpr char kSearchContextParameterKey[] = "mactx";

// Appends the url params from the map to the url.
GURL AppendUrlParamsFromMap(
    const GURL& url_to_modify,
    std::map<std::string, std::string> additional_params) {
  GURL url_with_params = GURL(url_to_modify);
  for (auto const& param : additional_params) {
    url_with_params = net::AppendOrReplaceQueryParameter(
        url_with_params, param.first, param.second);
  }
  return url_with_params;
}

}  // namespace

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify) {
  GURL new_url = url_to_modify;
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kSearchCompanionParameterKey, kSearchCompanionParameterValue);
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kAmbientParameterKey, kAmbientParameterValue);
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kLanguageCodeParameterKey,
      g_browser_process->GetApplicationLocale());
  return new_url;
}

GURL AppendSearchContextParamToURL(const GURL& url_to_modify,
                                   std::optional<GURL> page_url,
                                   std::optional<std::string> page_title) {
  if (!lens::features::UseSearchContextForTextOnlyLensOverlayRequests() ||
      (!page_url.has_value() && !page_title.has_value())) {
    return url_to_modify;
  }

  GURL new_url = url_to_modify;
  omnibox::SearchContext search_context;
  if (page_url.has_value()) {
    search_context.set_webpage_url(page_url->spec());
  }
  if (page_title.has_value()) {
    search_context.set_webpage_title(*page_title);
  }
  std::string serialized_search_context;
  if (!search_context.SerializeToString(&serialized_search_context)) {
    return url_to_modify;
  }
  std::string encoded_search_context;
  base::Base64UrlEncode(serialized_search_context,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_search_context);
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kSearchContextParameterKey, encoded_search_context);
  return new_url;
}

GURL BuildTextOnlySearchURL(
    const std::string& text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::map<std::string, std::string> additional_search_query_params) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = AppendUrlParamsFromMap(
      url_with_query_params, additional_search_query_params);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey, text_query);
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params);
  url_with_query_params = AppendSearchContextParamToURL(url_with_query_params,
                                                        page_url, page_title);
  return url_with_query_params;
}

GURL BuildLensSearchURL(
    std::optional<std::string> text_query,
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::LensOverlayClusterInfo cluster_info,
    std::map<std::string, std::string> additional_search_query_params) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = AppendUrlParamsFromMap(
      url_with_query_params, additional_search_query_params);
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey,
      text_query.has_value() ? *text_query : "");

  // The search url should use the search session id from the cluster info.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kSearchSessionIdParameterKey,
      cluster_info.search_session_id());

  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kModeParameterKey,
      text_query.has_value() ? kMultimodalModeParameterValue
                             : kUnimodalModeParameterValue);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kRequestIdParameterKey, encoded_request_id);

  return url_with_query_params;
}

const std::string GetTextQueryParameterValue(const GURL& url) {
  std::string param_value = "";
  net::GetValueForKeyInQuery(url, kTextQueryParameterKey, &param_value);
  return param_value;
}

bool HasCommonSearchQueryParameters(const GURL& url) {
  // Needed to prevent memory leaks even though we do not use the output.
  std::string temp_output_string;
  return net::GetValueForKeyInQuery(url, kSearchCompanionParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kAmbientParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kLanguageCodeParameterKey,
                                    &temp_output_string);
}

bool IsValidSearchResultsUrl(const GURL& url) {
  const GURL results_url(lens::features::GetLensOverlayResultsSearchURL());
  return url.is_valid() && results_url.SchemeIs(url.scheme()) &&
         results_url.path() == url.path() &&
         net::registry_controlled_domains::SameDomainOrHost(
             results_url, url,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace lens
