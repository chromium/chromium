// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions_client.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::Value;

namespace policy {

PolicyConversionsClient::PolicyConversionsClient() = default;
PolicyConversionsClient::~PolicyConversionsClient() = default;

void PolicyConversionsClient::EnableConvertTypes(bool enabled) {
  convert_types_enabled_ = enabled;
}

void PolicyConversionsClient::EnableConvertValues(bool enabled) {
  convert_values_enabled_ = enabled;
}

void PolicyConversionsClient::EnableDeviceLocalAccountPolicies(bool enabled) {
  device_local_account_policies_enabled_ = enabled;
}

void PolicyConversionsClient::EnableDeviceInfo(bool enabled) {
  device_info_enabled_ = enabled;
}

void PolicyConversionsClient::EnablePrettyPrint(bool enabled) {
  pretty_print_enabled_ = enabled;
}

void PolicyConversionsClient::EnableUserPolicies(bool enabled) {
  user_policies_enabled_ = enabled;
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void PolicyConversionsClient::SetUpdaterPolicies(
    std::unique_ptr<PolicyMap> policies) {
  updater_policies_ = std::move(policies);
}
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::string PolicyConversionsClient::ConvertValueToJSON(
    const Value& value) const {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      value,
      (pretty_print_enabled_ ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0),
      &json_string);
  return json_string;
}

base::Value PolicyConversionsClient::GetChromePolicies() {
  DCHECK(HasUserPolicies());

  PolicyService* policy_service = GetPolicyService();
  PolicyMap map;

  auto* schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    LOG(ERROR) << "Cannot dump Chrome policies, no schema registry";
    return Value(Value::Type::DICTIONARY);
  }

  const scoped_refptr<SchemaMap> schema_map = schema_registry->schema_map();
  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  map.CopyFrom(policy_service->GetPolicies(policy_namespace));

  // Get a list of all the errors in the policy values.
  const ConfigurationPolicyHandlerList* handler_list = GetHandlerList();
  PolicyErrorMap errors;
  PoliciesSet deprecated_policies;
  PoliciesSet future_policies;
  handler_list->ApplyPolicySettings(map, nullptr, &errors, &deprecated_policies,
                                    &future_policies);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  return GetPolicyValues(map, &errors, deprecated_policies, future_policies,
                         GetKnownPolicies(schema_map, policy_namespace));
}

Value PolicyConversionsClient::CopyAndMaybeConvert(
    const Value& value,
    const base::Optional<Schema>& schema) const {
  Value value_copy = value.Clone();
  if (schema.has_value())
    schema->MaskSensitiveValues(&value_copy);
  if (!convert_values_enabled_)
    return value_copy;
  if (value_copy.is_dict())
    return Value(ConvertValueToJSON(value_copy));

  if (!value_copy.is_list()) {
    return value_copy;
  }

  Value result(Value::Type::LIST);
  for (const auto& element : value_copy.GetList()) {
    if (element.is_dict()) {
      result.Append(Value(ConvertValueToJSON(element)));
    } else {
      result.Append(element.Clone());
    }
  }
  return result;
}

