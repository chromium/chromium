// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_entrypoints.h"

#include "base/strings/strcat.h"

namespace lens {

// Entry point string names.
constexpr char kEntryPointQueryParameter[] = "ep=";
constexpr char kChromeRegionSearchMenuItem[] = "crs";
constexpr char kChromeSearchWithGoogleLensContextMenuItem[] = "ccm";

std::string GetQueryParameterFromEntryPoint(EntryPoint ep) {
  std::string query_parameter = std::string(kEntryPointQueryParameter);
  switch (ep) {
    case CHROME_REGION_SEARCH_MENU_ITEM:
      base::StrAppend(&query_parameter, {kChromeRegionSearchMenuItem});
      break;
    case CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM:
      base::StrAppend(&query_parameter,
                      {kChromeSearchWithGoogleLensContextMenuItem});
      break;
    default:
      // Empty strings are ignored when query parameters are built.
      return std::string();
  }
  return query_parameter;
}

}  // namespace lens
