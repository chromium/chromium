// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_entrypoints.h"

#include "base/strings/strcat.h"

namespace {

// Entry point string names.
constexpr char kEntryPointQueryParameter[] = "ep";
constexpr char kChromeRegionSearchMenuItem[] = "crs";
constexpr char kChromeSearchWithGoogleLensContextMenuItem[] = "ccm";
constexpr char kChromeOpenNewTabSidePanel[] = "cnts";

constexpr char kSurfaceQueryParameter[] = "s";
constexpr char kSidePanel[] = "csp";

void AppendQueryParam(std::string* query_string,
                      const char name[],
                      const char value[]) {
  if (!query_string->empty()) {
    base::StrAppend(query_string, {"&"});
  }
  base::StrAppend(query_string, {name, "=", value});
}

}  // namespace

namespace lens {

std::string GetQueryParametersForLensRequest(EntryPoint ep,
                                             bool is_side_panel_request) {
  std::string query_string;
  switch (ep) {
    case CHROME_OPEN_NEW_TAB_SIDE_PANEL:
      AppendQueryParam(&query_string, kEntryPointQueryParameter,
                       kChromeOpenNewTabSidePanel);
      break;
    case CHROME_REGION_SEARCH_MENU_ITEM:
      AppendQueryParam(&query_string, kEntryPointQueryParameter,
                       kChromeRegionSearchMenuItem);
      break;
    case CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM:
      AppendQueryParam(&query_string, kEntryPointQueryParameter,
                       kChromeSearchWithGoogleLensContextMenuItem);
      break;
    default:
      // Empty strings are ignored when query parameters are built.
      break;
  }
  if (is_side_panel_request) {
    AppendQueryParam(&query_string, kSurfaceQueryParameter, kSidePanel);
  }
  return query_string;
}

}  // namespace lens
