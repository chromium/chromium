// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_

#include <map>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "url/gurl.h"

namespace lens {

class LensOverlayClusterInfo;
class LensOverlayRequestId;

void AppendTranslateParamsToMap(std::map<std::string, std::string>& params,
                                const std::string& query,
                                const std::string& content_language);

void AppendStickinessSignalForFormula(
    std::map<std::string, std::string>& params,
    const std::string& formula);

void AppendLensOverlaySidePanelParams(
    std::map<std::string, std::string>& params,
    uint64_t gen204_id,
    bool has_text,
    bool has_image);

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify,
                                       bool use_dark_mode);

GURL AppendVideoContextParamToURL(const GURL& url_to_modify,
                                  std::optional<GURL> page_url);

GURL AppendDarkModeParamToURL(const GURL& url_to_modify, bool use_dark_mode);

GURL BuildTextOnlySearchURL(
    base::Time query_start_time,
    const std::string& text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source,
    lens::LensOverlaySelectionType lens_selection_type,
    bool use_dark_mode);

GURL BuildLensSearchURL(
    base::Time query_start_time,
    std::optional<std::string> text_query,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::LensOverlayClusterInfo cluster_info,
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode);

// Returns the value of the text query parameter value from the provided search
// URL if any. Empty string otherwise.
const std::string ExtractTextQueryParameterValue(const GURL& url);

// Returns the value of the lens mode parameter value from the provided search
// URL if any. Empty string otherwise.
const std::string ExtractLensModeParameterValue(const GURL& url);

// Returns true if the two URLs have the same base url, and the same query
// parameters. This differs from comparing two GURLs using == since this method
// will ensure equivalence even if there are empty query params, viewport
// params, or different query param ordering.
bool AreSearchUrlsEquivalent(const GURL& a, const GURL& b);

// Returns whether the given |url| contains all the common search query
// parameters required to properly enable the lens overlay results in the side
// panel. This does not check the value of these query parameters.
bool HasCommonSearchQueryParameters(const GURL& url);

// Returns whether the given |url| is a valid lens overlay search URL. This
// could differ from values in common APIs since the search URL is set via a
// finch configured flag.
bool IsValidSearchResultsUrl(const GURL& url);

// Returns whether the given |url| is an AIM URL.
bool IsAimQuery(const GURL& url);

// Returns whether the `url` is a valid lens overlay search URL but contains
// parameters known not to be supported in the side panel and thus should be
// opened in a new tab. `is_aim_feature_enabled` indicates whether the AIM M3
// feature is enabled, and should be passed in via the lens::IsAimM3Enabled from
// lens_search_feature_flag_utils. This function keeps a bool to keep
// dependencies light and testing easy
bool ShouldOpenSearchURLInNewTab(const GURL& url, bool is_aim_feature_enabled);

// Returns whether the given |url| is a valid lens overlay search redirect URL.
// This could differ from values in common APIs since the search URL is set via
// a finch configured flag.
GURL GetSearchResultsUrlFromRedirectUrl(const GURL& url);

// Removes parameters that frequently change on the SRP URL due to redirects or
// client changes without changing the actual results. This allows us to compare
// search url's accurately in AddQueryToHistory when the side panel is resized
// or when the SRP redirects to append parameters unrelated to the search
// results.
GURL RemoveIgnoredSearchURLParameters(const GURL& url);

// Returns the URL to open in a new tab by adding a unique vsrid to the side
// panel new tab URL. If the given URL is empty, or is a URL for a contextual
// query, returns an empty URL since they cannot be opened in a new tab.
GURL GetSidePanelNewTabUrl(const GURL& side_panel_url, std::string vsrid);

// Builds the appropriate translate service URL for fetching supported
// languages.
GURL BuildTranslateLanguagesURL(std::string_view country,
                                std::string_view language);

// Returns whether |lens_selection_type| should be considered as a text-only
// selection type.
bool IsLensTextSelectionType(
    lens::LensOverlaySelectionType lens_selection_type);

// Returns whether `first_url` is equal to `second_url` when the text fragment
// is stripped from the ref if it exists at all. This fragment is stripped from
// both URLs.
bool URLsMatchWithoutTextFragment(const GURL& first_url,
                                  const GURL& second_url);

// Adds the `text_fragments` and `pdf_page_number` to the ref attribute of `url`
// without modifying any part of the rest of the URL. Any information in the
// current ref of `url` is discarded.
GURL AddPDFScrollToParametersToUrl(
    const GURL& url,
    const std::vector<std::string>& text_fragments,
    int pdf_page_number);

// Return the time from a `t=` parameter if it exists.
std::optional<base::TimeDelta> ExtractTimeInSecondsFromQueryIfExists(
    const GURL& target);

// Return the video ID if it's set in `url`.
std::optional<std::string> ExtractVideoNameIfExists(const GURL& url);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_URL_BUILDER_H_
