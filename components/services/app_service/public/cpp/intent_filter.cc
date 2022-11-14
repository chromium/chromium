// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter.h"
#include "url/url_constants.h"

namespace apps {

APP_ENUM_TO_STRING(ConditionType,
                   kScheme,
                   kHost,
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
      case ConditionType::kHost:
        match_level += static_cast<int>(IntentFilterMatchLevel::kHost);
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

std::set<std::string> IntentFilter::GetSupportedLinksForAppManagement() {
  std::set<std::string> hosts;
  std::set<std::string> paths;
  bool is_http_or_https = false;

  for (auto& condition : conditions) {
    // For scheme conditions we check if it's http or https and set a Boolean
    // if this intent filter is for one of those schemes.
    if (condition->condition_type == ConditionType::kScheme) {
      for (auto& condition_value : condition->condition_values) {
        // We only care about http and https schemes.
        if (condition_value->value == url::kHttpScheme ||
            condition_value->value == url::kHttpsScheme) {
          is_http_or_https = true;
          break;
        }
      }

      // There should only be one condition of type |kScheme| so if there
      // aren't any http or https scheme values this indicates that no http or
      // https scheme exists in the intent filter and thus we will have to
      // return an empty list.
      if (!is_http_or_https) {
        break;
      }
    }

    // For host conditions we add each value to the |hosts| set.
    if (condition->condition_type == ConditionType::kHost) {
      for (auto& condition_value : condition->condition_values) {
        // Prepend the wildcard to indicate any subdomain in the hosts
        std::string host = condition_value->value;
        if (condition_value->match_type == PatternMatchType::kSuffix) {
          host = "*" + host;
        }
        hosts.insert(host);
      }
    }

    // For path conditions we add each value to the |paths| set.
    if (condition->condition_type == ConditionType::kPath) {
      for (auto& condition_value : condition->condition_values) {
        std::string value = condition_value->value;
        // Glob and literal patterns can be printed exactly, but prefix
        // patterns must have be appended with "*" to indicate that
        // anything with that prefix can be matched.
        if (condition_value->match_type == PatternMatchType::kPrefix) {
          value.append("*");
        }
        paths.insert(value);
      }
    }
  }

  // We only care about http and https schemes.
  if (!is_http_or_https) {
    return std::set<std::string>();
  }

  std::set<std::string> supported_links;
  for (auto& host : hosts) {
    for (auto& path : paths) {
      if (!path.empty() && path.front() == '/') {
        supported_links.insert(host + path);
      } else {
        supported_links.insert(host + "/" + path);
      }
    }
  }

  return supported_links;
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

ConditionType ConvertMojomConditionTypeToConditionType(
    const apps::mojom::ConditionType& mojom_condition_type) {
  switch (mojom_condition_type) {
    case apps::mojom::ConditionType::kScheme:
      return ConditionType::kScheme;
    case apps::mojom::ConditionType::kHost:
      return ConditionType::kHost;
    case apps::mojom::ConditionType::kPath:
      return ConditionType::kPath;
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
    case ConditionType::kPath:
      return apps::mojom::ConditionType::kPath;
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

IntentFilters ConvertMojomIntentFiltersToIntentFilters(
    const std::vector<apps::mojom::IntentFilterPtr>& mojom_intent_filters) {
  IntentFilters intent_filters;
  intent_filters.reserve(mojom_intent_filters.size());
  for (const auto& mojom_intent_filter : mojom_intent_filters) {
    intent_filters.push_back(
        ConvertMojomIntentFilterToIntentFilter(mojom_intent_filter));
  }
  return intent_filters;
}

std::vector<apps::mojom::IntentFilterPtr>
ConvertIntentFiltersToMojomIntentFilters(const IntentFilters& intent_filters) {
  std::vector<apps::mojom::IntentFilterPtr> mojom_intent_filters;
  mojom_intent_filters.reserve(intent_filters.size());
  for (const auto& intent_filter : intent_filters) {
    mojom_intent_filters.push_back(
        ConvertIntentFilterToMojomIntentFilter(intent_filter));
  }
  return mojom_intent_filters;
}

}  // namespace apps
