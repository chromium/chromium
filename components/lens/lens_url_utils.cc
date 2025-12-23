// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_url_utils.h"

#include <map>
#include <string>

#include "base/base64url.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.h"
#include "components/lens/lens_metadata.mojom.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Entry point string names.
constexpr char kEntryPointQueryParameter[] = "ep";
constexpr char kChromeRegionSearchMenuItem[] = "crs";
constexpr char kChromeSearchWithGoogleLensContextMenuItem[] = "ccm";
constexpr char kChromeVideoFrameSearchContextMenuItem[] = "cvfs";
constexpr char kChromeLensOverlayLocationBar[] = "crmntob";

constexpr char kSurfaceQueryParameter[] = "s";
// The value of Surface.CHROMIUM expected by Lens Web
constexpr char kChromiumSurfaceProtoValue[] = "4";

constexpr char kStartTimeQueryParameter[] = "st";
constexpr char kLensMetadataParameter[] = "lm";

constexpr char kRenderingEnvironmentQueryParameter[] = "re";
constexpr char kOneLensDesktopWebFullscreen[] = "df";

// Query parameter for the invocation source.
inline constexpr char kInvocationSourceParameterKey[] = "source";
inline constexpr char kLensPanelPrefix[] = "chrome.cr.";
inline constexpr char kContextualTasksPrefix[] = "chrome.crn.";
inline constexpr char kInvocationSourceAppMenu[] = "menu";
inline constexpr char kInvocationSourcePageSearchContextMenu[] = "ctxp";
inline constexpr char kInvocationSourceImageSearchContextMenu[] = "ctxi";
inline constexpr char kInvocationSourceTextSearchContextMenu[] = "ctxt";
inline constexpr char kInvocationSourceVideoSearchContextMenu[] = "ctxv";
inline constexpr char kInvocationSourceFindInPage[] = "find";
inline constexpr char kInvocationSourceToolbarIcon[] = "tbic";
inline constexpr char kInvocationSourceOmniboxIcon[] = "obic";
inline constexpr char kInvocationSourceOmniboxPageAction[] = "obpa";
inline constexpr char kInvocationSourceOmniboxContextualSuggestion[] = "obcs";
inline constexpr char kInvocationSourceHomeworkActionChip[] = "hwac";

void AppendQueryParam(std::string* query_string,
                      const char name[],
                      const char value[]) {
  if (!query_string->empty()) {
    base::StrAppend(query_string, {"&"});
  }
  base::StrAppend(query_string, {name, "=", value});
}

std::string GetEntryPointQueryString(lens::EntryPoint entry_point) {
  switch (entry_point) {
    case lens::CHROME_REGION_SEARCH_MENU_ITEM:
      return kChromeRegionSearchMenuItem;
    case lens::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM:
      return kChromeSearchWithGoogleLensContextMenuItem;
    case lens::CHROME_VIDEO_FRAME_SEARCH_CONTEXT_MENU_ITEM:
      return kChromeVideoFrameSearchContextMenuItem;
    case lens::CHROME_LENS_OVERLAY_LOCATION_BAR:
      return kChromeLensOverlayLocationBar;
    case lens::UNKNOWN:
      return "";
  }
}

std::map<std::string, std::string> GetLensQueryParametersMap(
    lens::EntryPoint ep) {
  std::map<std::string, std::string> query_parameters;

  // Insert EntryPoint query parameter.
  std::string entry_point_query_string = GetEntryPointQueryString(ep);
  if (!entry_point_query_string.empty()) {
    query_parameters.insert(
        {kEntryPointQueryParameter, entry_point_query_string});
  }

  // Insert RenderingEnvironment desktop fullscreen query parameter.
  query_parameters.insert(
      {kRenderingEnvironmentQueryParameter, kOneLensDesktopWebFullscreen});

  query_parameters.insert({kSurfaceQueryParameter, kChromiumSurfaceProtoValue});
  int64_t current_time_ms = base::Time::Now().InMillisecondsSinceUnixEpoch();
  query_parameters.insert(
      {kStartTimeQueryParameter, base::NumberToString(current_time_ms)});
  return query_parameters;
}

}  // namespace

namespace lens {

void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data) {
  if (!log_data.empty()) {
    AppendQueryParam(query_string, kLensMetadataParameter,
                     LensMetadata::CreateProto(std::move(log_data)).c_str());
  }
}

std::string GetQueryParametersForLensRequest(lens::EntryPoint ep) {
  std::string query_string;
  for (auto const& param : GetLensQueryParametersMap(ep)) {
    AppendQueryParam(&query_string, param.first.c_str(), param.second.c_str());
  }
  return query_string;
}

bool IsLensMWebResult(const GURL& url) {
  std::string request_id;
  std::string surface;
  GURL result_url = GURL(lens::features::GetLensOverlayResultsSearchURL());
  return !url.is_empty() && url.GetHost() == result_url.GetHost() &&
         url.GetPath() == result_url.GetPath() &&
         net::GetValueForKeyInQuery(url, kLensRequestQueryParameter,
                                    &request_id) &&
         !net::GetValueForKeyInQuery(url, kLensSurfaceQueryParameter, &surface);
}

std::string Base64EncodeRequestId(lens::LensOverlayRequestId request_id) {
  std::string serialized_request_id;
  CHECK(request_id.SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);
  return encoded_request_id;
}

