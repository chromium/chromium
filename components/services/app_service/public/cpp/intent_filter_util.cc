// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/url_constants.h"

namespace {

bool ConditionsHaveOverlap(const apps::mojom::ConditionPtr& condition1,
                           const apps::mojom::ConditionPtr& condition2) {
  if (condition1->condition_type != condition2->condition_type) {
    return false;
  }
  // If there are same |condition_value| exist in the both |condition|s, there
  // is an overlap.
  for (auto& value1 : condition1->condition_values) {
    for (auto& value2 : condition2->condition_values) {
      if (value1 == value2) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

namespace apps_util {

apps::mojom::ConditionValuePtr MakeConditionValue(
    const std::string& value,
    apps::mojom::PatternMatchType pattern_match_type) {
  auto condition_value = apps::mojom::ConditionValue::New();
  condition_value->value = value;
  condition_value->match_type = pattern_match_type;

  return condition_value;
}

apps::mojom::ConditionPtr MakeCondition(
    apps::mojom::ConditionType condition_type,
    std::vector<apps::mojom::ConditionValuePtr> condition_values) {
  auto condition = apps::mojom::Condition::New();
  condition->condition_type = condition_type;
  condition->condition_values = std::move(condition_values);

  return condition;
}

void AddSingleValueCondition(apps::mojom::ConditionType condition_type,
                             const std::string& value,
                             apps::mojom::PatternMatchType pattern_match_type,
                             apps::mojom::IntentFilterPtr& intent_filter) {
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(
      apps_util::MakeConditionValue(value, pattern_match_type));
  auto condition =
      apps_util::MakeCondition(condition_type, std::move(condition_values));
  intent_filter->conditions.push_back(std::move(condition));
}

apps::mojom::IntentFilterPtr CreateIntentFilterForUrlScope(
    const GURL& url,
    bool with_action_view) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  if (with_action_view) {
    AddSingleValueCondition(
        apps::mojom::ConditionType::kAction, apps_util::kIntentActionView,
        apps::mojom::PatternMatchType::kNone, intent_filter);
  }

  AddSingleValueCondition(apps::mojom::ConditionType::kScheme, url.scheme(),
                          apps::mojom::PatternMatchType::kNone, intent_filter);

  AddSingleValueCondition(apps::mojom::ConditionType::kHost, url.host(),
                          apps::mojom::PatternMatchType::kNone, intent_filter);

  AddSingleValueCondition(apps::mojom::ConditionType::kPattern, url.path(),
                          apps::mojom::PatternMatchType::kPrefix,
                          intent_filter);

  return intent_filter;
}

int GetFilterMatchLevel(const apps::mojom::IntentFilterPtr& intent_filter) {
  int match_level = IntentFilterMatchLevel::kNone;
  for (const auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kAction:
        // Action always need to be matched, so there is no need for
        // match level.
        break;
      case apps::mojom::ConditionType::kScheme:
        match_level += IntentFilterMatchLevel::kScheme;
        break;
      case apps::mojom::ConditionType::kHost:
        match_level += IntentFilterMatchLevel::kHost;
        break;
      case apps::mojom::ConditionType::kPattern:
        match_level += IntentFilterMatchLevel::kPattern;
        break;
      case apps::mojom::ConditionType::kMimeType:
        match_level += IntentFilterMatchLevel::kMimeType;
        break;
    }
  }
  return match_level;
}

bool FiltersHaveOverlap(const apps::mojom::IntentFilterPtr& filter1,
                        const apps::mojom::IntentFilterPtr& filter2) {
  if (GetFilterMatchLevel(filter1) != GetFilterMatchLevel(filter2)) {
    return false;
  }
  for (size_t i = 0; i < filter1->conditions.size(); i++) {
    auto& condition1 = filter1->conditions[i];
    auto& condition2 = filter2->conditions[i];
    if (!ConditionsHaveOverlap(condition1, condition2)) {
      return false;
    }
  }
  return true;
}

bool FilterNeedsUpgrade(const apps::mojom::IntentFilterPtr& filter) {
  for (const auto& condition : filter->conditions) {
    if (condition->condition_type == apps::mojom::ConditionType::kAction) {
      return false;
    }
  }
  return true;
}

void UpgradeFilter(apps::mojom::IntentFilterPtr& filter) {
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(apps_util::MakeConditionValue(
      apps_util::kIntentActionView, apps::mojom::PatternMatchType::kNone));
  auto condition = apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                            std::move(condition_values));
  filter->conditions.insert(filter->conditions.begin(), std::move(condition));
}

bool IsBrowserFilter(const apps::mojom::IntentFilterPtr& filter) {
  if (GetFilterMatchLevel(filter) != IntentFilterMatchLevel::kScheme) {
    return false;
  }
  for (const auto& condition : filter->conditions) {
    if (condition->condition_type != apps::mojom::ConditionType::kScheme) {
      continue;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->value == url::kHttpScheme ||
          condition_value->value == url::kHttpsScheme) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace apps_util
