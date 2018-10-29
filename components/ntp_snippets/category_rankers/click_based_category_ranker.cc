// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/time_serialization.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

// In order to increase stability and predictability of the order, an extra
// level of "confidence" is required before moving a category upwards. In other
// words, the category is moved not when it reaches the previous one, but rather
// when it leads by some amount. We refer to this required extra "confidence" as
// a passing margin. Each position has its own passing margin. The category is
// moved upwards (i.e. passes another category) when it has at least passing
// margin of the previous category position more clicks.
const int kDefaultPassingMargin = 5;
const char kPassingMarginParamName[] =
    "click_based_category_ranker-passing_margin";

// The first categories get more attention and, therefore, here more stability
// is needed. The passing margin of such categories is increased and they are
// referred to as top categories (with extra margin). Only category position
// defines whether a category is top, but not its content.
const int kDefaultNumTopCategoriesWithExtraMargin = 3;
const char kNumTopCategoriesWithExtraMarginParamName[] =
    "click_based_category_ranker-num_top_categories_with_extra_margin";

// The increase of passing margin for each top category compared to the next
// category (e.g. the first top category has passing margin larger by this value
// than the second top category, the last top category has it larger by this
// value than the first non-top category).
const int kDefaultExtraPassingMargin = 2;
const char kExtraPassingMarginParamName[] =
    "click_based_category_ranker-extra_passing_margin";

// The ranker must "forget" history with time, so that changes in the user
// behavior are reflected by the order in reasonable time. This is done using
// click count decay with time. However, if there is not enough data, there is
// no need in "forgetting" it. This value defines how many total clicks (across
// categories) are considered enough to decay.
const int kDefaultMinNumClicksToDecay = 30;
const char kMinNumClicksToDecayParamName[] =
    "click_based_category_ranker-min_num_clicks_to_decay";

// Time between two consecutive decays (assuming enough clicks).
const int kDefaultTimeBetweenDecaysMinutes = 24 * 60;  // 24 hours = 1 day
const char kTimeBetweenDecaysParamName[] =
    "click_based_category_ranker-time_between_decays_minutes";

// Decay factor as a fraction. The current value approximates the seventh root
// of 0.5. This yields a 50% decay per seven decays. Seven weak decays are used
// instead of one 50% decay in order to decrease difference of click weight in
// time.
const int kDefaultDecayFactorNumerator = 91;
const int kDefaultDecayFactorDenominator = 100;  // pow(0.91, 7) = 0.517
const char kDecayFactorNumeratorParamName[] =
    "click_based_category_ranker-decay_factor_numerator";
const char kDecayFactorDenominatorParamName[] =
    "click_based_category_ranker-decay_factor_denominator";

// Number of positions by which a dismissed category is downgraded.
const int kDefaultDismissedCategoryPenalty = 1;
const char kDismissedCategoryPenaltyParamName[] =
    "click_based_category_ranker-dismissed_category_penalty";

const char kCategoryIdKey[] = "category";
const char kClicksKey[] = "clicks";
const char kLastDismissedKey[] = "last_dismissed";
const char kContentSuggestionsPromotedCategory[] =
    "click_based_category_ranker-promoted_category";

int GetExtraPassingMargin() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kExtraPassingMarginParamName,
      kDefaultExtraPassingMargin);
}

int GetMinNumClicksToDecay() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kMinNumClicksToDecayParamName,
      kDefaultMinNumClicksToDecay);
}

base::TimeDelta GetTimeBetweenDecays() {
  return base::TimeDelta::FromMinutes(
      variations::GetVariationParamByFeatureAsInt(
          kCategoryRanker, kTimeBetweenDecaysParamName,
          kDefaultTimeBetweenDecaysMinutes));
}

int GetDecayFactorNumerator() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kDecayFactorNumeratorParamName,
      kDefaultDecayFactorNumerator);
}

int GetDecayFactorDenominator() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kDecayFactorDenominatorParamName,
      kDefaultDecayFactorDenominator);
}

base::Optional<Category> GetPromotedCategoryFromVariations() {
  int category_id = variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kContentSuggestionsPromotedCategory, -1);
  if (category_id < 0) {
    return base::nullopt;
  }
  if (!Category::IsValidIDValue(category_id)) {
    LOG(WARNING) << "Received invalid category ID for promotion: "
                 << category_id << ". Ignoring promotion.";
    return base::nullopt;
  }
  return Category::FromIDValue(category_id);
}

