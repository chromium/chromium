// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_constants.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "url/url_constants.h"

namespace {

// Returns true if |value1| has overlapping values with |value2|. This method
// should be called twice, with |value1| and |value2| swapped.
bool ConditionValuesHaveOverlap(const apps::mojom::ConditionValuePtr& value1,
                                const apps::mojom::ConditionValuePtr& value2) {
  if (value1 == value2) {
    return true;
  }

  if (value1->match_type == apps::mojom::PatternMatchType::kSuffix &&
      (value2->match_type == apps::mojom::PatternMatchType::kNone ||
       value2->match_type == apps::mojom::PatternMatchType::kLiteral ||
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

void AddSingleValueCondition(apps::ConditionType condition_type,
                             const std::string& value,
                             apps::PatternMatchType pattern_match_type,
                             apps::IntentFilterPtr& intent_filter) {
  apps::ConditionValues condition_values;
  condition_values.push_back(
      std::make_unique<apps::ConditionValue>(value, pattern_match_type));
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      condition_type, std::move(condition_values)));
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

  AddSingleValueCondition(apps::ConditionType::kAction,
                          apps_util::kIntentActionView,
                          apps::PatternMatchType::kNone, intent_filter);

  AddSingleValueCondition(apps::ConditionType::kScheme, url.scheme(),
                          apps::PatternMatchType::kNone, intent_filter);

  AddSingleValueCondition(apps::ConditionType::kHost, url.host(),
                          apps::PatternMatchType::kNone, intent_filter);

  AddSingleValueCondition(apps::ConditionType::kPattern, url.path(),
                          apps::PatternMatchType::kPrefix, intent_filter);

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateIntentFilterForUrlScope(const GURL& url) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  AddSingleValueCondition(apps::mojom::ConditionType::kAction,
                          apps_util::kIntentActionView,
                          apps::mojom::PatternMatchType::kNone, intent_filter);

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
      case apps::mojom::ConditionType::kFile:
        match_level += IntentFilterMatchLevel::kMimeType;
        break;
    }
  }
  return match_level;
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

// This function returns all of the links that the given intent filter would
// accept, to be used in listing all of the supported links for a given app.
std::set<std::string> AppManagementGetSupportedLinks(
    const apps::mojom::IntentFilterPtr& intent_filter) {
  std::set<std::string> hosts;
  std::set<std::string> paths;
  bool is_http_or_https = false;

  for (auto& condition : intent_filter->conditions) {
    // For scheme conditions we check if it's http or https and set a Boolean
    // if this intent filter is for one of those schemes.
    if (condition->condition_type == apps::mojom::ConditionType::kScheme) {
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
    if (condition->condition_type == apps::mojom::ConditionType::kHost) {
      for (auto& condition_value : condition->condition_values) {
        // Prepend the wildcard to indicate any subdomain in the hosts
        std::string host = condition_value->value;
        if (condition_value->match_type ==
            apps::mojom::PatternMatchType::kSuffix) {
          host = "*" + host;
        }
        hosts.insert(host);
      }
    }

    // For path conditions we add each value to the |paths| set.
    if (condition->condition_type == apps::mojom::ConditionType::kPattern) {
      for (auto& condition_value : condition->condition_values) {
        std::string value = condition_value->value;
        // Glob and literal patterns can be printed exactly, but prefix
        // patterns must have be appended with "*" to indicate that
        // anything with that prefix can be matched.
        if (condition_value->match_type ==
            apps::mojom::PatternMatchType::kPrefix) {
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
      if (path.front() == '/') {
        supported_links.insert(host + path);
      } else {
        supported_links.insert(host + "/" + path);
      }
    }
  }

  return supported_links;
}

bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::mojom::IntentFilterPtr& intent_filter) {
  // Filters associated with kUseBrowserForLink are a special case. These
  // filters do not "belong" to the app and should not be treated as supported
  // links.
  if (app_id == apps::kUseBrowserForLink) {
    return false;
  }

  bool action = false;
  bool scheme = false;
  bool host = false;
  bool pattern = false;
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
      case apps::mojom::ConditionType::kPattern:
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
