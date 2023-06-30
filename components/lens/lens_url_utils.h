// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <string>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_rendering_environment.h"
#include "ui/gfx/geometry/size_f.h"

class GURL;

namespace lens {

// Query parameter for the payload.
constexpr char kPayloadQueryParameter[] = "p";
// Query parameter for the translate source language.
constexpr char kTranslateSourceQueryParameter[] = "sourcelang";
// Query parameter for the translate target language.
constexpr char kTranslateTargetQueryParameter[] = "targetlang";
// Query parameter for the filter type.
constexpr char kFilterTypeQueryParameter[] = "filtertype";
constexpr char kTranslateFilterTypeQueryParameterValue[] = "tr";

// Appends logs to query param as a string
extern void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a modified GURL with appended or replaced parameters depending on the
// entrypoint and other parameters. The width and height of the side panel
// initial size are ignored if they are 0 or if the request is not a side panel
// request.
extern GURL AppendOrReplaceQueryParametersForLensRequest(
    const GURL& url,
    lens::EntryPoint ep,
    lens::RenderingEnvironment re,
    bool is_side_panel_request,
    const gfx::Size& side_panel_initial_size_upper_bound);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to. The width and height of the side panel
// initial size are ignored if they are 0 or if the request is not a Lens side
// panel or companion request.
extern std::string GetQueryParametersForLensRequest(
    lens::EntryPoint ep,
    bool is_lens_side_panel_request,
    const gfx::Size& side_panel_initial_size_upper_bound,
    bool is_full_screen_region_search_request,
    bool is_companion_request = false);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_
