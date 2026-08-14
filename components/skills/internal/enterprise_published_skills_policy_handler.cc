// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_published_skills_policy_handler.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace skills {

EnterprisePublishedSkillsPolicyHandler::EnterprisePublishedSkillsPolicyHandler(
    policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kEnterprisePublishedSkills,
          skills::prefs::kEnterprisePublishedSkills,
          schema,
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
          policy::SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          policy::SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

EnterprisePublishedSkillsPolicyHandler::
    ~EnterprisePublishedSkillsPolicyHandler() = default;

bool EnterprisePublishedSkillsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return true;
  }

  if (!policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
          policies, errors)) {
    return false;
  }

  const auto& skills_list = value->GetList();
  ValidateAndFilterSkillsList(skills_list, errors);

  return true;
}

void EnterprisePublishedSkillsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return;
  }

  const auto& skills_list = value->GetList();
  if (skills_list.empty()) {
    return;
  }

  base::ListValue valid_list =
      ValidateAndFilterSkillsList(skills_list, nullptr);

  if (!valid_list.empty()) {
    prefs->SetValue(skills::prefs::kEnterprisePublishedSkills,
                    base::Value(std::move(valid_list)));
  }
}

base::ListValue
EnterprisePublishedSkillsPolicyHandler::ValidateAndFilterSkillsList(
    const base::ListValue& skills_list,
    policy::PolicyErrorMap* errors) {
  base::flat_set<std::string> valid_urls;
  base::ListValue valid_list_out;

  if (errors && skills_list.size() > kMaxSkillsLimit) {
    RecordEnterprisePublishedSkillsError(
        EnterprisePublishedSkillsError::kExceedsLimit);
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     {base::NumberToString(kMaxSkillsLimit)},
                     policy::PolicyErrorPath{},
                     policy::PolicyMap::MessageType::kWarning);
  }

  for (const auto& skill_entry : skills_list) {
    if (valid_list_out.size() >= kMaxSkillsLimit) {
      break;
    }

    if (!skill_entry.is_dict()) {
      if (errors) {
        RecordEnterprisePublishedSkillsError(
            EnterprisePublishedSkillsError::kInvalidType);
      }
      continue;
    }

    const std::string* url = skill_entry.GetDict().FindString("url");
    const std::string* hash = skill_entry.GetDict().FindString("hash");
    if (!url || !hash) {
      if (errors) {
        RecordEnterprisePublishedSkillsError(
            EnterprisePublishedSkillsError::kMissingUrlOrHash);
      }
      continue;
    }

    GURL gurl(*url);
    if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
      if (errors) {
        RecordEnterprisePublishedSkillsError(
            EnterprisePublishedSkillsError::kInvalidUrl);
        errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                         policy::PolicyErrorPath{},
                         policy::PolicyMap::MessageType::kWarning);
      }
      continue;
    }

    if (valid_urls.contains(*url)) {
      if (errors) {
        RecordEnterprisePublishedSkillsError(
            EnterprisePublishedSkillsError::kDuplicateUrl);
      }
      continue;
    }
    valid_urls.insert(*url);

    valid_list_out.Append(skill_entry.Clone());
  }

  return valid_list_out;
}

}  // namespace skills