std::string VitQueryParamValueForMimeType(MimeType mime_type) {
  // Default contextual visual input type.
  std::string vitValue = kContextualVisualInputTypeQueryParameterValue;
  switch (mime_type) {
    case lens::MimeType::kPdf:
      vitValue = kPdfVisualInputTypeQueryParameterValue;
      break;
    case lens::MimeType::kHtml:
    case lens::MimeType::kPlainText:
    case lens::MimeType::kAnnotatedPageContent:
      vitValue = kWebpageVisualInputTypeQueryParameterValue;
      break;
    case lens::MimeType::kUnknown:
      break;
    case lens::MimeType::kImage:
      vitValue = kImageVisualInputTypeQueryParameterValue;
      break;
    case lens::MimeType::kVideo:
    case lens::MimeType::kAudio:
    case lens::MimeType::kJson:
      // These content types are not supported for the page content upload flow.
      NOTREACHED() << "Unsupported option in page content upload";
  }
  return vitValue;
}

std::string VitQueryParamValueForMediaType(
    LensOverlayRequestId_MediaType media_type) {
  switch (media_type) {
    case LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE:
      return kImageVisualInputTypeQueryParameterValue;
    case LensOverlayRequestId::MEDIA_TYPE_WEBPAGE:
    case LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE:
      return kWebpageVisualInputTypeQueryParameterValue;
    case LensOverlayRequestId::MEDIA_TYPE_PDF:
    case LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE:
      return kPdfVisualInputTypeQueryParameterValue;
    default:
      return "";
  }
}

std::map<std::string, std::string> GetParametersMapWithoutQuery(
    const GURL& url) {
  std::map<std::string, std::string> additional_query_parameters;
  net::QueryIterator query_iterator(url);
  while (!query_iterator.IsAtEnd()) {
    std::string_view key = query_iterator.GetKey();
    if (kTextQueryParameterKey != key) {
      additional_query_parameters.insert(std::make_pair(
          query_iterator.GetKey(), query_iterator.GetUnescapedValue()));
    }
    query_iterator.Advance();
  }
  return additional_query_parameters;
}

std::map<std::string, std::string> GetCommonSearchParametersMap(
    const std::string& country_code,
    bool use_dark_mode,
    bool is_side_panel) {
  std::map<std::string, std::string> params;
  params.insert({kChromeSidePanelParameterKey,
                 is_side_panel
                     ? lens::features::GetLensOverlayGscQueryParamValue()
                     : ""});
  params.insert({kLanguageCodeParameterKey, country_code});
  params.insert({kDarkModeParameterKey, use_dark_mode
                                            ? kDarkModeParameterDarkValue
                                            : kDarkModeParameterLightValue});
  return params;
}

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify,
                                       const std::string& country_code,
                                       bool use_dark_mode) {
  GURL new_url = url_to_modify;
  for (const auto& [key, value] :
       GetCommonSearchParametersMap(country_code, use_dark_mode,
                                    /*is_side_panel=*/true)) {
    new_url = net::AppendOrReplaceQueryParameter(new_url, key, value);
  }
  return new_url;
}

bool HasCommonSearchQueryParameters(const GURL& url) {
  // Needed to prevent memory leaks even though we do not use the output.
  std::string temp_output_string;
  return net::GetValueForKeyInQuery(url, kChromeSidePanelParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kLanguageCodeParameterKey,
                                    &temp_output_string) &&
         net::GetValueForKeyInQuery(url, kDarkModeParameterKey,
                                    &temp_output_string);
}

GURL AppendDarkModeParamToURL(const GURL& url_to_modify, bool use_dark_mode) {
  return net::AppendOrReplaceQueryParameter(
      url_to_modify, kDarkModeParameterKey,
      use_dark_mode ? kDarkModeParameterDarkValue
                    : kDarkModeParameterLightValue);
}

GURL RemoveSidePanelURLParameters(const GURL& url) {
  GURL processed_url = url;
  processed_url = net::AppendOrReplaceQueryParameter(
      processed_url, kChromeSidePanelParameterKey, std::nullopt);
  return processed_url;
}

GURL AppendInvocationSourceParamToURL(
    const GURL& url_to_modify,
    lens::LensOverlayInvocationSource invocation_source,
    bool is_contextual_tasks) {
  std::string param_value =
      is_contextual_tasks ? kContextualTasksPrefix : kLensPanelPrefix;
  switch (invocation_source) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      param_value += kInvocationSourceAppMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      param_value += kInvocationSourcePageSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      param_value += kInvocationSourceImageSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuText:
      param_value += kInvocationSourceTextSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuVideo:
      param_value += kInvocationSourceVideoSearchContextMenu;
      break;
    case lens::LensOverlayInvocationSource::kToolbar:
      param_value += kInvocationSourceToolbarIcon;
      break;
    case lens::LensOverlayInvocationSource::kFindInPage:
      param_value += kInvocationSourceFindInPage;
      break;
    case lens::LensOverlayInvocationSource::kOmnibox:
      param_value += kInvocationSourceOmniboxIcon;
      break;
    case lens::LensOverlayInvocationSource::kOmniboxPageAction:
      param_value += kInvocationSourceOmniboxPageAction;
      break;
    case lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion:
      param_value += kInvocationSourceOmniboxContextualSuggestion;
      break;
    case lens::LensOverlayInvocationSource::kHomeworkActionChip:
      param_value += kInvocationSourceHomeworkActionChip;
      break;
    case lens::LensOverlayInvocationSource::kLVFShutterButton:
    case lens::LensOverlayInvocationSource::kLVFGallery:
    case lens::LensOverlayInvocationSource::kContextMenu:
    case lens::LensOverlayInvocationSource::kAIHub:
    case lens::LensOverlayInvocationSource::kFREPromo:
      NOTREACHED() << "Invocation source not supported.";
  }
  return net::AppendOrReplaceQueryParameter(
      url_to_modify, kInvocationSourceParameterKey, param_value);
}

}  // namespace lens