Value PolicyConversionsClient::GetPolicyValue(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    PolicyErrorMap* errors,
    const base::Optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  base::Optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);
  Value value(Value::Type::DICTIONARY);
  value.SetKey("value",
               CopyAndMaybeConvert(*policy.value(), known_policy_schema));
  if (convert_types_enabled_) {
    value.SetKey(
        "scope",
        Value((policy.scope == POLICY_SCOPE_USER) ? "user" : "machine"));
    value.SetKey("level", Value(Value((policy.level == POLICY_LEVEL_RECOMMENDED)
                                          ? "recommended"
                                          : "mandatory")));
    value.SetKey("source", Value(policy.IsDefaultValue()
                                     ? "sourceDefault"
                                     : kPolicySources[policy.source].name));
  } else {
    value.SetKey("scope", Value(policy.scope));
    value.SetKey("level", Value(policy.level));
    value.SetKey("source", Value(policy.source));
  }

  // Policies that have at least one source that could not be merged will
  // still be treated as conflicted policies while policies that had all of
  // their sources merged will not be considered conflicted anymore. Some
  // policies have only one source but still appear as POLICY_SOURCE_MERGED
  // because all policies that are listed as policies that should be merged are
  // treated as merged regardless the number of sources. Those policies will not
  // be treated as conflicted policies.
  if (policy.source == POLICY_SOURCE_MERGED) {
    bool policy_has_unmerged_source = false;
    for (const auto& conflict : policy.conflicts) {
      if (PolicyMerger::ConflictCanBeMerged(conflict, policy))
        continue;
      policy_has_unmerged_source = true;
      break;
    }
    value.SetKey("allSourcesMerged", Value(policy.conflicts.size() <= 1 ||
                                           !policy_has_unmerged_source));
  }

  base::string16 error;
  if (!known_policy_schema.has_value()) {
    // We don't know what this policy is. This is an important error to
    // show.
    error = l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  } else {
    // The PolicyMap contains errors about retrieving the policy, while the
    // PolicyErrorMap contains validation errors. Concat the errors.
    auto policy_map_errors = policy.GetLocalizedErrors(
        base::BindRepeating(&l10n_util::GetStringUTF16));
    auto error_map_errors =
        errors ? errors->GetErrors(policy_name) : base::string16();
    if (policy_map_errors.empty())
      error = error_map_errors;
    else if (error_map_errors.empty())
      error = policy_map_errors;
    else
      error =
          base::JoinString({policy_map_errors, errors->GetErrors(policy_name)},
                           base::ASCIIToUTF16("\n"));
  }
  if (!error.empty())
    value.SetKey("error", Value(error));

  base::string16 warning = policy.GetLocalizedWarnings(
      base::BindRepeating(&l10n_util::GetStringUTF16));
  if (!warning.empty())
    value.SetKey("warning", Value(warning));

  if (policy.ignored())
    value.SetBoolKey("ignored", true);

  if (deprecated_policies.find(policy_name) != deprecated_policies.end())
    value.SetBoolKey("deprecated", true);

  if (future_policies.find(policy_name) != future_policies.end())
    value.SetBoolKey("future", true);

  if (!policy.conflicts.empty()) {
    Value conflict_values(Value::Type::LIST);
    for (const auto& conflict : policy.conflicts) {
      base::Value conflicted_policy_value =
          GetPolicyValue(policy_name, conflict, deprecated_policies,
                         future_policies, errors, known_policy_schemas);
      conflict_values.Append(std::move(conflicted_policy_value));
    }

    value.SetKey("conflicts", std::move(conflict_values));
  }

  return value;
}

Value PolicyConversionsClient::GetPolicyValues(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    const base::Optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  base::Value values(base::Value::Type::DICTIONARY);
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == POLICY_SCOPE_USER && !user_policies_enabled_)
      continue;
    base::Value value =
        GetPolicyValue(policy_name, policy, deprecated_policies,
                       future_policies, errors, known_policy_schemas);
    values.SetKey(policy_name, std::move(value));
  }
  return values;
}

base::Optional<Schema> PolicyConversionsClient::GetKnownPolicySchema(
    const base::Optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas,
    const std::string& policy_name) const {
  if (!known_policy_schemas.has_value())
    return base::nullopt;
  auto known_policy_iterator = known_policy_schemas->find(policy_name);
  if (known_policy_iterator == known_policy_schemas->end())
    return base::nullopt;
  return known_policy_iterator->second;
}

base::Optional<PolicyConversions::PolicyToSchemaMap>
PolicyConversionsClient::GetKnownPolicies(
    const scoped_refptr<SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace) const {
  const Schema* schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid())
    return base::nullopt;

  // Build a vector first and construct the PolicyToSchemaMap (which is a
  // |flat_map|) from that. The reason is that insertion into a |flat_map| is
  // O(n), which would make the loop O(n^2), but constructing from a
  // pre-populated vector is less expensive.
  std::vector<std::pair<std::string, Schema>> policy_to_schema_entries;
  for (auto it = schema->GetPropertiesIterator(); !it.IsAtEnd(); it.Advance()) {
    policy_to_schema_entries.push_back(std::make_pair(it.key(), it.schema()));
  }
  return PolicyConversions::PolicyToSchemaMap(
      std::move(policy_to_schema_entries));
}

bool PolicyConversionsClient::GetDeviceLocalAccountPoliciesEnabled() const {
  return device_local_account_policies_enabled_;
}

bool PolicyConversionsClient::GetDeviceInfoEnabled() const {
  return device_info_enabled_;
}

bool PolicyConversionsClient::GetUserPoliciesEnabled() const {
  return user_policies_enabled_;
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
Value PolicyConversionsClient::GetUpdaterPolicies() {
  return updater_policies_
             ? GetPolicyValues(*updater_policies_, nullptr, PoliciesSet(),
                               PoliciesSet(), updater_policy_schemas_)
             : base::Value(base::Value::Type::DICTIONARY);
}

bool PolicyConversionsClient::PolicyConversionsClient::HasUpdaterPolicies()
    const {
  return !!updater_policies_;
}

void PolicyConversionsClient::SetUpdaterPolicySchemas(
    PolicyConversions::PolicyToSchemaMap schemas) {
  updater_policy_schemas_ = std::move(schemas);
}
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace policy
