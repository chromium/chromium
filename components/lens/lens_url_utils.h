// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <string>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"
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
constexpr char kLensRequestQueryParameter[] = "vsrid";

// Appends logs to query param as a string
extern void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a modified GURL with appended or replaced parameters depending on the
// entrypoint and other parameters.
extern GURL AppendOrReplaceQueryParametersForLensRequest(const GURL& url,
                                                         lens::EntryPoint ep);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
extern std::string GetQueryParametersForLensRequest(lens::EntryPoint ep);

// Check if the lens URL is a valid results page. This is done by checking if
// the URL has a payload parameter.
bool IsValidLensResultUrl(const GURL& url);

// Returns true if the given URL corresponds to a Lens mWeb result page. This is
// done by checking the URL and its parameters.
bool IsLensMWebResult(const GURL& url);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_