std::string GetOptionalCategoryAsString(
    const base::Optional<Category>& optional_category) {
  if (optional_category.has_value()) {
    return base::IntToString(optional_category->id());
  }
  return "None";
}

}  // namespace

ClickBasedCategoryRanker::ClickBasedCategoryRanker(PrefService* pref_service,
                                                   base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {
  if (!ReadOrderFromPrefs(&ordered_categories_)) {
    // TODO(crbug.com/676273): Handle adding new hardcoded KnownCategories to
    // existing order from prefs. Currently such new category is completely
    // ignored and may be never shown.
    RestoreDefaultOrder();
  }

  if (ReadLastDecayTimeFromPrefs() == DeserializeTime(0)) {
    StoreLastDecayTimeToPrefs(clock_->Now());
  }
  promoted_category_ = DeterminePromotedCategory();
}

// |ordered_categories_| needs to be properly initialized before calling
// this method.
base::Optional<Category> ClickBasedCategoryRanker::DeterminePromotedCategory() {
  base::Optional<Category> promoted = GetPromotedCategoryFromVariations();
  if (!promoted.has_value()) {
    return base::nullopt;
  }
  auto promoted_it = FindCategory(promoted.value());
  if (promoted_it != ordered_categories_.end() &&
      promoted_it->last_dismissed >
          clock_->Now() - base::TimeDelta::FromDays(14)) {
    // Only promote categories to the top if they weren't dismissed within the
    // last 2 weeks.
    return base::nullopt;
  }
  return promoted;
}

ClickBasedCategoryRanker::~ClickBasedCategoryRanker() = default;

bool ClickBasedCategoryRanker::Compare(Category left, Category right) const {
  if (!ContainsCategory(left)) {
    LOG(DFATAL) << "The category with ID " << left.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (!ContainsCategory(right)) {
    LOG(DFATAL) << "The category with ID " << right.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (left == right) {
    return false;
  }
  if (promoted_category_.has_value() && left == *promoted_category_) {
    return true;
  }
  if (promoted_category_.has_value() && right == *promoted_category_) {
    return false;
  }
  for (const RankedCategory& ranked_category : ordered_categories_) {
    if (ranked_category.category == left) {
      return true;
    }
    if (ranked_category.category == right) {
      return false;
    }
  }
  // This fallback is provided only to satisfy "Compare" contract if by mistake
  // categories are not added using AppendCategoryIfNecessary. One should not
  // rely on this, instead the order must be defined explicitly using
  // AppendCategoryIfNecessary.
  return left.id() < right.id();
}

void ClickBasedCategoryRanker::ClearHistory(base::Time begin, base::Time end) {
  // Ignore all partial removals and react only to "entire" history removal.
  bool is_entire_history = (begin == base::Time() && end == base::Time::Max());
  if (!is_entire_history) {
    return;
  }

  StoreLastDecayTimeToPrefs(DeserializeTime(0));

  // The categories added through |AppendCategoryIfNecessary| cannot be
  // completely removed, since no one is required to reregister them. Instead
  // they are preserved in the default order (sorted by id).
  std::vector<RankedCategory> old_categories = ordered_categories_;
  RestoreDefaultOrder();

  std::vector<Category> added_categories;
  for (const RankedCategory& old_category : old_categories) {
    auto it =
        std::find_if(ordered_categories_.begin(), ordered_categories_.end(),
                     [old_category](const RankedCategory& other) {
                       return other.category == old_category.category;
                     });
    if (it == ordered_categories_.end()) {
      added_categories.push_back(old_category.category);
    }
  }

  // Sort added categories by id to make their order history independent.
  std::sort(added_categories.begin(), added_categories.end(),
            Category::CompareByID());

  for (Category added_category : added_categories) {
    ordered_categories_.push_back(RankedCategory(
        added_category, /*clicks=*/0, /*last_dismissed=*/base::Time()));
  }

  StoreOrderToPrefs(ordered_categories_);
}

void ClickBasedCategoryRanker::AppendCategoryIfNecessary(Category category) {
  if (!ContainsCategory(category)) {
    ordered_categories_.push_back(RankedCategory(
        category, /*clicks=*/0, /*last_dismissed=*/base::Time()));
    StoreOrderToPrefs(ordered_categories_);
  }
}

void ClickBasedCategoryRanker::InsertCategoryBeforeIfNecessary(
    Category category_to_insert,
    Category anchor) {
  InsertCategoryRelativeToIfNecessary(category_to_insert, anchor,
                                      /*after=*/false);
}

void ClickBasedCategoryRanker::InsertCategoryAfterIfNecessary(
    Category category_to_insert,
    Category anchor) {
  InsertCategoryRelativeToIfNecessary(category_to_insert, anchor,
                                      /*after=*/true);
}

std::vector<CategoryRanker::DebugDataItem>
ClickBasedCategoryRanker::GetDebugData() {
  std::vector<CategoryRanker::DebugDataItem> result;
  result.push_back(
      CategoryRanker::DebugDataItem("Type", "ClickBasedCategoryRanker"));

  std::string initial_order_type;
  CategoryOrderChoice choice = GetSelectedCategoryOrder();
  if (choice == CategoryOrderChoice::GENERAL) {
    initial_order_type = "GENERAL";
  }
  if (choice == CategoryOrderChoice::EMERGING_MARKETS_ORIENTED) {
    initial_order_type = "EMERGING_MARKETS_ORIENTED;";
  }
  result.push_back(
      CategoryRanker::DebugDataItem("Initial order type", initial_order_type));

  std::vector<std::string> category_strings;
  for (const auto& ranked_category : ordered_categories_) {
    category_strings.push_back(base::ReplaceStringPlaceholders(
        "($1; $2)",
        {base::IntToString(ranked_category.category.id()),
         base::IntToString(ranked_category.clicks)},
        /*offsets=*/nullptr));
  }
  result.push_back(
      CategoryRanker::DebugDataItem("Current order (with click counts)",
                                    base::JoinString(category_strings, ", ")));

  result.push_back(CategoryRanker::DebugDataItem(
      "Actual promoted category",
      GetOptionalCategoryAsString(promoted_category_)));

  result.push_back(CategoryRanker::DebugDataItem(
      "Raw promoted category from variations",
      GetOptionalCategoryAsString(GetPromotedCategoryFromVariations())));

  return result;
}

void ClickBasedCategoryRanker::OnSuggestionOpened(Category category) {
  if (!ContainsCategory(category)) {
    LOG(DFATAL) << "The category with ID " << category.id()
                << " has not been added using AppendCategoryIfNecessary.";
    return;
  }

  DecayClicksIfNeeded();

  auto current = FindCategory(category);
  DCHECK_GE(current->clicks, 0);
  // The overflow is ignored. It is unlikely to happen, because of click count
  // decay.
  current->clicks++;

  // Move the category up if appropriate.
  if (current != ordered_categories_.begin()) {
    auto previous = current - 1;
    const int passing_margin = GetPositionPassingMargin(previous);
    if (current->clicks >= previous->clicks + passing_margin) {
      const int new_index = previous - ordered_categories_.begin();
      ntp_snippets::metrics::OnCategoryMovedUp(new_index);
      // It is intended to move only by one position per click in order to avoid
      // dramatic changes, which could confuse the user.
      std::swap(*current, *previous);
    }
  }

  StoreOrderToPrefs(ordered_categories_);
}

void ClickBasedCategoryRanker::OnCategoryDismissed(Category category) {
  if (!ContainsCategory(category)) {
    LOG(DFATAL) << "The category with ID " << category.id()
                << " has not been added using AppendCategoryIfNecessary.";
    return;
  }

  const int penalty = GetDismissedCategoryPenalty();
  if (penalty != 0) {  // Dismissed category penalty is turned on?
    auto current = FindCategory(category);
    for (int downgrade = 0; downgrade < penalty; ++downgrade) {
      auto next = current + 1;
      if (next == ordered_categories_.end()) {
        break;
      }
      std::swap(*current, *next);
      current = next;
    }

    DCHECK(current != ordered_categories_.begin());
    auto previous = current - 1;
    int new_clicks = std::max(previous->clicks - GetPassingMargin(), 0);
    // The previous category may have more clicks (but not enough to pass the
    // margin, this is possible when penalty >= 2), therefore, we ensure that
    // for this category we don't increase clicks.
    current->clicks = std::min(current->clicks, new_clicks);
  }
  FindCategory(category)->last_dismissed = clock_->Now();
  if (promoted_category_.has_value() && category == *promoted_category_) {
    promoted_category_.reset();
  }
  StoreOrderToPrefs(ordered_categories_);
}

base::Time ClickBasedCategoryRanker::GetLastDecayTime() const {
  return ReadLastDecayTimeFromPrefs();
}

// static
void ClickBasedCategoryRanker::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kClickBasedCategoryRankerOrderWithClicks);
  registry->RegisterInt64Pref(prefs::kClickBasedCategoryRankerLastDecayTime,
                              /*default_value=*/0);
}

// static
int ClickBasedCategoryRanker::GetPassingMargin() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kPassingMarginParamName, kDefaultPassingMargin);
}

