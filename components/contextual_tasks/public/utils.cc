// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_url_utils.h"
#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/docs_url_strip_handler.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/url_deduplication/url_strip_handler.h"
#include "components/visited_url_ranking/public/features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace contextual_tasks {

std::unique_ptr<url_deduplication::URLDeduplicationHelper>
CreateURLDeduplicationHelperForContextualTask() {
  std::vector<std::unique_ptr<url_deduplication::URLStripHandler>> handlers;
  url_deduplication::DeduplicationStrategy strategy =
      url_deduplication::DeduplicationStrategy();

  handlers.push_back(
      std::make_unique<url_deduplication::DocsURLStripHandler>());

  strategy.update_scheme = true;
  strategy.clear_username = true;
  strategy.clear_password = true;
  strategy.clear_ref = true;
  strategy.clear_port = true;

  // The title if passed here will be appended to the merge key and will be used
  // for dedup. If a caller doesn't want to include title for dedup, they should
  // send empty string as the title param in
  // `visited_url_ranking::ComputeURLMergeKey()`.
  strategy.include_title = true;

  // Intentionally treat different paths and query params as distinct URLs.
  strategy.clear_path = false;
  strategy.clear_query = false;

  // Certain URL prefixes should be excluded from deduplication.
  auto prefix_list = base::SplitString(
      visited_url_ranking::features::
          kVisitedURLRankingDeduplicationExcludedPrefixes.Get(),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  strategy.excluded_prefixes = prefix_list;

  return std::make_unique<url_deduplication::URLDeduplicationHelper>(
      std::move(handlers), strategy);
}

GURL GetDefaultAimUrl(const std::string& locale,
                      omnibox::ChromeAimEntryPoint entry_point) {
  GURL url = lens::AppendCommonSearchParametersToURL(
      GURL(GetContextualTasksAiPageUrl()), locale, false);
  return AppendAimEntryPointParams(url, entry_point);
}

GURL AppendAimEntryPointParams(GURL url,
                               omnibox::ChromeAimEntryPoint entry_point) {
  if (entry_point == omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT) {
    return url;
  }

  GURL new_url = url;
  auto invocation_source = GetLensInvocationSourceForAimZeroState(entry_point);
  if (invocation_source.has_value()) {
    new_url = lens::AppendInvocationSourceParamToURL(
        new_url, invocation_source.value(), /*is_contextual_tasks=*/true);
  }
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, "aep", base::NumberToString(static_cast<int>(entry_point)));
  return new_url;
}

std::optional<lens::LensOverlayInvocationSource>
GetLensInvocationSourceForAimZeroState(
    omnibox::ChromeAimEntryPoint entry_point) {
  switch (entry_point) {
    case omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON:
      return lens::LensOverlayInvocationSource::kCobrowseToolbarButton;
    default:
      return std::nullopt;
  }
}

}  // namespace contextual_tasks
