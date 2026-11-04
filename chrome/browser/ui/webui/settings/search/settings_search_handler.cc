// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_handler.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/settings/search/mojom/settings_search.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"

namespace settings_search {

SettingsSearchHandler::SettingsSearchHandler(
    mojo::PendingReceiver<mojom::SettingsSearchHandler> receiver,
    SettingsSearchRegistry* registry)
    : registry_(registry), receiver_(this, std::move(receiver)) {
  CHECK(features::IsSettingsSearch2026Enabled());
  CHECK(registry_);
}

SettingsSearchHandler::~SettingsSearchHandler() = default;

void SettingsSearchHandler::Search(const std::u16string& query,
                                   uint32_t max_results,
                                   SearchCallback callback) {
  std::u16string_view bounded_query =
      base::TrimWhitespace(query, base::TRIM_ALL).substr(0, kMaxQueryLength);
  // Some characters, like most emoji, take two UTF-16 code units. If the
  // truncation split one in half, drop the leftover first half.
  if (bounded_query.size() == kMaxQueryLength &&
      CBU16_IS_LEAD(bounded_query.back())) {
    bounded_query.remove_suffix(1);
  }
  bounded_query = base::TrimWhitespace(bounded_query, base::TRIM_TRAILING);
  max_results = std::min(max_results, kMaxResults);
  if (max_results == 0 || !FuzzyFinder::HasMinQueryLength(bounded_query)) {
    std::move(callback).Run({});
    return;
  }

  std::vector<raw_ptr<SettingsSearchItem>> items = registry_->GetItems();
  FuzzyFinder finder(
      std::vector<raw_ptr<FuzzySearchItem>>(items.begin(), items.end()));
  std::vector<mojom::SearchResultPtr> results;
  for (const FuzzySearchResult& match :
       finder.FuzzyFind(bounded_query, max_results)) {
    // The registry only holds SettingsSearchItems, so the downcast is safe.
    const auto* item = static_cast<const SettingsSearchItem*>(match.item.get());
    results.push_back(mojom::SearchResult::New(
        item->id(), item->route_path(), item->GetTitle(), item->breadcrumbs(),
        match.score, match.match_ranges));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace settings_search
