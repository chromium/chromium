// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/browser/policy_conversions_client.h"

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_logger.h"
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

namespace {
const char* USER_SCOPE = "user";
const char* DEVICE_SCOPE = "machine";
#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char* ALL_USERS_SCOPE = "allUsers";
#endif

// Return true if machine policy information needs to be hidden.
bool IsMachineInfoHidden(PolicyScope scope, bool show_machine_values) {
  return !show_machine_values && scope == PolicyScope::POLICY_SCOPE_MACHINE;
}

}  // namespace

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

void PolicyConversionsClient::SetDropDefaultValues(bool enabled) {
  drop_default_values_enabled_ = enabled;
}

void PolicyConversionsClient::EnableShowMachineValues(bool enabled) {
  show_machine_values_ = enabled;
}

std::string PolicyConversionsClient::ConvertValueToJSON(
    const Value& value) const {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      value,
      (pretty_print_enabled_ ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0),
      &json_string);
  return json_string;
}

base::Value::Dict PolicyConversionsClient::GetChromePolicies() {
  DCHECK(HasUserPolicies());

  PolicyService* policy_service = GetPolicyService();

  auto* schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    return Value::Dict();
  }

  const scoped_refptr<SchemaMap> schema_map = schema_registry->schema_map();
  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  PolicyMap map = policy_service->GetPolicies(policy_namespace).Clone();

  // Get a list of all the errors in the policy values.
  const ConfigurationPolicyHandlerList* handler_list = GetHandlerList();
  PolicyErrorMap errors;
  PoliciesSet deprecated_policies;
  PoliciesSet future_policies;
  handler_list->ApplyPolicySettings(map, nullptr, &errors, &deprecated_policies,
                                    &future_policies);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  PopulatePerProfileMap();
#endif

  return GetPolicyValues(map, &errors, deprecated_policies, future_policies,
                         GetKnownPolicies(schema_map, policy_namespace));
}

base::Value::Dict PolicyConversionsClient::GetPrecedencePolicies() {
  DCHECK(HasUserPolicies());

  VLOG_POLICY(3, POLICY_FETCHING) << "Client has user policies; getting "
                                     "precedence-related policies for Chrome";
  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& chrome_policies =
      GetPolicyService()->GetPolicies(policy_namespace);

  auto* schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot retrieve Chrome precedence policies, no schema registry";
    return Value::Dict();
  }

  base::Value::Dict values;
  // Iterate through all precedence metapolicies and retrieve their value only
  // if they are set in the PolicyMap.
  for (auto* policy : metapolicy::kPrecedence) {
    auto* entry = chrome_policies.Get(policy);

    if (entry) {
      values.Set(policy,
                 GetPolicyValue(policy, entry->DeepCopy(), PoliciesSet(),
                                PoliciesSet(), nullptr,
                                GetKnownPolicies(schema_registry->schema_map(),
                                                 policy_namespace)));
    }
  }

  return values;
}

base::Value::List PolicyConversionsClient::GetPrecedenceOrder() {
  DCHECK(HasUserPolicies());

#if !BUILDFLAG(IS_CHROMEOS)
  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& chrome_policies =
      GetPolicyService()->GetPolicies(policy_namespace);

  bool cloud_machine_precedence =
      chrome_policies.GetValue(key::kCloudPolicyOverridesPlatformPolicy,
                               base::Value::Type::BOOLEAN) &&
      chrome_policies
          .GetValue(key::kCloudPolicyOverridesPlatformPolicy,
                    base::Value::Type::BOOLEAN)
          ->GetBool();
  bool cloud_user_precedence =
      chrome_policies.IsUserAffiliated() &&
      chrome_policies.GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                               base::Value::Type::BOOLEAN) &&
      chrome_policies
          .GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                    base::Value::Type::BOOLEAN)
          ->GetBool();

  std::vector<int> precedence_order(4);
  if (cloud_user_precedence) {
    if (cloud_machine_precedence) {
      precedence_order = {IDS_POLICY_PRECEDENCE_CLOUD_USER,
                          IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_USER};
    } else {
      precedence_order = {IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
                          IDS_POLICY_PRECEDENCE_CLOUD_USER,
                          IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_USER};
    }
  } else {
    if (cloud_machine_precedence) {
      precedence_order = {IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_USER,
                          IDS_POLICY_PRECEDENCE_CLOUD_USER};
    } else {
      precedence_order = {IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
                          IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
                          IDS_POLICY_PRECEDENCE_PLATFORM_USER,
                          IDS_POLICY_PRECEDENCE_CLOUD_USER};
    }
  }
