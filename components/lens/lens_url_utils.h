// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

class GURL;

namespace lens {

// Query parameter for denoting a search companion request.
inline constexpr char kChromeSidePanelParameterKey[] = "gsc";

inline constexpr char kContextualVisualInputTypeQueryParameterValue[] = "video";

inline constexpr char kDarkModeParameterKey[] = "cs";
inline constexpr char kDarkModeParameterLightValue[] = "0";
inline constexpr char kDarkModeParameterDarkValue[] = "1";

// Query parameter for the filter type.
inline constexpr char kFilterTypeQueryParameter[] = "filtertype";

inline constexpr char kImageVisualInputTypeQueryParameterValue[] = "img";

// Query parameter for the language code.
inline constexpr char kLanguageCodeParameterKey[] = "hl";

inline constexpr char kLensRequestQueryParameter[] = "vsrid";
inline constexpr char kLensSurfaceQueryParameter[] = "lns_surface";

// Query parameter for the payload.
inline constexpr char kPayloadQueryParameter[] = "p";

inline constexpr char kPdfVisualInputTypeQueryParameterValue[] = "pdf";

// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";

// Query parameter for the translate source language.
inline constexpr char kTranslateSourceQueryParameter[] = "sourcelang";

inline constexpr char kTranslateFilterTypeQueryParameterValue[] = "tr";

// Query parameter for the translate target language.
inline constexpr char kTranslateTargetQueryParameter[] = "targetlang";

inline constexpr char kUnifiedDrillDownQueryParameter[] = "udm";
inline constexpr char kWebpageVisualInputTypeQueryParameterValue[] = "wp";

inline constexpr std::array<lens::MimeType, 3> kUnsupportedVitMimeTypes = {
    lens::MimeType::kVideo, lens::MimeType::kAudio};

// Appends logs to query param as a string
void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
std::string GetQueryParametersForLensRequest(EntryPoint ep);

// Returns true if the given URL corresponds to a Lens mWeb result page. This is
// done by checking the URL and its parameters.
bool IsLensMWebResult(const GURL& url);

std::string Base64EncodeRequestId(LensOverlayRequestId request_id);

// Returns the vit query parameter value for the given mime type.
std::string VitQueryParamValueForMimeType(MimeType mime_type);

// Returns the vit query parameter value for the given media type.
std::string VitQueryParamValueForMediaType(
    LensOverlayRequestId::MediaType media_type);

// Returns a key-value map of all parameters in `url` except the query
// parameter.
std::map<std::string, std::string> GetParametersMapWithoutQuery(
    const GURL& url);

// Returns a map of all the common search query parameters required to enable
// the lens overlay results in the side panel.
std::map<std::string, std::string> GetCommonSearchParametersMap(
    const std::string& country_code,
    bool use_dark_mode,
    bool is_side_panel);

// Returns |url_to_modify| with all the common search query parameters required
// to enable the lens overlay results in the side panel.
GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify,
                                       const std::string& country_code,
                                       bool use_dark_mode);

// Returns whether the given |url| contains all the common search query
// parameters required to properly enable the lens overlay results in the side
// panel. This does not check the value of these query parameters.
bool HasCommonSearchQueryParameters(const GURL& url);

// Append the dark mode param to the provided |url_to_modify|.
GURL AppendDarkModeParamToURL(const GURL& url_to_modify, bool use_dark_mode);

// Remove parameters that cause the SRP to be rendered for the side panel. Used
// when opening the SRP in a new tab.
GURL RemoveSidePanelURLParameters(const GURL& url);

// Appends the invocation source parameter to the URL. If `is_contextual_tasks`
// is true, the source URL param will be prefixed with `chrome.crn`. If false,
// the param will be prefixed with `chrome.cr`.
GURL AppendInvocationSourceParamToURL(
    const GURL& url_to_modify,
    lens::LensOverlayInvocationSource invocation_source,
    bool is_contextual_tasks);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_
