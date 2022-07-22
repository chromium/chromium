// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_entrypoints.h"

#include <map>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Entry point string names.
constexpr char kEntryPointQueryParameter[] = "ep";
constexpr char kChromeRegionSearchMenuItem[] = "crs";
constexpr char kChromeSearchWithGoogleLensContextMenuItem[] = "ccm";
constexpr char kChromeOpenNewTabSidePanel[] = "cnts";
constexpr char kChromeFullscreenSearchMenuItem[] = "cfs";
constexpr char kChromeScreenshotSearch[] = "css";

constexpr char kSurfaceQueryParameter[] = "s";
constexpr char kStartTimeQueryParameter[] = "st";
constexpr char kSidePanel[] = "csp";

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
    case lens::CHROME_FULLSCREEN_SEARCH_MENU_ITEM:
      query_parameters.insert(
          {kEntryPointQueryParameter, kChromeFullscreenSearchMenuItem});
      break;
    case lens::CHROME_SCREENSHOT_SEARCH:
      query_parameters.insert(
          {kEntryPointQueryParameter, kChromeScreenshotSearch});
      break;
    default:
      // Empty strings are ignored when query parameters are built.
      break;
  }
  if (is_side_panel_request) {
    query_parameters.insert({kSurfaceQueryParameter, kSidePanel});
  } else {
    // Set the surface parameter to an empty string to represent default value.
    query_parameters.insert({kSurfaceQueryParameter, ""});
  }
  int64_t current_time_ms = base::Time::Now().ToJavaTime();
  query_parameters.insert(
      {kStartTimeQueryParameter, base::NumberToString(current_time_ms)});
  return query_parameters;
}

}  // namespace

namespace lens {

GURL AppendOrReplaceQueryParametersForLensRequest(const GURL& url,
                                                  EntryPoint ep,
                                                  bool is_side_panel_request) {
  GURL modified_url(url);
  for (auto const& param : GetLensQueryParametersMap(ep, is_side_panel_request))
    modified_url = net::AppendOrReplaceQueryParameter(modified_url, param.first,
                                                      param.second);
  return modified_url;
}

std::string GetQueryParametersForLensRequest(EntryPoint ep,
                                             bool is_side_panel_request) {
  std::string query_string;
  for (auto const& param : GetLensQueryParametersMap(ep, is_side_panel_request))
    AppendQueryParam(&query_string, param.first.c_str(), param.second.c_str());
  return query_string;
}

}  // namespace lens