#else
  std::vector<int> precedence_order{IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
                                    IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
                                    IDS_POLICY_PRECEDENCE_PLATFORM_USER,
                                    IDS_POLICY_PRECEDENCE_CLOUD_USER};
#endif  // !BUILDFLAG(IS_CHROMEOS)

  base::Value::List precedence_order_localized;
  for (int label_id : precedence_order) {
    precedence_order_localized.Append(l10n_util::GetStringUTF16(label_id));
  }

  return precedence_order_localized;
}

Value PolicyConversionsClient::CopyAndMaybeConvert(
    const Value& value,
    const std::optional<Schema>& schema,
    PolicyScope scope) const {
  if (IsMachineInfoHidden(scope, show_machine_values_)) {
    return base::Value(kSensitiveValueMask);
  }

  Value value_copy = value.Clone();
  if (schema.has_value()) {
    schema->MaskSensitiveValues(&value_copy);
  }

  if (!convert_values_enabled_)
    return value_copy;
  if (value_copy.is_dict())
    return Value(ConvertValueToJSON(value_copy));

  if (!value_copy.is_list()) {
    return value_copy;
  }

  Value::List result;
  for (const auto& element : value_copy.GetList()) {
    if (element.is_dict()) {
      result.Append(Value(ConvertValueToJSON(element)));
    } else {
      result.Append(element.Clone());
    }
  }
  return base::Value(std::move(result));
}

Value::Dict PolicyConversionsClient::GetPolicyValue(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    PolicyErrorMap* errors,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  std::optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);
  Value::Dict value;
  value.Set("value", CopyAndMaybeConvert(*policy.value_unsafe(),
                                         known_policy_schema, policy.scope));
  if (convert_types_enabled_) {
    value.Set("scope", GetPolicyScope(policy_name, policy.scope));
    value.Set("level", (policy.level == POLICY_LEVEL_RECOMMENDED)
                           ? "recommended"
                           : "mandatory");
    value.Set("source", policy.IsDefaultValue()
                            ? "sourceDefault"
                            : kPolicySources[policy.source].name);
  } else {
    value.Set("scope", policy.scope);
    value.Set("level", policy.level);
    value.Set("source", policy.source);
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
      if (PolicyMerger::EntriesCanBeMerged(
              conflict.entry(), policy,
              /*is_user_cloud_merging_enabled=*/false))
        continue;
      policy_has_unmerged_source = true;
      break;
    }
    value.Set("allSourcesMerged",
              (policy.conflicts.size() <= 1 || !policy_has_unmerged_source));
  }

  std::u16string error =
      GetPolicyError(policy_name, policy, errors, known_policy_schema);

  if (!error.empty()) {
    value.Set("error", error);
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << policy_name << " has an error of type: " << error;
  }

  if (!IsMachineInfoHidden(policy.scope, show_machine_values_)) {
    std::u16string warning = policy.GetLocalizedMessages(
        PolicyMap::MessageType::kWarning,
        base::BindRepeating(&l10n_util::GetStringUTF16));
    if (!warning.empty()) {
      value.Set("warning", warning);
    }
  }

  std::u16string info = policy.GetLocalizedMessages(
      PolicyMap::MessageType::kInfo,
      base::BindRepeating(&l10n_util::GetStringUTF16));
  if (!info.empty())
    value.Set("info", info);

  if (policy.ignored())
    value.Set("ignored", true);

  if (deprecated_policies.find(policy_name) != deprecated_policies.end())
    value.Set("deprecated", true);

  if (future_policies.find(policy_name) != future_policies.end())
    value.Set("future", true);

  if (!policy.conflicts.empty()) {
    Value::List override_values;
    Value::List supersede_values;

    bool has_override_values = false;
    bool has_supersede_values = false;
    for (const auto& conflict : policy.conflicts) {
      base::Value::Dict conflicted_policy_value =
          GetPolicyValue(policy_name, conflict.entry(), deprecated_policies,
                         future_policies, errors, known_policy_schemas);
      switch (conflict.conflict_type()) {
        case PolicyMap::ConflictType::Supersede:
          supersede_values.Append(std::move(conflicted_policy_value));
          has_supersede_values = true;
          break;
        case PolicyMap::ConflictType::Override:
          override_values.Append(std::move(conflicted_policy_value));
          has_override_values = true;
          break;
        default:
          break;
      }
    }
    if (has_override_values) {
      value.Set("conflicts", std::move(override_values));
    }
    if (has_supersede_values) {
      value.Set("superseded", std::move(supersede_values));
    }
  }

  return value;
}

