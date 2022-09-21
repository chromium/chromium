// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_converter.h"

namespace {

constexpr int kVersionInitial = 0;
constexpr int kVersionSupportsSharing = 1;

base::Value ConvertConditionValueToValue(
    const apps::ConditionValuePtr& condition_value) {
  base::Value condition_value_dict(base::Value::Type::DICTIONARY);
  condition_value_dict.SetStringKey(apps::kValueKey, condition_value->value);
  condition_value_dict.SetIntKey(apps::kMatchTypeKey,
                                 static_cast<int>(condition_value->match_type));
  return condition_value_dict;
}

base::Value ConvertConditionToValue(const apps::ConditionPtr& condition) {
  base::Value condition_dict(base::Value::Type::DICTIONARY);
  condition_dict.SetIntKey(apps::kConditionTypeKey,
                           static_cast<int>(condition->condition_type));
  base::Value condition_values_list(base::Value::Type::LIST);
  for (auto& condition_value : condition->condition_values) {
    condition_values_list.Append(ConvertConditionValueToValue(condition_value));
  }
  condition_dict.SetKey(apps::kConditionValuesKey,
                        std::move(condition_values_list));
  return condition_dict;
}

base::Value ConvertIntentFilterToValue(
    const apps::IntentFilterPtr& intent_filter) {
  base::Value intent_filter_value(base::Value::Type::LIST);
  for (auto& condition : intent_filter->conditions) {
    intent_filter_value.Append(ConvertConditionToValue(condition));
  }
  return intent_filter_value;
}

apps::ConditionValuePtr ParseValueToConditionValue(const base::Value& value) {
  auto* value_string = value.FindStringKey(apps::kValueKey);
  if (!value_string) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \""
             << apps::kValueKey << "\" key with string value.";
    return nullptr;
  }
  auto match_type = value.FindIntKey(apps::kMatchTypeKey);
  if (!match_type.has_value()) {
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

apps::ConditionPtr ParseValueToCondition(const base::Value& value) {
  auto condition_type = value.FindIntKey(apps::kConditionTypeKey);
  if (!condition_type.has_value()) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionTypeKey << "\" key with int value.";
    return nullptr;
  }

  apps::ConditionValues condition_values;
  auto* values = value.FindKey(apps::kConditionValuesKey);
  if (!values || !values->is_list()) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionValuesKey << "\" key with list value.";
    return nullptr;
  }
  for (auto& condition_value : values->GetList()) {
    auto parsed_condition_value = ParseValueToConditionValue(condition_value);
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
  for (auto& condition : value->GetList()) {
    auto parsed_condition = ParseValueToCondition(condition);
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
  base::Value preferred_apps_value(base::Value::Type::DICTIONARY);
  int version = kVersionSupportsSharing;
  preferred_apps_value.SetIntKey(kVersionKey, version);
  base::Value preferred_apps_list(base::Value::Type::LIST);
  for (auto& preferred_app : preferred_apps) {
    base::Value preferred_app_dict(base::Value::Type::DICTIONARY);
    preferred_app_dict.SetKey(
        kIntentFilterKey,
        ConvertIntentFilterToValue(preferred_app->intent_filter));
    preferred_app_dict.SetStringKey(kAppIdKey, preferred_app->app_id);
    preferred_apps_list.Append(std::move(preferred_app_dict));
  }
  preferred_apps_value.SetKey(kPreferredAppsKey,
                              std::move(preferred_apps_list));
  return preferred_apps_value;
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
  for (auto& entry : preferred_apps_list->GetList()) {
    auto* app_id = entry.FindStringKey(kAppIdKey);
    if (!app_id) {
      DVLOG(0) << "Fail to parse condition value. Cannot find \""
               << apps::kAppIdKey << "\" key with string value.";
      return PreferredApps();
    }
    auto parsed_intent_filter =
        ParseValueToIntentFilter(entry.FindKey(kIntentFilterKey));
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
