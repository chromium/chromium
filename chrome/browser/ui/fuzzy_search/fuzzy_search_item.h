// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_
#define CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_

#include <string>
#include <vector>

// The FuzzySearchItem is an interface used to represent the searchable terms
// through the fuzzy search finder.
class FuzzySearchItem {
 public:
  virtual ~FuzzySearchItem() = default;

  // Returns the primary display title/label of the search item.
  virtual const std::u16string& GetTitle() const = 0;

  // Returns the secondary or contextual text (e.g., section header) for the
  // item.
  virtual const std::u16string& GetSecondaryText() const = 0;

  // Returns alternative terms/keywords used for fuzzy search matching.
  virtual const std::vector<std::u16string>& GetSynonyms() const = 0;
};

#endif  // CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_
