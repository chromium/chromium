// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "components/services/app_service/public/cpp/intent_filter.h"

#include "url/url_constants.h"

namespace apps {

APP_ENUM_TO_STRING(ConditionType,
                   kScheme,
                   kAuthority,
                   kPath,
                   kAction,
                   kMimeType,
                   kFile)

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

std::string ConditionValue::ToString() const {
  std::stringstream out;
  if (match_type == PatternMatchType::kSuffix) {
    out << "*";
  }
  out << value;
  if (match_type == PatternMatchType::kPrefix) {
    out << "*";
  }
  return out.str();
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

std::string Condition::ToString() const {
  std::stringstream out;
  out << " - " << EnumToString(condition_type) << ":";
  for (const auto& condition_value : condition_values) {
    out << " " << condition_value->ToString();
  }
  return out.str();
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

void IntentFilter::AddSingleValueCondition(
    ConditionType condition_type,
    const std::string& value,
    PatternMatchType pattern_match_type) {
  ConditionValues condition_values;
  condition_values.push_back(
      std::make_unique<ConditionValue>(value, pattern_match_type));
  conditions.push_back(
      std::make_unique<Condition>(condition_type, std::move(condition_values)));
}

int IntentFilter::GetFilterMatchLevel() {
  int match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  for (const auto& condition : conditions) {
    switch (condition->condition_type) {
      case ConditionType::kAction:
        // Action always need to be matched, so there is no need for
        // match level.
        break;
      case ConditionType::kScheme:
        match_level += static_cast<int>(IntentFilterMatchLevel::kScheme);
        break;
      case ConditionType::kAuthority:
        match_level += static_cast<int>(IntentFilterMatchLevel::kAuthority);
        break;
      case ConditionType::kPath:
        match_level += static_cast<int>(IntentFilterMatchLevel::kPath);
        break;
      case ConditionType::kMimeType:
      case ConditionType::kFile:
        match_level += static_cast<int>(IntentFilterMatchLevel::kMimeType);
        break;
    }
  }
  return match_level;
}

void IntentFilter::GetMimeTypesAndExtensions(
    std::set<std::string>& mime_types,
    std::set<std::string>& file_extensions) {
  for (const auto& condition : conditions) {
    if (condition->condition_type != ConditionType::kFile) {
      continue;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->match_type == PatternMatchType::kFileExtension) {
        file_extensions.insert(condition_value->value);
      }
      if (condition_value->match_type == PatternMatchType::kMimeType) {
        mime_types.insert(condition_value->value);
      }
    }
  }
}

bool IntentFilter::IsBrowserFilter() {
  if (GetFilterMatchLevel() !=
      static_cast<int>(IntentFilterMatchLevel::kScheme)) {
    return false;
  }
  for (const auto& condition : conditions) {
    if (condition->condition_type != ConditionType::kScheme) {
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

bool IntentFilter::IsFileExtensionsFilter() {
  for (const auto& condition : conditions) {
    // We expect action conditions to be paired with file conditions.
    if (condition->condition_type == ConditionType::kAction) {
      continue;
    }
    if (condition->condition_type != ConditionType::kFile) {
      return false;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->match_type != PatternMatchType::kFileExtension) {
        return false;
      }
    }
  }
  return true;
}

bool IntentFilter::FilterNeedsUpgrade() {
  for (const auto& condition : conditions) {
    if (condition->condition_type == ConditionType::kAction) {
      return false;
    }
  }
  return true;
}

std::string IntentFilter::ToString() const {
  std::stringstream out;
  if (activity_name.has_value()) {
    out << " activity_name: " << activity_name.value() << std::endl;
  }
  if (activity_label.has_value()) {
    out << " activity_label: " << activity_label.value() << std::endl;
  }
  if (!conditions.empty()) {
    out << " conditions:" << std::endl;
    for (const auto& condition : conditions) {
      out << condition->ToString() << std::endl;
    }
  }
  return out.str();
}

IntentFilters CloneIntentFilters(const IntentFilters& intent_filters) {
  IntentFilters ret;
  for (const auto& intent_filter : intent_filters) {
    ret.push_back(intent_filter->Clone());
  }
  return ret;
}

base::flat_map<std::string, IntentFilters> CloneIntentFiltersMap(
    const base::flat_map<std::string, IntentFilters>& intent_filters_map) {
  base::flat_map<std::string, IntentFilters> ret;
  for (const auto& it : intent_filters_map) {
    ret.insert(std::make_pair(it.first, CloneIntentFilters(it.second)));
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

bool Contains(const IntentFilters& intent_filters,
              const IntentFilterPtr& intent_filter) {
  for (const auto& filter : intent_filters) {
    if (*filter == *intent_filter) {
      return true;
    }
  }
  return false;
}

}  // namespace apps
