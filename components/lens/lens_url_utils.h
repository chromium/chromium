// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <string>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_rendering_environment.h"

class GURL;

namespace lens {

// Query parameter for the payload.
constexpr char kPayloadQueryParameter[] = "p";

// Appends logs to query param as a string
extern void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a modified GURL with appended or replaced parameters depending on the
// entrypoint and other parameters.
extern GURL AppendOrReplaceQueryParametersForLensRequest(
    const GURL& url,
    lens::EntryPoint ep,
    lens::RenderingEnvironment re,
    bool is_side_panel_request);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
extern std::string GetQueryParametersForLensRequest(
    lens::EntryPoint ep,
    bool is_side_panel_request,
    bool is_full_screen_region_search_request);
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_