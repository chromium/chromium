// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/docs_url_strip_handler.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/url_deduplication/url_strip_handler.h"
#include "components/visited_url_ranking/public/features.h"

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

}  // namespace contextual_tasks
