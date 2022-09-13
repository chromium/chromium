// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "url/url_constants.h"

namespace {

// Returns true if |value1| has overlapping values with |value2|. This method
// should be called twice, with |value1| and |value2| swapped.
bool ConditionValuesHaveOverlap(const apps::ConditionValuePtr& value1,
                                const apps::ConditionValuePtr& value2) {
  if (*value1 == *value2) {
    return true;
  }

  if (value1->match_type == apps::PatternMatchType::kSuffix &&
      (value2->match_type == apps::PatternMatchType::kLiteral ||
       value2->match_type == apps::PatternMatchType::kSuffix)) {
    return base::EndsWith(/*str=*/value2->value,
                          /*search_for=*/value1->value);
  }

  else if (value1->match_type == apps::PatternMatchType::kLiteral) {
    if (value2->match_type == apps::PatternMatchType::kPrefix) {
      return base::StartsWith(/*str=*/value1->value,
                              /*search_for=*/value2->value);
    } else if (value2->match_type == apps::PatternMatchType::kGlob) {
      return apps_util::MatchGlob(/*value=*/value1->value,
                                  /*pattern=*/value2->value);
    }
  }

  else if (value1->match_type == apps::PatternMatchType::kPrefix &&
           value2->match_type == apps::PatternMatchType::kPrefix) {
    return base::StartsWith(/*str=*/value1->value,
                            /*search_for=*/value2->value) ||
           base::StartsWith(/*str=*/value2->value,
                            /*search_for=*/value1->value);
  }

  return false;
}

// Returns true if |value1| has overlapping values with |value2|. This method
// should be called twice, with |value1| and |value2| swapped.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool ConditionValuesHaveOverlap(const apps::mojom::ConditionValuePtr& value1,
                                const apps::mojom::ConditionValuePtr& value2) {
  if (value1 == value2) {
    return true;
  }

  if (value1->match_type == apps::mojom::PatternMatchType::kSuffix &&
      (value2->match_type == apps::mojom::PatternMatchType::kLiteral ||
       value2->match_type == apps::mojom::PatternMatchType::kSuffix)) {
    return base::EndsWith(/*str=*/value2->value,
                          /*search_for=*/value1->value);
  }

  else if (value1->match_type == apps::mojom::PatternMatchType::kLiteral) {
    if (value2->match_type == apps::mojom::PatternMatchType::kPrefix) {
      return base::StartsWith(/*str=*/value1->value,
                              /*search_for=*/value2->value);
    } else if (value2->match_type == apps::mojom::PatternMatchType::kGlob) {
      return apps_util::MatchGlob(/*value=*/value1->value,
                                  /*pattern=*/value2->value);
    }
  }

  else if (value1->match_type == apps::mojom::PatternMatchType::kPrefix &&
           value2->match_type == apps::mojom::PatternMatchType::kPrefix) {
    return base::StartsWith(/*str=*/value1->value,
                            /*search_for=*/value2->value) ||
           base::StartsWith(/*str=*/value2->value,
                            /*search_for=*/value1->value);
  }

  return false;
}

