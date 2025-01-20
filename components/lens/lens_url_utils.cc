// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_url_utils.h"

#include <map>
#include <string>

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

GURL AppendOrReplaceQueryParametersForLensRequest(const GURL& url,
                                                  lens::EntryPoint ep) {
  GURL modified_url(url);
  for (auto const& param : GetLensQueryParametersMap(ep)) {
    modified_url = net::AppendOrReplaceQueryParameter(modified_url, param.first,
                                                      param.second);
  }

  return modified_url;
}

std::string GetQueryParametersForLensRequest(lens::EntryPoint ep) {
  std::string query_string;
  for (auto const& param : GetLensQueryParametersMap(ep)) {
    AppendQueryParam(&query_string, param.first.c_str(), param.second.c_str());
  }
  return query_string;
}

bool IsValidLensResultUrl(const GURL& url) {
  if (url.is_empty()) {
    return false;
  }

  std::string payload;
  // Make sure the payload is present
  return net::GetValueForKeyInQuery(url, kPayloadQueryParameter, &payload);
}

bool IsLensMWebResult(const GURL& url) {
  std::string request_id;
  std::string surface;
  GURL result_url = GURL(lens::features::GetLensOverlayResultsSearchURL());
  return !url.is_empty() && url.host() == result_url.host() &&
         url.path() == result_url.path() &&
         net::GetValueForKeyInQuery(url, kLensRequestQueryParameter,
                                    &request_id) &&
         !net::GetValueForKeyInQuery(url, kLensSurfaceQueryParameter, &surface);
}

}  // namespace lens