// static
int ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kNumTopCategoriesWithExtraMarginParamName,
      kDefaultNumTopCategoriesWithExtraMargin);
}

// static
int ClickBasedCategoryRanker::GetDismissedCategoryPenalty() {
  return variations::GetVariationParamByFeatureAsInt(
      kCategoryRanker, kDismissedCategoryPenaltyParamName,
      kDefaultDismissedCategoryPenalty);
}

ClickBasedCategoryRanker::RankedCategory::RankedCategory(
    Category category,
    int clicks,
    const base::Time& last_dismissed)
    : category(category), clicks(clicks), last_dismissed(last_dismissed) {}

// Returns passing margin for a given position taking into account whether it is
// a top category.
int ClickBasedCategoryRanker::GetPositionPassingMargin(
    std::vector<RankedCategory>::const_iterator category_position) const {
  int index = category_position - ordered_categories_.cbegin();
  int passing_margin_increase = 0;
  const int num_top_categories_with_extra_margin =
      GetNumTopCategoriesWithExtraMargin();
  if (index < num_top_categories_with_extra_margin) {
    passing_margin_increase = GetExtraPassingMargin() *
                              (num_top_categories_with_extra_margin - index);
  }
  return GetPassingMargin() + passing_margin_increase;
}

void ClickBasedCategoryRanker::RestoreDefaultOrder() {
  ordered_categories_.clear();

  std::vector<KnownCategories> ordered_known_categories =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();

  for (KnownCategories known_category : ordered_known_categories) {
    AppendKnownCategory(known_category);
  }

  StoreOrderToPrefs(ordered_categories_);
}

