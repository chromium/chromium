// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_test_util.h"

#include <utility>
#include <vector>

#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace {

apps::IntentFilterPtr MakeIntentFilter(const std::string& action,
                                       const std::string& mime_type,
                                       const std::string& file_extension,
                                       const std::string& url_pattern,
                                       const std::string& activity_label) {
  DCHECK(!mime_type.empty() || !file_extension.empty() || !url_pattern.empty());
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, action,
                                         apps::PatternMatchType::kLiteral);

  apps::ConditionValues condition_values;
  if (!mime_type.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        mime_type, apps::PatternMatchType::kMimeType));
  }
  if (!file_extension.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        file_extension, apps::PatternMatchType::kFileExtension));
  }
  if (!url_pattern.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        url_pattern, apps::PatternMatchType::kGlob));
  }
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kFile, std::move(condition_values)));

  intent_filter->activity_label = activity_label;
  return intent_filter;
}

}  // namespace

namespace apps_util {

apps::IntentFilterPtr MakeIntentFilterForMimeType(
    const std::string& mime_type) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                         kIntentActionSend,
                                         apps::PatternMatchType::kLiteral);

  std::vector<apps::ConditionValuePtr> condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      mime_type, apps::PatternMatchType::kMimeType));
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kMimeType, std::move(condition_values)));

  return intent_filter;
}

apps::IntentFilterPtr MakeIntentFilterForSend(
    const std::string& mime_type,
    const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionSend, mime_type, "", "", activity_label);
}

apps::IntentFilterPtr MakeIntentFilterForSendMultiple(
    const std::string& mime_type,
    const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionSendMultiple, mime_type, "", "",
                          activity_label);
}

apps::IntentFilterPtr MakeFileFilterForView(const std::string& mime_type,
                                            const std::string& file_extension,
                                            const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionView, mime_type, file_extension, "",
                          activity_label);
}

apps::IntentFilterPtr MakeURLFilterForView(const std::string& url_pattern,
                                           const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionView, "", "", url_pattern,
                          activity_label);
}

apps::IntentFilterPtr MakeSchemeOnlyFilter(const std::string& scheme) {
  apps::ConditionValues condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      scheme, apps::PatternMatchType::kLiteral));
  auto condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(condition_values));

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->conditions.push_back(std::move(condition));

  return intent_filter;
}

apps::IntentFilterPtr MakeSchemeAndHostOnlyFilter(const std::string& scheme,
                                                  const std::string& host) {
  apps::ConditionValues scheme_condition_values;
  scheme_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      scheme, apps::PatternMatchType::kLiteral));
  auto scheme_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(scheme_condition_values));

  apps::ConditionValues host_condition_values;
  host_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      host, apps::PatternMatchType::kLiteral));
  auto host_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kAuthority, std::move(host_condition_values));

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->conditions.push_back(std::move(scheme_condition));
  intent_filter->conditions.push_back(std::move(host_condition));

  return intent_filter;
}

void AddConditionValue(apps::ConditionType condition_type,
                       const std::string& value,
                       apps::PatternMatchType pattern_match_type,
                       apps::IntentFilterPtr& intent_filter) {
  for (auto& condition : intent_filter->conditions) {
    if (condition->condition_type == condition_type) {
      condition->condition_values.push_back(
          std::make_unique<apps::ConditionValue>(value, pattern_match_type));
      return;
    }
  }
  intent_filter->AddSingleValueCondition(condition_type, value,
                                         pattern_match_type);
}

}  // namespace apps_util
