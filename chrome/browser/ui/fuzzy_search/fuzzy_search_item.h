// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_
#define CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_

#include <string>

#include "base/containers/span.h"

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
  virtual base::span<const std::u16string> GetSynonyms() const = 0;

  // Returns a bonus in [0.0, 1.0] that FuzzyFinder::FuzzyFind() adds to this
  // item's match score when ordering results, or 0.0 for no bonus. A small
  // bonus ranks the item above matches of similar quality; a large one ranks
  // it above better matches. An item that does not match stays unmatched.
  virtual double GetBonusWeight() const = 0;
};

#endif  // CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_ITEM_H_
