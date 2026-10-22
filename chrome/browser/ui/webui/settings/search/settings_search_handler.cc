// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_handler.h"

#include <algorithm>
#include <memory>
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

namespace settings_search {

namespace {

std::vector<raw_ptr<FuzzySearchItem>> ToFuzzySearchItems(
    const std::vector<std::unique_ptr<SettingsSearchItem>>& items) {
  std::vector<raw_ptr<FuzzySearchItem>> result;
  result.reserve(items.size());
  for (const auto& item : items) {
    result.push_back(item.get());
  }
  return result;
}

}  // namespace

SettingsSearchHandler::SettingsSearchHandler(
    mojo::PendingReceiver<mojom::SettingsSearchHandler> receiver,
    std::vector<std::unique_ptr<SettingsSearchItem>> items)
    : items_(std::move(items)),
      finder_(ToFuzzySearchItems(items_)),
      receiver_(this, std::move(receiver)) {
  CHECK(features::IsSettingsSearch2026Enabled());
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
  max_results = std::min(max_results, kMaxResults);

  std::vector<mojom::SearchResultPtr> results;
  for (const FuzzySearchResult& match :
       finder_.FuzzyFind(bounded_query, max_results)) {
    // `finder_` only hands back pointers to `items_`, so the downcast is safe.
    const auto* item = static_cast<const SettingsSearchItem*>(match.item.get());
    results.push_back(mojom::SearchResult::New(
        item->id(), item->route_path(), item->GetTitle(), item->breadcrumbs(),
        match.score, match.match_ranges));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace settings_search
