// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_converter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"

namespace {

constexpr int kVersionInitial = 0;
constexpr int kVersionSupportsSharing = 1;

}  // namespace

namespace apps {

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
    preferred_app_dict.Set(kIntentFilterKey,
                           apps_util::ConvertIntentFilterConditionsToList(
                               preferred_app->intent_filter));
    preferred_app_dict.Set(kAppIdKey, preferred_app->app_id);
    preferred_apps_list.Append(std::move(preferred_app_dict));
  }
  preferred_apps_dict.Set(kPreferredAppsKey, std::move(preferred_apps_list));
  return base::Value(std::move(preferred_apps_dict));
}

PreferredApps ParseValueToPreferredApps(
    const base::Value& preferred_apps_value) {
  const base::Value::List* preferred_apps_list = nullptr;
  if (preferred_apps_value.is_list()) {
    preferred_apps_list = &preferred_apps_value.GetList();
  } else if (preferred_apps_value.is_dict()) {
    preferred_apps_list =
        preferred_apps_value.GetDict().FindList(kPreferredAppsKey);
  }
  if (!preferred_apps_list) {
    DVLOG(0)
        << "Fail to parse preferred apps. Cannot find the preferred app list.";
    return PreferredApps();
  }

  PreferredApps preferred_apps;
  for (const base::Value& entry_val : *preferred_apps_list) {
    const base::Value::Dict& entry = entry_val.GetDict();
    const std::string* app_id = entry.FindString(kAppIdKey);
    if (!app_id) {
      DVLOG(0) << "Fail to parse condition value. Cannot find \""
               << apps::kAppIdKey << "\" key with string value.";
      return PreferredApps();
    }
    auto parsed_intent_filter = apps_util::ConvertListToIntentFilterConditions(
        entry.FindList(kIntentFilterKey));
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
  auto version = preferred_apps_value.GetDict().FindInt(kVersionKey);
  return version.value_or(kVersionInitial) >= kVersionSupportsSharing;
}

}  // namespace apps
