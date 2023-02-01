// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_converter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr int kVersionInitial = 0;
constexpr int kVersionSupportsSharing = 1;

base::Value::Dict ConvertConditionValueToDict(
    const apps::ConditionValuePtr& condition_value) {
  base::Value::Dict condition_value_dict;
  condition_value_dict.Set(apps::kValueKey, condition_value->value);
  condition_value_dict.Set(apps::kMatchTypeKey,
                           static_cast<int>(condition_value->match_type));
  return condition_value_dict;
}

base::Value::Dict ConvertConditionToDict(const apps::ConditionPtr& condition) {
  base::Value::Dict condition_dict;
  condition_dict.Set(apps::kConditionTypeKey,
                     static_cast<int>(condition->condition_type));
  base::Value::List condition_values_list;
  for (auto& condition_value : condition->condition_values) {
    condition_values_list.Append(ConvertConditionValueToDict(condition_value));
  }
  condition_dict.Set(apps::kConditionValuesKey,
                     std::move(condition_values_list));
  return condition_dict;
}

base::Value::List ConvertIntentFilterToList(
    const apps::IntentFilterPtr& intent_filter) {
  base::Value::List intent_filter_list;
  for (auto& condition : intent_filter->conditions) {
    intent_filter_list.Append(ConvertConditionToDict(condition));
  }
  return intent_filter_list;
}

apps::ConditionValuePtr ParseDictToConditionValue(
    const base::Value::Dict& dict) {
  const std::string* value_string = dict.FindString(apps::kValueKey);
  if (!value_string) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \""
             << apps::kValueKey << "\" key with string value.";
    return nullptr;
  }
  const absl::optional<int> match_type = dict.FindInt(apps::kMatchTypeKey);
  if (!match_type) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \""
             << apps::kMatchTypeKey << "\" key with int value.";
    return nullptr;
  }
  // We used to have a kNone=0 defined in the enum which we have merged with
  // kLiteral. Some legacy storage may still have zero stored in seralized form
  // as an integer which we can safely treat as kLiteral=1.
  apps::PatternMatchType pattern_match_type = apps::PatternMatchType::kLiteral;
  if (match_type > 0) {
    pattern_match_type =
        static_cast<apps::PatternMatchType>(match_type.value());
  }
  return std::make_unique<apps::ConditionValue>(*value_string,
                                                pattern_match_type);
}

apps::ConditionPtr ParseDictToCondition(const base::Value::Dict& dict) {
  const absl::optional<int> condition_type =
      dict.FindInt(apps::kConditionTypeKey);
  if (!condition_type) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionTypeKey << "\" key with int value.";
    return nullptr;
  }

  apps::ConditionValues condition_values;
  const base::Value::List* values = dict.FindList(apps::kConditionValuesKey);
  if (!values) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionValuesKey << "\" key with list value.";
    return nullptr;
  }
  for (const base::Value& condition_value : *values) {
    auto parsed_condition_value =
        ParseDictToConditionValue(condition_value.GetDict());
    if (!parsed_condition_value) {
      DVLOG(0) << "Fail to parse condition. Cannot parse condition values";
      return nullptr;
    }
    condition_values.push_back(std::move(parsed_condition_value));
  }

  return std::make_unique<apps::Condition>(
      static_cast<apps::ConditionType>(condition_type.value()),
      std::move(condition_values));
}

apps::IntentFilterPtr ParseValueToIntentFilter(const base::Value* value) {
  if (!value || !value->is_list()) {
    DVLOG(0) << "Fail to parse intent filter. Cannot find the conditions list.";
    return nullptr;
  }
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  for (const base::Value& condition : value->GetList()) {
    auto parsed_condition = ParseDictToCondition(condition.GetDict());
    if (!parsed_condition) {
      DVLOG(0) << "Fail to parse intent filter. Cannot parse conditions.";
      return nullptr;
    }
    intent_filter->conditions.push_back(std::move(parsed_condition));
  }
  return intent_filter;
}

}  // namespace

namespace apps {

const char kConditionTypeKey[] = "condition_type";
const char kConditionValuesKey[] = "condition_values";
const char kValueKey[] = "value";
const char kMatchTypeKey[] = "match_type";
const char kAppIdKey[] = "app_id";
const char kIntentFilterKey[] = "intent_filter";
const char kPreferredAppsKey[] = "preferred_apps";
const char kVersionKey[] = "version";

base::Value ConvertPreferredAppsToValue(const PreferredApps& preferred_apps) {
  base::Value::Dict preferred_apps_dict;
  int version = kVersionSupportsSharing;
  preferred_apps_dict.Set(kVersionKey, version);
  base::Value::List preferred_apps_list;
  for (auto& preferred_app : preferred_apps) {
    base::Value::Dict preferred_app_dict;
    preferred_app_dict.Set(kIntentFilterKey, ConvertIntentFilterToList(
                                                 preferred_app->intent_filter));
    preferred_app_dict.Set(kAppIdKey, preferred_app->app_id);
    preferred_apps_list.Append(std::move(preferred_app_dict));
  }
  preferred_apps_dict.Set(kPreferredAppsKey, std::move(preferred_apps_list));
  return base::Value(std::move(preferred_apps_dict));
}

PreferredApps ParseValueToPreferredApps(
    const base::Value& preferred_apps_value) {
  const base::Value* preferred_apps_list = nullptr;
  if (preferred_apps_value.is_list()) {
    preferred_apps_list = &preferred_apps_value;
  } else if (preferred_apps_value.is_dict()) {
    preferred_apps_list = preferred_apps_value.FindListKey(kPreferredAppsKey);
  }
  if (!preferred_apps_list || !preferred_apps_list->is_list()) {
    DVLOG(0)
        << "Fail to parse preferred apps. Cannot find the preferred app list.";
    return PreferredApps();
  }

  PreferredApps preferred_apps;
  for (const base::Value& entry_val : preferred_apps_list->GetList()) {
    const base::Value::Dict& entry = entry_val.GetDict();
    const std::string* app_id = entry.FindString(kAppIdKey);
    if (!app_id) {
      DVLOG(0) << "Fail to parse condition value. Cannot find \""
               << apps::kAppIdKey << "\" key with string value.";
      return PreferredApps();
    }
    auto parsed_intent_filter =
        ParseValueToIntentFilter(entry.Find(kIntentFilterKey));
    if (!parsed_intent_filter) {
      DVLOG(0) << "Fail to parse condition value. Cannot parse intent filter.";
      return PreferredApps();
    }

    // Do not show other browser apps when the user is already using this
    // browser (matches Android behaviour).
    if (parsed_intent_filter->IsBrowserFilter()) {
      continue;
    }

    auto new_preferred_app = std::make_unique<PreferredApp>(
        std::move(parsed_intent_filter), *app_id);
    preferred_apps.push_back(std::move(new_preferred_app));
  }

  return preferred_apps;
}

void UpgradePreferredApps(PreferredApps& preferred_apps) {
  for (auto& preferred_app : preferred_apps) {
    if (preferred_app->intent_filter->FilterNeedsUpgrade()) {
      apps_util::UpgradeFilter(preferred_app->intent_filter);
    }
  }
}

bool IsUpgradedForSharing(const base::Value& preferred_apps_value) {
  if (preferred_apps_value.is_list()) {
    return false;
  }
  auto version = preferred_apps_value.FindIntKey(kVersionKey);
  return version.value_or(kVersionInitial) >= kVersionSupportsSharing;
}

}  // namespace apps
