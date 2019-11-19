// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/intent_util.h"

#include "base/optional.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace {

// Get the intent condition value based on the condition type.
base::Optional<std::string> GetIntentConditionValueByType(
    apps::mojom::ConditionType condition_type,
    const apps::mojom::IntentPtr& intent) {
  switch (condition_type) {
    case apps::mojom::ConditionType::kScheme:
      return intent->scheme;
    case apps::mojom::ConditionType::kHost:
      return intent->host;
    case apps::mojom::ConditionType::kPattern:
      return intent->path;
  }
}

}  // namespace

namespace apps_util {

apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url) {
  auto intent = apps::mojom::Intent::New();
  intent->scheme = url.scheme();
  intent->host = url.host();
  intent->path = url.path();
  return intent;
}

bool ConditionValueMatches(
    const std::string& value,
    const apps::mojom::ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    // Fallthrough as kNone and kLiteral has same matching type.
    case apps::mojom::PatternMatchType::kNone:
    case apps::mojom::PatternMatchType::kLiteral:
      return value == condition_value->value;
    case apps::mojom::PatternMatchType::kPrefix:
      return base::StartsWith(value, condition_value->value,
                              base::CompareCase::INSENSITIVE_ASCII);
    case apps::mojom::PatternMatchType::kGlob:
      return MatchGlob(value, condition_value->value);
  }
}

bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition) {
  base::Optional<std::string> value_to_match =
      GetIntentConditionValueByType(condition->condition_type, intent);
  if (!value_to_match.has_value()) {
    return false;
  }
  for (const auto& condition_value : condition->condition_values) {
    if (ConditionValueMatches(value_to_match.value(), condition_value)) {
      return true;
    }
  }
  return false;
}

bool IntentMatchesFilter(const apps::mojom::IntentPtr& intent,
                         const apps::mojom::IntentFilterPtr& filter) {
  // Intent matches with this intent filter when all of the existing conditions
  // match.
  for (const auto& condition : filter->conditions) {
    if (!IntentMatchesCondition(intent, condition)) {
      return false;
    }
  }
  return true;
}
}  // namespace apps_util
