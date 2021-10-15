// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
#define COMPONENTS_LENS_LENS_ENTRYPOINTS_H_

#include <string>

class GURL;

namespace lens {

// Query parameter for the payload.
constexpr char kPayloadQueryParameter[] = "p";

// Lens entry points for LWD.
enum EntryPoint {
  CHROME_OPEN_NEW_TAB_SIDE_PANEL,
  CHROME_REGION_SEARCH_MENU_ITEM,
  CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM,
  UNKNOWN
};

// Returns a modified GURL with appended or replaced parameters depending on the
// entrypoint and other parameters.
extern GURL AppendOrReplaceQueryParametersForLensRequest(
    const GURL& url,
    EntryPoint ep,
    bool is_side_panel_request);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
extern std::string GetQueryParametersForLensRequest(EntryPoint entry_point,
                                                    bool is_side_panel_request);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
