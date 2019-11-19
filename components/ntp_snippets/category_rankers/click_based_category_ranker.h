// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CLICK_BASED_CATEGORY_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CLICK_BASED_CATEGORY_RANKER_H_

#include <memory>
#include <vector>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets {

// An implementation of a CategoryRanker based on a number of clicks per
// category. Initial order is hardcoded, but sections with more clicks are moved
// to the top. The new remote categories must be registered using
// AppendCategoryIfNecessary. All other categories must be hardcoded in the
// initial order. The order and category usage data are persisted in prefs and
// reloaded on startup.
// TODO(crbug.com/675929): Remove unused categories from prefs.
class ClickBasedCategoryRanker : public CategoryRanker {
 public:
  explicit ClickBasedCategoryRanker(PrefService* pref_service,
                                    base::Clock* clock);
  ~ClickBasedCategoryRanker() override;

  // CategoryRanker implementation.
  bool Compare(Category left, Category right) const override;
  void ClearHistory(base::Time begin, base::Time end) override;
  void AppendCategoryIfNecessary(Category category) override;
  void InsertCategoryBeforeIfNecessary(Category category_to_insert,
                                       Category anchor) override;
  void InsertCategoryAfterIfNecessary(Category category_to_insert,
                                      Category anchor) override;
  std::vector<CategoryRanker::DebugDataItem> GetDebugData() override;
  void OnSuggestionOpened(Category category) override;
  void OnCategoryDismissed(Category category) override;

  // Returns time when last decay occured. For testing only.
  base::Time GetLastDecayTime() const;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns passing margin, i.e. a number of extra clicks required to move a
  // category upwards. For testing only.
  static int GetPassingMargin();

  // Returns number of top categories with extra margin (i.e. with increased
  // passing margin). For testing only.
  static int GetNumTopCategoriesWithExtraMargin();

  // Returns number of positions by which a dismissed category is downgraded.
  // For testing only.
  static int GetDismissedCategoryPenalty();

 private:
  struct RankedCategory {
    Category category;
    int clicks;
    base::Time last_dismissed;

    RankedCategory(Category category,
                   int clicks,
                   const base::Time& last_dismissed);
  };

  int GetPositionPassingMargin(
      std::vector<RankedCategory>::const_iterator category_position) const;
  void RestoreDefaultOrder();
  void AppendKnownCategory(KnownCategories known_category);
  bool ReadOrderFromPrefs(std::vector<RankedCategory>* result_categories) const;
  void StoreOrderToPrefs(const std::vector<RankedCategory>& ordered_categories);
  std::vector<RankedCategory>::iterator FindCategory(Category category);
  bool ContainsCategory(Category category) const;
  void InsertCategoryRelativeToIfNecessary(Category category_to_insert,
                                           Category anchor,
                                           bool after);

  base::Time ReadLastDecayTimeFromPrefs() const;
  void StoreLastDecayTimeToPrefs(base::Time last_decay_time);
  bool IsEnoughClicksToDecay() const;
  bool DecayClicksIfNeeded();

  std::vector<RankedCategory> ordered_categories_;
  PrefService* pref_service_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(ClickBasedCategoryRanker);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CLICK_BASED_CATEGORY_RANKER_H_
