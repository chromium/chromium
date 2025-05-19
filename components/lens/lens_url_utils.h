// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <string>
#include <vector>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"

class GURL;

namespace lens {

// Query parameter for the payload.
inline constexpr char kPayloadQueryParameter[] = "p";
// Query parameter for the translate source language.
inline constexpr char kTranslateSourceQueryParameter[] = "sourcelang";
// Query parameter for the translate target language.
inline constexpr char kTranslateTargetQueryParameter[] = "targetlang";
// Query parameter for the filter type.
inline constexpr char kFilterTypeQueryParameter[] = "filtertype";
inline constexpr char kTranslateFilterTypeQueryParameterValue[] = "tr";
inline constexpr char kLensRequestQueryParameter[] = "vsrid";
inline constexpr char kUnifiedDrillDownQueryParameter[] = "udm";
inline constexpr char kLensSurfaceQueryParameter[] = "lns_surface";

// Appends logs to query param as a string
void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a modified GURL with appended or replaced parameters depending on the
// entrypoint and other parameters.
GURL AppendOrReplaceQueryParametersForLensRequest(const GURL& url,
                                                  EntryPoint ep);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
std::string GetQueryParametersForLensRequest(EntryPoint ep);

// Check if the lens URL is a valid results page. This is done by checking if
// the URL has a payload parameter.
bool IsValidLensResultUrl(const GURL& url);

// Returns true if the given URL corresponds to a Lens mWeb result page. This is
// done by checking the URL and its parameters.
bool IsLensMWebResult(const GURL& url);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_