bool ConditionsHaveOverlap(const apps::ConditionPtr& condition1,
                           const apps::ConditionPtr& condition2) {
  if (condition1->condition_type != condition2->condition_type) {
    return false;
  }
  // If there are same |condition_value| exist in the both |condition|s, there
  // is an overlap.
  for (auto& value1 : condition1->condition_values) {
    for (auto& value2 : condition2->condition_values) {
      if (ConditionValuesHaveOverlap(value1, value2) ||
          ConditionValuesHaveOverlap(value2, value1)) {
        return true;
      }
    }
  }
  return false;
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool ConditionsHaveOverlap(const apps::mojom::ConditionPtr& condition1,
                           const apps::mojom::ConditionPtr& condition2) {
  if (condition1->condition_type != condition2->condition_type) {
    return false;
  }
  // If there are same |condition_value| exist in the both |condition|s, there
  // is an overlap.
  for (auto& value1 : condition1->condition_values) {
    for (auto& value2 : condition2->condition_values) {
      if (ConditionValuesHaveOverlap(value1, value2) ||
          ConditionValuesHaveOverlap(value2, value1)) {
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

apps::IntentFilterPtr MakeIntentFilterForUrlScope(const GURL& url) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                         apps_util::kIntentActionView,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url.scheme(),
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, url.host(),
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, url.path(),
                                         apps::PatternMatchType::kPrefix);

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateIntentFilterForUrlScope(const GURL& url) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, apps_util::kIntentActionView,
      apps::mojom::PatternMatchType::kLiteral, intent_filter);

  AddSingleValueCondition(apps::mojom::ConditionType::kScheme, url.scheme(),
                          apps::mojom::PatternMatchType::kLiteral,
                          intent_filter);

  AddSingleValueCondition(apps::mojom::ConditionType::kHost, url.host(),
                          apps::mojom::PatternMatchType::kLiteral,
                          intent_filter);

  AddSingleValueCondition(apps::mojom::ConditionType::kPath, url.path(),
                          apps::mojom::PatternMatchType::kPrefix,
                          intent_filter);

  return intent_filter;
}

int GetFilterMatchLevel(const apps::mojom::IntentFilterPtr& intent_filter) {
  int match_level = static_cast<int>(apps::IntentFilterMatchLevel::kNone);
  for (const auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kAction:
        // Action always need to be matched, so there is no need for
        // match level.
        break;
      case apps::mojom::ConditionType::kScheme:
        match_level += static_cast<int>(apps::IntentFilterMatchLevel::kScheme);
        break;
      case apps::mojom::ConditionType::kHost:
        match_level += static_cast<int>(apps::IntentFilterMatchLevel::kHost);
        break;
      case apps::mojom::ConditionType::kPath:
        match_level += static_cast<int>(apps::IntentFilterMatchLevel::kPath);
        break;
      case apps::mojom::ConditionType::kMimeType:
      case apps::mojom::ConditionType::kFile:
        match_level +=
            static_cast<int>(apps::IntentFilterMatchLevel::kMimeType);
        break;
    }
  }
  return match_level;
}

bool FiltersHaveOverlap(const apps::IntentFilterPtr& filter1,
                        const apps::IntentFilterPtr& filter2) {
  if (filter1->conditions.size() != filter2->conditions.size()) {
    return false;
  }
  if (filter1->GetFilterMatchLevel() != filter2->GetFilterMatchLevel()) {
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

bool FiltersHaveOverlap(const apps::mojom::IntentFilterPtr& filter1,
                        const apps::mojom::IntentFilterPtr& filter2) {
  if (filter1->conditions.size() != filter2->conditions.size()) {
    return false;
  }
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

void UpgradeFilter(apps::IntentFilterPtr& filter) {
  std::vector<apps::ConditionValuePtr> condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kLiteral));
  auto condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(condition_values));
  filter->conditions.insert(filter->conditions.begin(), std::move(condition));
}

void UpgradeFilter(apps::mojom::IntentFilterPtr& filter) {
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(apps_util::MakeConditionValue(
      apps_util::kIntentActionView, apps::mojom::PatternMatchType::kLiteral));
  auto condition = apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                            std::move(condition_values));
  filter->conditions.insert(filter->conditions.begin(), std::move(condition));
}

