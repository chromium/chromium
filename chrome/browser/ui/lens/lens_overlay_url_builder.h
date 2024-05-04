// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_

#include <optional>
#include <string>

#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "url/gurl.h"

namespace lens {
GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify);

GURL AppendSearchContextParamToURL(const GURL& url_to_modify,
                                   std::optional<GURL> page_url,
                                   std::optional<std::string> page_title);

GURL BuildTextOnlySearchURL(
    const std::string& text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::map<std::string, std::string> additional_search_query_params);

GURL BuildLensSearchURL(
    std::optional<std::string> text_query,
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::LensOverlayClusterInfo cluster_info,
    std::map<std::string, std::string> additional_search_query_params);

// Returns the value of the text query parameter value from the provided search
// URL if any. Empty string otherwise.
const std::string GetTextQueryParameterValue(const GURL& url);

// Returns whether the given |url| contains all the common search query
// parameters required to properly enable the lens overlay results in the side
// panel. This does not check the value of these query parameters.
bool HasCommonSearchQueryParameters(const GURL& url);

// Returns whether the given |url| is a valid lens overlay search URL. This
// could differ from values in common APIs since the search URL is set via a
// finch configured flag.
bool IsValidSearchResultsUrl(const GURL& url);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_