void ClickBasedCategoryRanker::AppendKnownCategory(
    KnownCategories known_category) {
  Category category = Category::FromKnownCategory(known_category);
  DCHECK(!ContainsCategory(category));
  ordered_categories_.push_back(
      RankedCategory(category, /*clicks=*/0, /*last_dismissed=*/base::Time()));
}

namespace {

base::Time ParseLastDismissedDate(const base::DictionaryValue& value) {
  // We don't expect the last-dismissed value to be present in all cases (we
  // added this after the fact).
  std::string serialized_value;
  int64_t parsed_value;
  if (value.GetString(kLastDismissedKey, &serialized_value) &&
      base::StringToInt64(serialized_value, &parsed_value)) {
    return DeserializeTime(parsed_value);
  }
  return base::Time();
}

}  // namespace

bool ClickBasedCategoryRanker::ReadOrderFromPrefs(
    std::vector<RankedCategory>* result_categories) const {
  result_categories->clear();
  const base::ListValue* list =
      pref_service_->GetList(prefs::kClickBasedCategoryRankerOrderWithClicks);
  if (!list || list->GetSize() == 0) {
    return false;
  }

  for (const base::Value& value : *list) {
    const base::DictionaryValue* dictionary;
    if (!value.GetAsDictionary(&dictionary)) {
      LOG(DFATAL) << "Failed to parse category data from prefs param "
                  << prefs::kClickBasedCategoryRankerOrderWithClicks
                  << " into dictionary.";
      return false;
    }
    int category_id, clicks;
    if (!dictionary->GetInteger(kCategoryIdKey, &category_id)) {
      LOG(DFATAL) << "Dictionary does not have '" << kCategoryIdKey << "' key.";
      return false;
    }
    if (!dictionary->GetInteger(kClicksKey, &clicks)) {
      LOG(DFATAL) << "Dictionary does not have '" << kClicksKey << "' key.";
      return false;
    }
    base::Time last_dismissed = ParseLastDismissedDate(*dictionary);
    Category category = Category::FromIDValue(category_id);
    result_categories->push_back(
        RankedCategory(category, clicks, last_dismissed));
  }
  return true;
}

