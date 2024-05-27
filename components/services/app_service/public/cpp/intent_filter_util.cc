// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/url_constants.h"

namespace apps_util {

namespace {

// Assumes that the inputs are already not equal to each other.
bool ConditionValuesHaveDirectionalOverlap(apps::PatternMatchType match_type1,
                                           std::string_view value1,
                                           apps::PatternMatchType match_type2,
                                           std::string_view value2) {
  if (match_type1 == apps::PatternMatchType::kSuffix &&
      (match_type2 == apps::PatternMatchType::kLiteral ||
       match_type2 == apps::PatternMatchType::kSuffix)) {
    return base::EndsWith(/*str=*/value2,
                          /*search_for=*/value1);
  }

  else if (match_type1 == apps::PatternMatchType::kLiteral) {
    if (match_type2 == apps::PatternMatchType::kPrefix) {
      return base::StartsWith(/*str=*/value1,
                              /*search_for=*/value2);
    } else if (match_type2 == apps::PatternMatchType::kGlob) {
      return MatchGlob(/*value=*/value1,
                       /*pattern=*/value2);
    }
  }

  else if (match_type1 == apps::PatternMatchType::kPrefix &&
           match_type2 == apps::PatternMatchType::kPrefix) {
    return base::StartsWith(/*str=*/value1,
                            /*search_for=*/value2) ||
           base::StartsWith(/*str=*/value2,
                            /*search_for=*/value1);
  }

  return false;
}

bool ConditionValuesHaveOverlap(apps::PatternMatchType match_type1,
                                std::string_view value1,
                                apps::PatternMatchType match_type2,
                                std::string_view value2) {
  if (match_type1 == match_type2 && value1 == value2) {
    return true;
  }

  return ConditionValuesHaveDirectionalOverlap(match_type1, value1, match_type2,
                                               value2) ||
         ConditionValuesHaveDirectionalOverlap(match_type2, value2, match_type1,
                                               value1);
}

bool ConditionValuesHaveOverlap(const apps::ConditionType type,
                                const apps::ConditionValuePtr& value1,
                                const apps::ConditionValuePtr& value2) {
  // kAuthority composes host and optional port in the string and must be
  // handled specially. match_type only applies to the host component.
  if (type == apps::ConditionType::kAuthority) {
    AuthorityView authority1 = AuthorityView::Decode(value1->value);
    AuthorityView authority2 = AuthorityView::Decode(value2->value);
    if (authority1.port.has_value() && authority2.port.has_value() &&
        authority1.port != authority2.port) {
      return false;
    }
    return ConditionValuesHaveOverlap(value1->match_type, authority1.host,
                                      value2->match_type, authority2.host);
  }

  return ConditionValuesHaveOverlap(value1->match_type, value1->value,
                                    value2->match_type, value2->value);
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
      if (ConditionValuesHaveOverlap(condition1->condition_type, value1,
                                     value2)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

const char kValueKey[] = "value";
const char kMatchTypeKey[] = "match_type";
const char kConditionTypeKey[] = "condition_type";
const char kConditionValuesKey[] = "condition_values";
const char kConditionsKey[] = "conditions";
const char kActivityNameKey[] = "activity_name";
const char kActivityLabelKey[] = "activity_label";

apps::IntentFilterPtr MakeIntentFilterForUrlScope(const GURL& url,
                                                  bool omit_port_for_testing) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                         kIntentActionView,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url.scheme(),
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAuthority,
                                         omit_port_for_testing
                                             ? std::string(url.host())
                                             : AuthorityView::Encode(url),
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, url.path(),
                                         apps::PatternMatchType::kPrefix);

  return intent_filter;
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

void UpgradeFilter(apps::IntentFilterPtr& filter) {
  std::vector<apps::ConditionValuePtr> condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kIntentActionView, apps::PatternMatchType::kLiteral));
  auto condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(condition_values));
  filter->conditions.insert(filter->conditions.begin(), std::move(condition));
}

bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::IntentFilterPtr& intent_filter) {
  // Filters associated with kUseBrowserForLink are a special case. These
  // filters do not "belong" to the app and should not be treated as supported
  // links.
  if (app_id == kUseBrowserForLink) {
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
          if (condition_value->value == kIntentActionView) {
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
      case apps::ConditionType::kAuthority:
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

std::set<std::string> GetSupportedLinksForAppManagement(
    const apps::IntentFilterPtr& intent_filter) {
  std::set<std::string> hosts;
  std::set<std::string> paths;
  bool is_http_or_https = false;

  for (auto& condition : intent_filter->conditions) {
    // For scheme conditions we check if it's http or https and set a Boolean
    // if this intent filter is for one of those schemes.
    if (condition->condition_type == apps::ConditionType::kScheme) {
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
    if (condition->condition_type == apps::ConditionType::kAuthority) {
      for (auto& condition_value : condition->condition_values) {
        auto authority = AuthorityView::Decode(condition_value->value);
        // Prepend the wildcard to indicate any subdomain in the hosts
        hosts.insert(base::StrCat(
            {condition_value->match_type == apps::PatternMatchType::kSuffix
                 ? "*"
                 : "",
             authority.host}));
        // TODO(crbug.com/40277276): Display authority.port if it is not the
        // default for the scheme.
      }
    }

    // For path conditions we add each value to the |paths| set.
    if (condition->condition_type == apps::ConditionType::kPath) {
      for (auto& condition_value : condition->condition_values) {
        std::string value = condition_value->value;
        // Glob and literal patterns can be printed exactly, but prefix
        // patterns must have be appended with "*" to indicate that
        // anything with that prefix can be matched.
        if (condition_value->match_type == apps::PatternMatchType::kPrefix) {
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

base::Value::Dict ConvertConditionValueToDict(
    const apps::ConditionValuePtr& condition_value) {
  base::Value::Dict condition_value_dict;
  condition_value_dict.Set(kValueKey, condition_value->value);
  condition_value_dict.Set(kMatchTypeKey,
                           static_cast<int>(condition_value->match_type));
  return condition_value_dict;
}

apps::ConditionValuePtr ConvertDictToConditionValue(
    const base::Value::Dict& dict) {
  const std::string* value_string = dict.FindString(kValueKey);
  if (!value_string) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \"" << kValueKey
             << "\" key with string value.";
    return nullptr;
  }
  const std::optional<int> match_type = dict.FindInt(kMatchTypeKey);
  if (!match_type) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \"" << kMatchTypeKey
             << "\" key with int value.";
    return nullptr;
  }

  // We used to have a kNone=0 defined in the enum which we have merged with
  // kLiteral. Some legacy storage may still have zero stored in serialized form
  // as an integer which we can safely treat as kLiteral=1.
  apps::PatternMatchType pattern_match_type = apps::PatternMatchType::kLiteral;
  if (match_type > 0) {
    pattern_match_type =
        static_cast<apps::PatternMatchType>(match_type.value());
  }
  return std::make_unique<apps::ConditionValue>(*value_string,
                                                pattern_match_type);
}

base::Value::Dict ConvertConditionToDict(const apps::ConditionPtr& condition) {
  base::Value::Dict condition_dict;
  condition_dict.Set(kConditionTypeKey,
                     static_cast<int>(condition->condition_type));
  base::Value::List condition_values_list;
  for (auto& condition_value : condition->condition_values) {
    condition_values_list.Append(ConvertConditionValueToDict(condition_value));
  }
  condition_dict.Set(kConditionValuesKey, std::move(condition_values_list));
  return condition_dict;
}

apps::ConditionPtr ConvertDictToCondition(const base::Value::Dict& dict) {
  const std::optional<int> condition_type = dict.FindInt(kConditionTypeKey);
  if (!condition_type) {
    DVLOG(0) << "Fail to parse condition. Cannot find \"" << kConditionTypeKey
             << "\" key with int value.";
    return nullptr;
  }

  apps::ConditionValues condition_values;
  const base::Value::List* values = dict.FindList(kConditionValuesKey);
  if (!values) {
    DVLOG(0) << "Fail to parse condition. Cannot find \"" << kConditionValuesKey
             << "\" key with list value.";
    return nullptr;
  }
  for (const base::Value& condition_value : *values) {
    auto parsed_condition_value =
        apps_util::ConvertDictToConditionValue(condition_value.GetDict());
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

base::Value::List ConvertIntentFilterConditionsToList(
    const apps::IntentFilterPtr& intent_filter) {
  base::Value::List intent_filter_list;
  for (auto& condition : intent_filter->conditions) {
    intent_filter_list.Append(apps_util::ConvertConditionToDict(condition));
  }
  return intent_filter_list;
}

apps::IntentFilterPtr ConvertListToIntentFilterConditions(
    const base::Value::List* value) {
  if (!value) {
    DVLOG(0) << "Fail to parse intent filter. Cannot find the conditions list.";
    return nullptr;
  }
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  for (const base::Value& condition : *value) {
    auto parsed_condition =
        apps_util::ConvertDictToCondition(condition.GetDict());
    if (!parsed_condition) {
      DVLOG(0) << "Fail to parse intent filter. Cannot parse conditions.";
      return nullptr;
    }
    intent_filter->conditions.push_back(std::move(parsed_condition));
  }
  return intent_filter;
}

base::Value::Dict ConvertIntentFilterToDict(
    const apps::IntentFilterPtr& intent_filter) {
  base::Value::Dict intent_filter_dict;

  if (!intent_filter) {
    return intent_filter_dict;
  }

  if (!intent_filter->conditions.empty()) {
    intent_filter_dict.Set(kConditionsKey,
                           ConvertIntentFilterConditionsToList(intent_filter));
  }

  if (intent_filter->activity_name.has_value()) {
    intent_filter_dict.Set(kActivityNameKey,
                           intent_filter->activity_name.value());
  }

  if (intent_filter->activity_label.has_value()) {
    intent_filter_dict.Set(kActivityLabelKey,
                           intent_filter->activity_label.value());
  }

  return intent_filter_dict;
}

apps::IntentFilterPtr ConvertDictToIntentFilter(const base::Value::Dict* dict) {
  if (!dict) {
    return nullptr;
  }

  apps::IntentFilterPtr intent_filter = std::make_unique<apps::IntentFilter>();

  const base::Value::List* conditions_list = dict->FindList(kConditionsKey);
  if (conditions_list) {
    for (const base::Value& condition : *conditions_list) {
      auto parsed_condition =
          apps_util::ConvertDictToCondition(condition.GetDict());
      if (parsed_condition) {
        intent_filter->conditions.push_back(std::move(parsed_condition));
      }
    }
  }

  const std::string* activity_name = dict->FindString(kActivityNameKey);
  if (activity_name) {
    intent_filter->activity_name = *activity_name;
  }

  const std::string* activity_label = dict->FindString(kActivityLabelKey);
  if (activity_label) {
    intent_filter->activity_label = *activity_label;
  }

  return intent_filter;
}

}  // namespace apps_util
