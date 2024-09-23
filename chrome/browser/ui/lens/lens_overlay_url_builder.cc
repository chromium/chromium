// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "chrome/browser/browser_process.h"
#include "components/language/core/common/language_util.h"
#include "components/lens/lens_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_knowledge_intent_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_knowledge_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_stickiness_signals.pb.h"
#include "third_party/lens_server_proto/lens_overlay_translate_stickiness_signals.pb.h"
#include "third_party/lens_server_proto/lens_overlay_video_context_input_params.pb.h"
#include "third_party/lens_server_proto/lens_overlay_video_params.pb.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace lens {
namespace {
// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";

// Query parameter for denoting a search companion request.
inline constexpr char kSearchCompanionParameterKey[] = "gsc";

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

// Query parameter for the video context.
inline constexpr char kVideoContextParameterKey[] = "vidcip";

// Query parameter for the lens mode.
inline constexpr char kLensModeParameterKey[] = "lns_mode";
inline constexpr char kLensModeParameterTextValue[] = "text";
inline constexpr char kLensModeParameterUnimodalValue[] = "un";
inline constexpr char kLensModeParameterMultimodalValue[] = "mu";

// Parameters to trigger the Translation One-box.
inline constexpr char kSrpStickinessSignalKey[] = "stick";

// Query parameter for the invocation source.
inline constexpr char kInvocationSourceParameterKey[] = "source";
inline constexpr char kInvocationSourceAppMenu[] = "chrome.cr.menu";
inline constexpr char kInvocationSourcePageSearchContextMenu[] =
    "chrome.cr.ctxp";
inline constexpr char kInvocationSourceImageSearchContextMenu[] =
    "chrome.cr.ctxi";
inline constexpr char kInvocationSourceFindInPage[] = "chrome.cr.find";
inline constexpr char kInvocationSourceToolbarIcon[] = "chrome.cr.tbic";
inline constexpr char kInvocationSourceOmniboxIcon[] = "chrome.cr.obic";

// The url query param for the viewport width and height.
inline constexpr char kViewportWidthQueryParamKey[] = "biw";
inline constexpr char kViewportHeightQueryParamKey[] = "bih";

// Query parameters that can be appended by GWS to the URL after a redirect.
inline constexpr char kXSRFTokenQueryParamKey[] = "sxsrf";
inline constexpr char kSecActQueryParamKey[] = "sec_act";

// The list of query parameters to ignore when comparing search URLs.
inline constexpr std::string kIgnoredSearchUrlQueryParameters[] = {
    kViewportWidthQueryParamKey, kViewportHeightQueryParamKey,
    kXSRFTokenQueryParamKey, kSecActQueryParamKey};

// Query parameter for dark mode.
inline constexpr char kDarkModeParameterKey[] = "cs";
inline constexpr char kDarkModeParameterLightValue[] = "0";
inline constexpr char kDarkModeParameterDarkValue[] = "1";

// Query parameter for the Lens footprint.
inline constexpr char kLensFootprintParameterKey[] = "lns_fp";
inline constexpr char kLensFootprintParameterValue[] = "1";

// Url path for redirects from the results base URL.
inline constexpr char kUrlRedirectPath[] = "/url";

// Query parameter for the URL to redirect to.
inline constexpr char kUrlQueryParameterKey[] = "url";

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

void AppendTranslateParamsToMap(std::map<std::string, std::string>& params,
                                const std::string& query,
                                const std::string& content_language) {
  lens::StickinessSignals stickiness_signals;
  stickiness_signals.set_id_namespace(lens::StickinessSignals::TRANSLATE_LITE);
  auto* intent_query = stickiness_signals.mutable_interpretation()
                           ->mutable_message_set_extension()
                           ->mutable_intent_query();
  intent_query->set_name("Translate");
  intent_query->mutable_signals()
      ->mutable_translate_stickiness_signals()
      ->set_translate_suppress_echo_for_sticky(false);
  auto* text_argument = intent_query->add_argument();
  text_argument->set_name("Text");
  text_argument->mutable_value()->mutable_simple_value()->set_string_value(
      query);

  std::string serialized_proto;
  stickiness_signals.SerializeToString(&serialized_proto);
  std::string compressed_proto;
  compression::GzipCompress(serialized_proto, &compressed_proto);
  std::string stickiness_signal_value;
  base::Base64UrlEncode(compressed_proto,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &stickiness_signal_value);

  params[kSrpStickinessSignalKey] = stickiness_signal_value;
}

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify,
                                       bool use_dark_mode) {
  GURL new_url = url_to_modify;
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kSearchCompanionParameterKey,
      lens::features::GetLensOverlayGscQueryParamValue());
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kLanguageCodeParameterKey,
      g_browser_process->GetApplicationLocale());
  new_url = AppendDarkModeParamToURL(new_url, use_dark_mode);
  return new_url;
}

GURL AppendVideoContextParamToURL(const GURL& url_to_modify,
                                  std::optional<GURL> page_url) {
  if (!page_url.has_value()) {
    return url_to_modify;
  }

  lens::LensOverlayVideoParams video_params;
  video_params.mutable_video_context_input_params()->set_url(page_url->spec());
  std::string serialized_video_params;
  if (!video_params.SerializeToString(&serialized_video_params)) {
    return url_to_modify;
  }
  std::string encoded_video_params;
  base::Base64UrlEncode(serialized_video_params,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_video_params);
  GURL new_url = url_to_modify;
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kVideoContextParameterKey, encoded_video_params);
  return new_url;
}