void ClickBasedCategoryRanker::StoreOrderToPrefs(
    const std::vector<RankedCategory>& ordered_categories) {
  base::ListValue list;
  for (const RankedCategory& category : ordered_categories) {
    auto dictionary = std::make_unique<base::DictionaryValue>();
    dictionary->SetInteger(kCategoryIdKey, category.category.id());
    dictionary->SetInteger(kClicksKey, category.clicks);
    dictionary->SetString(
        kLastDismissedKey,
        base::Int64ToString(SerializeTime(category.last_dismissed)));
    list.Append(std::move(dictionary));
  }
  pref_service_->Set(prefs::kClickBasedCategoryRankerOrderWithClicks, list);
}

std::vector<ClickBasedCategoryRanker::RankedCategory>::iterator
ClickBasedCategoryRanker::FindCategory(Category category) {
  return std::find_if(ordered_categories_.begin(), ordered_categories_.end(),
                      [category](const RankedCategory& ranked_category) {
                        return category == ranked_category.category;
                      });
}

bool ClickBasedCategoryRanker::ContainsCategory(Category category) const {
  for (const auto& ranked_category : ordered_categories_) {
    if (category == ranked_category.category) {
      return true;
    }
  }
  return false;
}

void ClickBasedCategoryRanker::InsertCategoryRelativeToIfNecessary(
    Category category_to_insert,
    Category anchor,
    bool after) {
  DCHECK(ContainsCategory(anchor));
  if (ContainsCategory(category_to_insert)) {
    return;
  }

  auto anchor_it = FindCategory(anchor);
  ordered_categories_.insert(anchor_it + (after ? 1 : 0),
                             RankedCategory(category_to_insert,
                                            /*clicks=*/anchor_it->clicks,
                                            /*last_dismissed=*/base::Time()));
  StoreOrderToPrefs(ordered_categories_);
}

base::Time ClickBasedCategoryRanker::ReadLastDecayTimeFromPrefs() const {
  return DeserializeTime(
      pref_service_->GetInt64(prefs::kClickBasedCategoryRankerLastDecayTime));
}

void ClickBasedCategoryRanker::StoreLastDecayTimeToPrefs(
    base::Time last_decay_time) {
  pref_service_->SetInt64(prefs::kClickBasedCategoryRankerLastDecayTime,
                          SerializeTime(last_decay_time));
}

bool ClickBasedCategoryRanker::IsEnoughClicksToDecay() const {
  int64_t num_clicks = 0;
  for (const RankedCategory& ranked_category : ordered_categories_) {
    num_clicks += ranked_category.clicks;
  }
  return num_clicks >= GetMinNumClicksToDecay();
}

bool ClickBasedCategoryRanker::DecayClicksIfNeeded() {
  base::Time now = clock_->Now();
  base::Time last_decay = ReadLastDecayTimeFromPrefs();
  if (last_decay == base::Time::FromInternalValue(0)) {
    // No last decay time, start from now.
    StoreLastDecayTimeToPrefs(clock_->Now());
    return false;
  }
  DCHECK_LE(last_decay, now);

  int num_pending_decays = (now - last_decay) / GetTimeBetweenDecays();
  int executed_decays = 0;
  while (executed_decays < num_pending_decays && IsEnoughClicksToDecay()) {
    for (RankedCategory& ranked_category : ordered_categories_) {
      DCHECK_GE(ranked_category.clicks, 0);
      const int64_t old_clicks = static_cast<int64_t>(ranked_category.clicks);
      ranked_category.clicks =
          old_clicks * GetDecayFactorNumerator() / GetDecayFactorDenominator();
    }

    ++executed_decays;
  }

  // No matter how many decays were actually executed, all of them are marked
  // done. Even if some were ignored due to absense of clicks, they would have
  // no effect anyway for the same reason.
  StoreLastDecayTimeToPrefs(last_decay +
                            num_pending_decays * GetTimeBetweenDecays());

  if (executed_decays > 0) {
    StoreOrderToPrefs(ordered_categories_);
    return true;
  }
  return false;
}

}  // namespace ntp_snippets
