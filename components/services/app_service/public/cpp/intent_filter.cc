// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter.h"

namespace apps {

ConditionValue::ConditionValue(const std::string& value,
                               PatternMatchType match_type)
    : value(value), match_type(match_type) {}

ConditionValue::~ConditionValue() = default;

bool ConditionValue::operator==(const ConditionValue& other) const {
  return value == other.value && match_type == other.match_type;
}

bool ConditionValue::operator!=(const ConditionValue& other) const {
  return !(*this == other);
}

Condition::Condition(ConditionType condition_type,
                     ConditionValues condition_values)
    : condition_type(condition_type),
      condition_values(std::move(condition_values)) {}

Condition::~Condition() = default;

bool Condition::operator==(const Condition& other) const {
  if (condition_values.size() != other.condition_values.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(condition_values.size()); i++) {
    if (*condition_values[i] != *other.condition_values[i]) {
      return false;
    }
  }

  return condition_type == other.condition_type;
}

bool Condition::operator!=(const Condition& other) const {
  return !(*this == other);
}

ConditionPtr Condition::Clone() const {
  ConditionValues values;
  for (const auto& condition_value : condition_values) {
    values.push_back(std::make_unique<ConditionValue>(
        condition_value->value, condition_value->match_type));
  }

  return std::make_unique<Condition>(condition_type, std::move(values));
}

IntentFilter::IntentFilter() = default;
IntentFilter::~IntentFilter() = default;

bool IntentFilter::operator==(const IntentFilter& other) const {
  if (conditions.size() != other.conditions.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(conditions.size()); i++) {
    if (*conditions[i] != *other.conditions[i]) {
      return false;
    }
  }

  return activity_name == other.activity_name &&
         activity_label == other.activity_label;
}

bool IntentFilter::operator!=(const IntentFilter& other) const {
  return !(*this == other);
}

IntentFilterPtr IntentFilter::Clone() const {
  IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();

  for (const auto& condition : conditions) {
    intent_filter->conditions.push_back(condition->Clone());
  }

  if (activity_name.has_value())
    intent_filter->activity_name = activity_name.value();

  if (activity_label.has_value())
    intent_filter->activity_label = activity_label.value();

  return intent_filter;
}

IntentFilters CloneIntentFilters(const IntentFilters& intent_filters) {
  IntentFilters ret;
  for (const auto& intent_filter : intent_filters) {
    ret.push_back(intent_filter->Clone());
  }
  return ret;
}

bool IsEqual(const IntentFilters& source, const IntentFilters& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

ConditionType ConvertMojomConditionTypeToConditionType(
    const apps::mojom::ConditionType& mojom_condition_type) {
  switch (mojom_condition_type) {
    case apps::mojom::ConditionType::kScheme:
      return ConditionType::kScheme;
    case apps::mojom::ConditionType::kHost:
      return ConditionType::kHost;
    case apps::mojom::ConditionType::kPattern:
      return ConditionType::kPattern;
    case apps::mojom::ConditionType::kAction:
      return ConditionType::kAction;
    case apps::mojom::ConditionType::kMimeType:
      return ConditionType::kMimeType;
    case apps::mojom::ConditionType::kFile:
      return ConditionType::kFile;
  }
}

apps::mojom::ConditionType ConvertConditionTypeToMojomConditionType(
    const ConditionType& condition_type) {
  switch (condition_type) {
    case ConditionType::kScheme:
      return apps::mojom::ConditionType::kScheme;
    case ConditionType::kHost:
      return apps::mojom::ConditionType::kHost;
    case ConditionType::kPattern:
      return apps::mojom::ConditionType::kPattern;
    case ConditionType::kAction:
      return apps::mojom::ConditionType::kAction;
    case ConditionType::kMimeType:
      return apps::mojom::ConditionType::kMimeType;
    case ConditionType::kFile:
      return apps::mojom::ConditionType::kFile;
  }
}

PatternMatchType ConvertMojomPatternMatchTypeToPatternMatchType(
    const apps::mojom::PatternMatchType& mojom_pattern_match_type) {
  switch (mojom_pattern_match_type) {
    case apps::mojom::PatternMatchType::kNone:
      return PatternMatchType::kNone;
    case apps::mojom::PatternMatchType::kLiteral:
      return PatternMatchType::kLiteral;
    case apps::mojom::PatternMatchType::kPrefix:
      return PatternMatchType::kPrefix;
    case apps::mojom::PatternMatchType::kGlob:
      return PatternMatchType::kGlob;
    case apps::mojom::PatternMatchType::kMimeType:
      return PatternMatchType::kMimeType;
    case apps::mojom::PatternMatchType::kFileExtension:
      return PatternMatchType::kFileExtension;
    case apps::mojom::PatternMatchType::kIsDirectory:
      return PatternMatchType::kIsDirectory;
    case apps::mojom::PatternMatchType::kSuffix:
      return PatternMatchType::kSuffix;
  }
}

apps::mojom::PatternMatchType ConvertPatternMatchTypeToMojomPatternMatchType(
    const PatternMatchType& pattern_match_type) {
  switch (pattern_match_type) {
    case PatternMatchType::kNone:
      return apps::mojom::PatternMatchType::kNone;
    case PatternMatchType::kLiteral:
      return apps::mojom::PatternMatchType::kLiteral;
    case PatternMatchType::kPrefix:
      return apps::mojom::PatternMatchType::kPrefix;
    case PatternMatchType::kGlob:
      return apps::mojom::PatternMatchType::kGlob;
    case PatternMatchType::kMimeType:
      return apps::mojom::PatternMatchType::kMimeType;
    case PatternMatchType::kFileExtension:
      return apps::mojom::PatternMatchType::kFileExtension;
    case PatternMatchType::kIsDirectory:
      return apps::mojom::PatternMatchType::kIsDirectory;
    case PatternMatchType::kSuffix:
      return apps::mojom::PatternMatchType::kSuffix;
  }
}

ConditionValuePtr ConvertMojomConditionValueToConditionValue(
    const apps::mojom::ConditionValuePtr& mojom_condition_value) {
  if (!mojom_condition_value) {
    return nullptr;
  }

  ConditionValuePtr condition_value = std::make_unique<ConditionValue>(
      mojom_condition_value->value,
      ConvertMojomPatternMatchTypeToPatternMatchType(
          mojom_condition_value->match_type));
  return condition_value;
}

apps::mojom::ConditionValuePtr ConvertConditionValueToMojomConditionValue(
    const ConditionValuePtr& condition_value) {
  auto mojom_condition_value = apps::mojom::ConditionValue::New();
  if (!condition_value) {
    return mojom_condition_value;
  }

  mojom_condition_value->value = condition_value->value;
  mojom_condition_value->match_type =
      ConvertPatternMatchTypeToMojomPatternMatchType(
          condition_value->match_type);
  return mojom_condition_value;
}

ConditionPtr ConvertMojomConditionToCondition(
    const apps::mojom::ConditionPtr& mojom_condition) {
  if (!mojom_condition) {
    return nullptr;
  }

  ConditionValues values;
  for (const auto& condition_value : mojom_condition->condition_values) {
    values.push_back(
        ConvertMojomConditionValueToConditionValue(condition_value));
  }
  return std::make_unique<Condition>(
      ConvertMojomConditionTypeToConditionType(mojom_condition->condition_type),
      std::move(values));
}

apps::mojom::ConditionPtr ConvertConditionToMojomCondition(
    const ConditionPtr& condition) {
  auto mojom_condition = apps::mojom::Condition::New();
  if (!condition) {
    return mojom_condition;
  }

  mojom_condition->condition_type =
      ConvertConditionTypeToMojomConditionType(condition->condition_type);

  for (const auto& condition_value : condition->condition_values) {
    if (condition_value) {
      mojom_condition->condition_values.push_back(
          ConvertConditionValueToMojomConditionValue(condition_value));
    }
  }
  return mojom_condition;
}

IntentFilterPtr ConvertMojomIntentFilterToIntentFilter(
    const apps::mojom::IntentFilterPtr& mojom_intent_filter) {
  if (!mojom_intent_filter) {
    return nullptr;
  }

  IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();
  for (const auto& condition : mojom_intent_filter->conditions) {
    if (condition) {
      intent_filter->conditions.push_back(
          ConvertMojomConditionToCondition(condition));
    }
  }

  if (mojom_intent_filter->activity_name.has_value())
    intent_filter->activity_name = mojom_intent_filter->activity_name.value();

  if (mojom_intent_filter->activity_label.has_value())
    intent_filter->activity_label = mojom_intent_filter->activity_label.value();

  return intent_filter;
}

apps::mojom::IntentFilterPtr ConvertIntentFilterToMojomIntentFilter(
    const IntentFilterPtr& intent_filter) {
  auto mojom_intent_filter = apps::mojom::IntentFilter::New();
  if (!intent_filter) {
    return mojom_intent_filter;
  }

  for (const auto& condition : intent_filter->conditions) {
    if (condition) {
      mojom_intent_filter->conditions.push_back(
          ConvertConditionToMojomCondition(condition));
    }
  }

  mojom_intent_filter->activity_name = intent_filter->activity_name;
  mojom_intent_filter->activity_label = intent_filter->activity_label;
  return mojom_intent_filter;
}

base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>>
ConvertIntentFiltersToMojomIntentFilters(
    const base::flat_map<std::string, apps::IntentFilters>& intent_filter) {
  base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>> ret;
  for (const auto& it : intent_filter) {
    std::vector<apps::mojom::IntentFilterPtr> mojom_filters;
    for (const auto& filter_it : it.second) {
      if (filter_it) {
        mojom_filters.push_back(
            ConvertIntentFilterToMojomIntentFilter(filter_it));
      }
    }
    ret[it.first] = std::move(mojom_filters);
  }
  return ret;
}

base::flat_map<std::string, apps::IntentFilters>
ConvertMojomIntentFiltersToIntentFilters(
    const base::flat_map<std::string,
                         std::vector<apps::mojom::IntentFilterPtr>>&
        mojom_intent_filter) {
  base::flat_map<std::string, apps::IntentFilters> ret;
  for (const auto& it : mojom_intent_filter) {
    apps::IntentFilters filters;
    for (const auto& filter_it : it.second) {
      if (filter_it)
        filters.push_back(ConvertMojomIntentFilterToIntentFilter(filter_it));
    }
    ret[it.first] = std::move(filters);
  }
  return ret;
}

}  // namespace apps
