// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_url_utils.h"

#include <map>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_rendering_environment.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Entry point string names.
constexpr char kEntryPointQueryParameter[] = "ep";
constexpr char kChromeRegionSearchMenuItem[] = "crs";
constexpr char kChromeSearchWithGoogleLensContextMenuItem[] = "ccm";
constexpr char kChromeTranslateImageWithGoogleLensContextMenuItem[] = "ctrcm";
constexpr char kChromeOpenNewTabSidePanel[] = "cnts";
constexpr char kChromeFullscreenSearchMenuItem[] = "cfs";
constexpr char kCompanionRegionSearch[] = "cscidr";

constexpr char kSurfaceQueryParameter[] = "s";
// The value of Surface.CHROMIUM expected by Lens Web
constexpr char kChromiumSurfaceProtoValue[] = "4";

constexpr char kStartTimeQueryParameter[] = "st";
constexpr char kLensMetadataParameter[] = "lm";

constexpr char kRenderingEnvironmentQueryParameter[] = "re";
constexpr char kOneLensDesktopWebChromeSidePanel[] = "dcsp";
constexpr char kOneLensDesktopWebFullscreen[] = "df";
constexpr char kOneLensAmbientVisualSearchWebFullscreen[] = "avsf";
constexpr char kChromeSearchCompanion[] = "csc";
constexpr char kViewportWidthQueryParameter[] = "vpw";
constexpr char kViewportHeightQueryParameter[] = "vph";
// Query parameter for source (aka Access Point).
constexpr char kSourceQueryParameter[] = "source";
constexpr char kSourceQueryParameterValue[] = "chrome.gsc";

void AppendQueryParam(std::string* query_string,
                      const char name[],
                      const char value[]) {
  if (!query_string->empty()) {
    base::StrAppend(query_string, {"&"});
  }
  base::StrAppend(query_string, {name, "=", value});
}

std::map<std::string, std::string> GetLensQueryParametersMap(
    lens::EntryPoint ep,
    lens::RenderingEnvironment re,
    bool is_side_panel_request) {
  std::map<std::string, std::string> query_parameters;
  switch (ep) {
    case lens::CHROME_OPEN_NEW_TAB_SIDE_PANEL:
      query_parameters.insert(
          {kEntryPointQueryParameter, kChromeOpenNewTabSidePanel});
      break;
    case lens::CHROME_REGION_SEARCH_MENU_ITEM:
      query_parameters.insert(
          {kEntryPointQueryParameter, kChromeRegionSearchMenuItem});
      break;
    case lens::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM:
      query_parameters.insert({kEntryPointQueryParameter,
                               kChromeSearchWithGoogleLensContextMenuItem});
      break;
    case lens::CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM:
      query_parameters.insert(
          {kEntryPointQueryParameter,
           kChromeTranslateImageWithGoogleLensContextMenuItem});
      break;
    case lens::CHROME_FULLSCREEN_SEARCH_MENU_ITEM:
      query_parameters.insert(
          {kEntryPointQueryParameter, kChromeFullscreenSearchMenuItem});
      break;
    case lens::COMPANION_REGION_SEARCH:
      query_parameters.insert(
          {kEntryPointQueryParameter, kCompanionRegionSearch});
      break;
    default:
      // Empty strings are ignored when query parameters are built.
      break;
  }
  switch (re) {
    case lens::ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL:
      query_parameters.insert({kRenderingEnvironmentQueryParameter,
                               kOneLensDesktopWebChromeSidePanel});
      break;
    case lens::ONELENS_DESKTOP_WEB_FULLSCREEN:
      query_parameters.insert(
          {kRenderingEnvironmentQueryParameter, kOneLensDesktopWebFullscreen});
      break;
    case lens::ONELENS_AMBIENT_VISUAL_SEARCH_WEB_FULLSCREEN:
      query_parameters.insert({kRenderingEnvironmentQueryParameter,
                               kOneLensAmbientVisualSearchWebFullscreen});
      break;
    case lens::CHROME_SEARCH_COMPANION:
      query_parameters.insert(
          {kRenderingEnvironmentQueryParameter, kChromeSearchCompanion});
      query_parameters.insert(
          {kSourceQueryParameter, kSourceQueryParameterValue});
      break;
    default:
      // Empty strings are ignored when query parameters are built.
      break;
  }

  query_parameters.insert({kSurfaceQueryParameter, kChromiumSurfaceProtoValue});
  int64_t current_time_ms = base::Time::Now().ToJavaTime();
  query_parameters.insert(
      {kStartTimeQueryParameter, base::NumberToString(current_time_ms)});
  return query_parameters;
}

lens::RenderingEnvironment GetRenderingEnvironment(
    bool is_lens_side_panel_request,
    bool is_full_screen_region_search_request,
    bool is_companion_request) {
  if (is_companion_request) {
    return lens::RenderingEnvironment::CHROME_SEARCH_COMPANION;
  }

  if (is_full_screen_region_search_request)
    return lens::RenderingEnvironment::
        ONELENS_AMBIENT_VISUAL_SEARCH_WEB_FULLSCREEN;

  if (is_lens_side_panel_request) {
    return lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL;
  }

  return lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
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

GURL AppendOrReplaceQueryParametersForLensRequest(const GURL& url,
                                                  lens::EntryPoint ep,
                                                  lens::RenderingEnvironment re,
                                                  bool is_side_panel_request) {
  GURL modified_url(url);
  for (auto const& param :
       GetLensQueryParametersMap(ep, re, is_side_panel_request)) {
    modified_url = net::AppendOrReplaceQueryParameter(modified_url, param.first,
                                                      param.second);
  }

  // Remove the viewport width and height params if the request is not a side
  // panel request.
  if (!is_side_panel_request) {
    modified_url = net::AppendOrReplaceQueryParameter(
        modified_url, kViewportWidthQueryParameter, absl::nullopt);
    modified_url = net::AppendOrReplaceQueryParameter(
        modified_url, kViewportHeightQueryParameter, absl::nullopt);
  }
  return modified_url;
}

GURL AppendOrReplaceViewportSizeForRequest(const GURL& url,
                                           const gfx::Size& viewport_size) {
  GURL modified_url(url);

  const int viewport_width = viewport_size.width();
  const int viewport_height = viewport_size.height();
  if (viewport_width != 0) {
    modified_url = net::AppendOrReplaceQueryParameter(
        modified_url, kViewportWidthQueryParameter,
        base::NumberToString(viewport_width));
  }
  if (viewport_height != 0) {
    modified_url = net::AppendOrReplaceQueryParameter(
        modified_url, kViewportHeightQueryParameter,
        base::NumberToString(viewport_height));
  }
  return modified_url;
}

std::string GetQueryParametersForLensRequest(
    lens::EntryPoint ep,
    bool is_lens_side_panel_request,
    bool is_full_screen_region_search_request,
    bool is_companion_request) {
  auto re = GetRenderingEnvironment(is_lens_side_panel_request,
                                    is_full_screen_region_search_request,
                                    is_companion_request);
  std::string query_string;
  const bool is_side_panel_request =
      is_lens_side_panel_request || is_companion_request;
  for (auto const& param :
       GetLensQueryParametersMap(ep, re, is_side_panel_request)) {
    AppendQueryParam(&query_string, param.first.c_str(), param.second.c_str());
  }
  return query_string;
}

}  // namespace lens
