// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/intent_test_util.h"

#include <utility>
#include <vector>

#include "chrome/services/app_service/public/cpp/intent_filter_util.h"

namespace apps_util {

apps::mojom::IntentFilterPtr CreateSchemeOnlyFilter(const std::string& scheme) {
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(apps_util::MakeConditionValue(
      scheme, apps::mojom::PatternMatchType::kNone));
  auto condition = apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                            std::move(condition_values));

  auto intent_filter = apps::mojom::IntentFilter::New();
  intent_filter->conditions.push_back(std::move(condition));

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateSchemeAndHostOnlyFilter(
    const std::string& scheme,
    const std::string& host) {
  std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
  scheme_condition_values.push_back(apps_util::MakeConditionValue(
      scheme, apps::mojom::PatternMatchType::kNone));
  auto scheme_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kScheme, std::move(scheme_condition_values));

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
  host_condition_values.push_back(apps_util::MakeConditionValue(
      host, apps::mojom::PatternMatchType::kNone));
  auto host_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kHost, std::move(host_condition_values));

  auto intent_filter = apps::mojom::IntentFilter::New();
  intent_filter->conditions.push_back(std::move(scheme_condition));
  intent_filter->conditions.push_back(std::move(host_condition));

  return intent_filter;
}

}  // namespace apps_util