GURL AppendInvocationSourceParamToURL(
    const GURL& url_to_modify,
    lens::LensOverlayInvocationSource invocation_source) {
  std::string param_value = "";
  switch (invocation_source) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      param_value = kInvocationSourceAppMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      param_value = kInvocationSourcePageSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      param_value = kInvocationSourceImageSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kToolbar:
      param_value = kInvocationSourceToolbarIcon;
      break;
    case lens::LensOverlayInvocationSource::kFindInPage:
      param_value = kInvocationSourceFindInPage;
      break;
    case lens::LensOverlayInvocationSource::kOmnibox:
      param_value = kInvocationSourceOmniboxIcon;
      break;
  }
  return net::AppendOrReplaceQueryParameter(
      url_to_modify, kInvocationSourceParameterKey, param_value);
}

GURL AppendDarkModeParamToURL(const GURL& url_to_modify, bool use_dark_mode) {
  return net::AppendOrReplaceQueryParameter(
      url_to_modify, kDarkModeParameterKey,
      use_dark_mode ? kDarkModeParameterDarkValue
                    : kDarkModeParameterLightValue);
}

GURL BuildTextOnlySearchURL(
    const std::string& text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source,
    TextOnlyQueryType text_only_query_type,
    bool use_dark_mode) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = AppendInvocationSourceParamToURL(
      url_with_query_params, invocation_source);
  url_with_query_params = AppendUrlParamsFromMap(
      url_with_query_params, additional_search_query_params);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey, text_query);
  if (text_only_query_type == TextOnlyQueryType::kLensTextSelection) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kLensFootprintParameterKey,
        kLensFootprintParameterValue);
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kLensModeParameterKey,
        kLensModeParameterTextValue);
  }
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params, use_dark_mode);
  if (lens::features::UseVideoContextForTextOnlyLensOverlayRequests()) {
    // All queries use the video context param to report page context
    // information even if the page does not contain a video.
    url_with_query_params =
        AppendVideoContextParamToURL(url_with_query_params, page_url);
  }
  return url_with_query_params;
}

GURL BuildLensSearchURL(
    std::optional<std::string> text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::LensOverlayClusterInfo cluster_info,
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = AppendInvocationSourceParamToURL(
      url_with_query_params, invocation_source);
  url_with_query_params = AppendUrlParamsFromMap(
      url_with_query_params, additional_search_query_params);
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params, use_dark_mode);
  if (text_query.has_value() &&
      lens::features::UseVideoContextForMultimodalLensOverlayRequests()) {
    // All pages use the video context param to report page context information
    // even if the page does not contain a video.
    url_with_query_params =
        AppendVideoContextParamToURL(url_with_query_params, page_url);
  }
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey,
      text_query.has_value() ? *text_query : "");
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kLensModeParameterKey,
      text_query.has_value() ? kLensModeParameterMultimodalValue
                             : kLensModeParameterUnimodalValue);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kLensFootprintParameterKey,
      kLensFootprintParameterValue);

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

const std::string GetLensModeParameterValue(const GURL& url) {
  std::string param_value = "";
  net::GetValueForKeyInQuery(url, kLensModeParameterKey, &param_value);
  return param_value;
}

bool HasCommonSearchQueryParameters(const GURL& url) {
  // Needed to prevent memory leaks even though we do not use the output.
  std::string temp_output_string;
  return net::GetValueForKeyInQuery(url, kSearchCompanionParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kLanguageCodeParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kDarkModeParameterKey,
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

GURL GetSearchResultsUrlFromRedirectUrl(const GURL& url) {
  const GURL results_url(lens::features::GetLensOverlayResultsSearchURL());
  // The URL should always be valid, have the same domain or host, and share the
  // same scheme as the base search results URL.
  if (!url.is_valid() || !results_url.SchemeIs(url.scheme()) ||
      !net::registry_controlled_domains::SameDomainOrHost(
          results_url, url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return GURL();
  }

  // We only allow paths from `/url` if they are redirecting to a search URL.
  std::string url_redirect_string;
  if (url.path() == kUrlRedirectPath &&
      net::GetValueForKeyInQuery(url, kUrlQueryParameterKey,
                                 &url_redirect_string)) {
    // The redirecting URL should be relative. Return false if not.
    GURL url_to_redirect_to = results_url.Resolve(url_redirect_string);
    if (!url_to_redirect_to.is_empty() && url_to_redirect_to.is_valid() &&
        results_url.path() == url_to_redirect_to.path()) {
      // Decode the url if needed since it should be encoded.
      url_to_redirect_to = GURL(base::UnescapeURLComponent(
          url_to_redirect_to.spec(), base::UnescapeRule::SPACES));
      return url_to_redirect_to;
    }
  }

  return GURL();
}

GURL RemoveIgnoredSearchURLParameters(const GURL& url) {
  GURL processed_url = url;
  for (std::string query_param : kIgnoredSearchUrlQueryParameters) {
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, query_param, std::nullopt);
  }
  return processed_url;
}

}  // namespace lens