Value::Dict PolicyConversionsClient::GetPolicyValues(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  DVLOG_POLICY(2, POLICY_PROCESSING) << "Retrieving map of policy values";

  base::Value::Dict values;
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == POLICY_SCOPE_USER && !user_policies_enabled_)
      continue;
    if (policy.IsDefaultValue() && drop_default_values_enabled_)
      continue;
    base::Value::Dict value =
        GetPolicyValue(policy_name, policy, deprecated_policies,
                       future_policies, errors, known_policy_schemas);
    values.Set(policy_name, std::move(value));
  }
  return values;
}

std::optional<Schema> PolicyConversionsClient::GetKnownPolicySchema(
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas,
    const std::string& policy_name) const {
  if (!known_policy_schemas.has_value())
    return std::nullopt;
  auto known_policy_iterator = known_policy_schemas->find(policy_name);
  if (known_policy_iterator == known_policy_schemas->end())
    return std::nullopt;
  return known_policy_iterator->second;
}

std::optional<PolicyConversions::PolicyToSchemaMap>
PolicyConversionsClient::GetKnownPolicies(
    const scoped_refptr<SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace) const {
  const Schema* schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid())
    return std::nullopt;

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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
Value::Dict PolicyConversionsClient::ConvertUpdaterPolicies(
    PolicyMap updater_policies,
    std::optional<PolicyConversions::PolicyToSchemaMap>
        updater_policy_schemas) {
  return GetPolicyValues(updater_policies, nullptr, PoliciesSet(),
                         PoliciesSet(), updater_policy_schemas);
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::string PolicyConversionsClient::GetPolicyScope(
    const std::string& policy_name,
    const PolicyScope& policy_scope) const {
  if (policy_scope != POLICY_SCOPE_USER) {
    return DEVICE_SCOPE;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (per_profile_map_) {
    auto it = per_profile_map_->find(policy_name);
    if (it != per_profile_map_->end()) {
      return it->second ? USER_SCOPE : ALL_USERS_SCOPE;
    }
  }
#endif

  // In Lacros case, this policy is missing from the policy templates.
  // Which means it's a policy for apps/extensions.
  return USER_SCOPE;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void PolicyConversionsClient::PopulatePerProfileMap() {
  if (per_profile_map_) {
    return;
  }

  per_profile_map_ = std::make_unique<std::map<std::string, bool>>();
  for (const BooleanPolicyAccess& access : kBooleanPolicyAccess) {
    per_profile_map_->emplace(std::string(access.policy_key),
                              access.per_profile);
  }
  for (const IntegerPolicyAccess& access : kIntegerPolicyAccess) {
    per_profile_map_->emplace(std::string(access.policy_key),
                              access.per_profile);
  }
  for (const StringPolicyAccess& access : kStringPolicyAccess) {
    per_profile_map_->emplace(std::string(access.policy_key),
                              access.per_profile);
  }
  for (const StringListPolicyAccess& access : kStringListPolicyAccess) {
    per_profile_map_->emplace(std::string(access.policy_key),
                              access.per_profile);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

std::u16string PolicyConversionsClient::GetPolicyError(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    PolicyErrorMap* errors,
    std::optional<Schema> known_policy_schema) const {
  if (IsMachineInfoHidden(policy.scope, show_machine_values_)) {
    return u"";
  }
  if (!known_policy_schema.has_value()) {
    // We don't know what this policy is. This is an important error to
    // show.
    return l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  }

  // The PolicyMap contains errors about retrieving the policy, while the
  // PolicyErrorMap contains validation errors. Concat the errors.
  auto policy_map_errors = policy.GetLocalizedMessages(
      PolicyMap::MessageType::kError,
      base::BindRepeating(&l10n_util::GetStringUTF16));
  auto error_map_errors =
      errors ? errors->GetErrorMessages(policy_name) : std::u16string();
  if (policy_map_errors.empty()) {
    return error_map_errors;
  }

  if (error_map_errors.empty()) {
    return policy_map_errors;
  }

  return base::JoinString(
      {policy_map_errors, errors->GetErrorMessages(policy_name)}, u"\n");
}

}  // namespace policy
