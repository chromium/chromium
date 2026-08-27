// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/search_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/i18n/string_search.h"
#include "base/strings/utf_string_conversions.h"

SearchHandler::SearchHandler(
    mojo::PendingReceiver<tab_search::mojom::SearchHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

SearchHandler::~SearchHandler() = default;

void SearchHandler::GetRangesIgnoringCaseAndAccents(
    const std::string& search_text,
    const std::vector<std::string>& targets,
    GetRangesIgnoringCaseAndAccentsCallback callback) {
  std::vector<std::vector<tab_search::mojom::TokenRangePtr>> results;
  results.reserve(targets.size());

  std::u16string find_this = base::UTF8ToUTF16(search_text);

  if (find_this.empty()) {
    for (size_t i = 0; i < targets.size(); ++i) {
      results.emplace_back();
    }
    std::move(callback).Run(std::move(results));
    return;
  }

  for (const auto& target : targets) {
    std::vector<tab_search::mojom::TokenRangePtr> ranges;
    std::u16string in_this = base::UTF8ToUTF16(target);
    base::i18n::RepeatingStringSearch searcher(find_this, in_this,
                                               /*case_sensitive=*/false);

    int match_index = 0;
    int match_length = 0;
    while (searcher.NextMatchResult(match_index, match_length)) {
      auto range = tab_search::mojom::TokenRange::New();
      range->start = match_index;
      range->length = match_length;
      ranges.push_back(std::move(range));
    }
    results.push_back(std::move(ranges));
  }

  std::move(callback).Run(std::move(results));
}
