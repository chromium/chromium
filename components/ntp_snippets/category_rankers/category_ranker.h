// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CATEGORY_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CATEGORY_RANKER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"

namespace ntp_snippets {

// Orders categories.
// The order may be dynamic and change at any time.
class CategoryRanker {
 public:
  virtual ~CategoryRanker() = default;

  // Compares two categories. True means that |left| precedes |right|, i.e. it
  // must be located earlier (above) on the NTP. This method must satisfy
  // "Compare" contract required by sort algorithms.
  virtual bool Compare(Category left, Category right) const = 0;

  // Deletes all history related data between |begin| and |end|. After this
  // call, the category order may not depend on this history range anymore.
  virtual void ClearHistory(base::Time begin, base::Time end) = 0;

  // If |category| has not been added previously, it is added after all already
  // known categories, otherwise nothing is changed.
  virtual void AppendCategoryIfNecessary(Category category) = 0;

  // If |category_to_insert| has not been added previously, it is added before
  // |anchor|, otherwise nothing is changed.
  virtual void InsertCategoryBeforeIfNecessary(Category category_to_insert,
                                               Category anchor) = 0;

  // If |category_to_insert| has not been added previously, it is added after
  // |anchor|, otherwise nothing is changed.
  virtual void InsertCategoryAfterIfNecessary(Category category_to_insert,
                                              Category anchor) = 0;

  struct DebugDataItem {
    std::string label;
    std::string content;
    DebugDataItem(const std::string& label, const std::string& content)
        : label(label), content(content) {}
  };

  // Returns DebugData in form of pairs of strings (label; content),
  // e.g. describing internal state or parameter values.
  virtual std::vector<DebugDataItem> GetDebugData() = 0;

  // Feedback data from the user to update the ranking.

  // Called whenever a suggestion is opened by the user.
  virtual void OnSuggestionOpened(Category category) = 0;

  // Called whenever a category is dismissed by the user.
  virtual void OnCategoryDismissed(Category category) = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CATEGORY_RANKER_H_