bool IsBrowserFilter(const apps::mojom::IntentFilterPtr& filter) {
  if (GetFilterMatchLevel(filter) !=
      static_cast<int>(apps::IntentFilterMatchLevel::kScheme)) {
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

bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::IntentFilterPtr& intent_filter) {
  // Filters associated with kUseBrowserForLink are a special case. These
  // filters do not "belong" to the app and should not be treated as supported
  // links.
  if (app_id == apps_util::kUseBrowserForLink) {
    return false;
  }

  bool action = false;
  bool scheme = false;
  bool host = false;
  bool pattern = false;
  for (auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::ConditionType::kAction:
        for (auto& condition_value : condition->condition_values) {
          if (condition_value->value == apps_util::kIntentActionView) {
            action = true;
            break;
          }
        }
        break;
      case apps::ConditionType::kScheme:
        for (auto& condition_value : condition->condition_values) {
          if (condition_value->value == "http" ||
              condition_value->value == "https") {
            scheme = true;
            break;
          }
        }
        break;
      case apps::ConditionType::kHost:
        host = true;
        break;
      case apps::ConditionType::kPath:
        pattern = true;
        break;
      default:
        break;
    }

    if (action && scheme && host && pattern) {
      return true;
    }
  }

  return false;
}

bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::mojom::IntentFilterPtr& intent_filter) {
  // Filters associated with kUseBrowserForLink are a special case. These
  // filters do not "belong" to the app and should not be treated as supported
  // links.
  if (app_id == apps_util::kUseBrowserForLink) {
    return false;
  }

  bool action = false;
  bool scheme = false;
  bool host = false;
  bool path = false;
  for (auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kAction:
        for (auto& condition_value : condition->condition_values) {
          if (condition_value->value == apps_util::kIntentActionView) {
            action = true;
            break;
          }
        }
        break;
      case apps::mojom::ConditionType::kScheme:
        for (auto& condition_value : condition->condition_values) {
          if (condition_value->value == "http" ||
              condition_value->value == "https") {
            scheme = true;
            break;
          }
        }
        break;
      case apps::mojom::ConditionType::kHost:
        host = true;
        break;
      case apps::mojom::ConditionType::kPath:
        path = true;
        break;
      default:
        break;
    }

    if (action && scheme && host && path) {
      return true;
    }
  }

  return false;
}

size_t IntentFilterUrlMatchLength(const apps::IntentFilterPtr& intent_filter,
                                  const GURL& url) {
  apps::Intent intent(kIntentActionView, url);
  if (!intent.MatchFilter(intent_filter)) {
    return 0;
  }
  // If the filter matches, all URL components match, so a kPath condition
  // matches and we add up the length of the filter's URL components (scheme,
  // host, path).
  size_t path_length = 0;
  for (const apps::ConditionPtr& condition : intent_filter->conditions) {
    if (condition->condition_type == apps::ConditionType::kPath) {
      for (const apps::ConditionValuePtr& value : condition->condition_values) {
        switch (value->match_type) {
          case apps::PatternMatchType::kLiteral:
          case apps::PatternMatchType::kPrefix:
            path_length = std::max(path_length, value->value.size());
            break;
          default:
            // the rest are ignored.
            break;
        }
      }
    }
  }
  if (path_length == 0) {
    return 0;
  }
  return url.scheme_piece().size() + /*length("://")*/ 3 +
         url.host_piece().size() + path_length;
}

}  // namespace apps_util

namespace apps {

// Example output:
//
// activity_name: FooActivity
// activity_label: Foo
// conditions:
// - kHost: *.wikipedia.org
// - kAction: view
// - kPattern: /a /b*
std::ostream& operator<<(std::ostream& out,
                         const apps::mojom::IntentFilterPtr& intent_filter) {
  if (intent_filter->activity_name.has_value()) {
    out << "activity_name: " << intent_filter->activity_name.value()
        << std::endl;
  }
  if (intent_filter->activity_label.has_value()) {
    out << "activity_label: " << intent_filter->activity_label.value()
        << std::endl;
  }
  out << "conditions:" << std::endl;
  for (const auto& condition : intent_filter->conditions) {
    out << "- " << condition->condition_type << ": ";
    for (const auto& value : condition->condition_values) {
      if (value->match_type == apps::mojom::PatternMatchType::kSuffix) {
        out << "*";
      }
      out << value->value;
      if (value->match_type == apps::mojom::PatternMatchType::kPrefix) {
        out << "*";
      }
      out << " ";
    }
    out << std::endl;
  }

  return out;
}

}  // namespace apps
