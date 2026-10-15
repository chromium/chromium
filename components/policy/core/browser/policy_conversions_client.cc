// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {
// Return true if machine policy information needs to be hidden.
bool IsMachineInfoHidden(PolicyScope scope, bool show_machine_values) {
  return !show_machine_values && scope == PolicyScope::POLICY_SCOPE_MACHINE;
}

// Returns the name of the source of `policy`. The name is also the key of the
// localized source string in the chrome://policy UI.
const char* GetPolicySourceName(const PolicyMap::Entry& policy) {
  return policy.IsDefaultValue() ? "sourceDefault"
                                 : kPolicySources[policy.source].name;
}

// Policies that have at least one source that could not be merged will still
// be treated as conflicted policies while policies that had all of their
// sources merged will not be considered conflicted anymore. Returns
// `std::nullopt` if `policy` is not the result of merging.
std::optional<bool> GetAllSourcesMerged(const PolicyMap::Entry& policy) {
  if (policy.source != POLICY_SOURCE_MERGED) {
    return std::nullopt;
  }
  for (const auto& conflict : policy.conflicts) {
    if (!PolicyMerger::EntriesCanBeMerged(
            conflict.entry(), policy,
            /*is_user_cloud_merging_enabled=*/false)) {
      return false;
    }
  }
  return true;
}

mojom::PolicyLevel ToMojoPolicyLevel(PolicyLevel level) {
  switch (level) {
    case POLICY_LEVEL_MANDATORY:
      return mojom::PolicyLevel::kMandatory;
    case POLICY_LEVEL_RECOMMENDED:
      return mojom::PolicyLevel::kRecommended;
    default:
      NOTREACHED();
  }
}

mojom::PolicyScope ToMojoPolicyScope(PolicyScope scope) {
  switch (scope) {
    case POLICY_SCOPE_USER:
      return mojom::PolicyScope::kUser;
    case POLICY_SCOPE_MACHINE:
      return mojom::PolicyScope::kMachine;
    default:
      NOTREACHED();
  }
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
    const base::Value& value) const {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      value,
      (pretty_print_enabled_ ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0),
      &json_string);
  return json_string;
}

std::optional<PolicyConversionsClient::ChromePoliciesForDisplay>
PolicyConversionsClient::GetChromePoliciesForDisplay() {
  const PolicyService* const policy_service = GetPolicyService();

  const auto* const schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    return std::nullopt;
  }

  const scoped_refptr<SchemaMap> schema_map = schema_registry->schema_map();
  const PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

  ChromePoliciesForDisplay policies;
  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  policies.map = policy_service->GetPolicies(policy_namespace).Clone();

  // Get a list of all the errors in the policy values.
  const ConfigurationPolicyHandlerList* const handler_list = GetHandlerList();
  policies.errors = std::make_unique<PolicyErrorMap>();
  handler_list->ApplyPolicySettings(
      policies.map, nullptr, policies.errors.get(),
      &policies.deprecated_policies, &policies.future_policies);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&policies.map);

  policies.known_policy_schemas =
      GetKnownPolicies(schema_map, policy_namespace);
  return policies;
}

base::DictValue PolicyConversionsClient::GetChromePolicies() {
  DCHECK(HasUserPolicies());

  const std::optional<ChromePoliciesForDisplay> policies =
      GetChromePoliciesForDisplay();
  if (!policies) {
    return base::DictValue();
  }
  return GetPolicyValues(
      policies->map, policies->errors.get(), policies->deprecated_policies,
      policies->future_policies, policies->known_policy_schemas);
}

base::flat_map<std::string, mojom::PolicyPtr>
PolicyConversionsClient::GetChromePoliciesMojo() {
  CHECK(HasUserPolicies());

  const std::optional<ChromePoliciesForDisplay> policies =
      GetChromePoliciesForDisplay();
  if (!policies) {
    return {};
  }
  return GetPolicyValuesMojo(
      policies->map, policies->errors.get(), policies->deprecated_policies,
      policies->future_policies, policies->known_policy_schemas);
}

base::DictValue PolicyConversionsClient::GetPrecedencePolicies() {
  DCHECK(HasUserPolicies());

  VLOG_POLICY(3, POLICY_FETCHING) << "Client has user policies; getting "
                                     "precedence-related policies for Chrome";
  const PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& chrome_policies =
      GetPolicyService()->GetPolicies(policy_namespace);

  const auto* const schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot retrieve Chrome precedence policies, no schema registry";
    return base::DictValue();
  }

  base::DictValue values;
  // Iterate through all precedence metapolicies and retrieve their value only
  // if they are set in the PolicyMap.
  for (const char* const policy : metapolicy::kPrecedence) {
    const auto* const entry = chrome_policies.Get(policy);

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

base::flat_map<std::string, mojom::PolicyPtr>
PolicyConversionsClient::GetPrecedencePoliciesMojo() {
  CHECK(HasUserPolicies());

  const PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& chrome_policies =
      GetPolicyService()->GetPolicies(policy_namespace);

  const auto* const schema_registry = GetPolicySchemaRegistry();
  if (!schema_registry) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot retrieve Chrome precedence policies, no schema registry";
    return {};
  }

  const std::optional<PolicyConversions::PolicyToSchemaMap>
      known_policy_schemas =
          GetKnownPolicies(schema_registry->schema_map(), policy_namespace);

  base::flat_map<std::string, mojom::PolicyPtr> values;
  // Iterate through all precedence metapolicies and retrieve their value only
  // if they are set in the PolicyMap.
  for (const char* const policy : metapolicy::kPrecedence) {
    if (const PolicyMap::Entry* const entry = chrome_policies.Get(policy)) {
      values.emplace(policy,
                     GetPolicyMojo(policy, *entry, PoliciesSet(), PoliciesSet(),
                                   nullptr, known_policy_schemas));
    }
  }
  return values;
}

base::ListValue PolicyConversionsClient::GetPrecedenceOrder() {
  DCHECK(HasUserPolicies());

  base::ListValue precedence_order_localized;
  for (const int label_id : GetPrecedenceOrderIds()) {
    precedence_order_localized.Append(l10n_util::GetStringUTF16(label_id));
  }

  return precedence_order_localized;
}

std::vector<std::string> PolicyConversionsClient::GetPrecedenceOrderMojo() {
  DCHECK(HasUserPolicies());

  std::vector<std::string> precedence_order_localized;
  for (const int label_id : GetPrecedenceOrderIds()) {
    precedence_order_localized.push_back(l10n_util::GetStringUTF8(label_id));
  }
  return precedence_order_localized;
}

std::vector<int> PolicyConversionsClient::GetPrecedenceOrderIds() const {
#if !BUILDFLAG(IS_CHROMEOS)
  const PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& chrome_policies =
      GetPolicyService()->GetPolicies(policy_namespace);

  const bool cloud_machine_precedence =
      chrome_policies.GetValue(key::kCloudPolicyOverridesPlatformPolicy,
                               base::Value::Type::BOOLEAN) &&
      chrome_policies
          .GetValue(key::kCloudPolicyOverridesPlatformPolicy,
                    base::Value::Type::BOOLEAN)
          ->GetBool();
  const bool cloud_user_precedence =
      chrome_policies.IsUserAffiliated() &&
      chrome_policies.GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                               base::Value::Type::BOOLEAN) &&
      chrome_policies
          .GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                    base::Value::Type::BOOLEAN)
          ->GetBool();

  if (cloud_user_precedence) {
    if (cloud_machine_precedence) {
      return {IDS_POLICY_PRECEDENCE_CLOUD_USER,
              IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_USER};
    } else {
      return {IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
              IDS_POLICY_PRECEDENCE_CLOUD_USER,
              IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_USER};
    }
  } else {
    if (cloud_machine_precedence) {
      return {IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_USER,
              IDS_POLICY_PRECEDENCE_CLOUD_USER};
    } else {
      return {IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
              IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
              IDS_POLICY_PRECEDENCE_PLATFORM_USER,
              IDS_POLICY_PRECEDENCE_CLOUD_USER};
    }
  }
#else   // !BUILDFLAG(IS_CHROMEOS)
  return {IDS_POLICY_PRECEDENCE_PLATFORM_MACHINE,
          IDS_POLICY_PRECEDENCE_CLOUD_MACHINE,
          IDS_POLICY_PRECEDENCE_PLATFORM_USER,
          IDS_POLICY_PRECEDENCE_CLOUD_USER};
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

base::Value PolicyConversionsClient::CopyAndMask(
    const base::Value& value,
    const std::optional<Schema>& schema,
    PolicyScope scope) const {
  if (IsMachineInfoHidden(scope, show_machine_values_)) {
    return base::Value(kSensitiveValueMask);
  }

  base::Value value_copy = value.Clone();
  if (schema.has_value()) {
    schema->MaskSensitiveValues(&value_copy);
  }
  return value_copy;
}

base::Value PolicyConversionsClient::CopyAndMaybeConvert(
    const base::Value& value,
    const std::optional<Schema>& schema,
    PolicyScope scope) const {
  base::Value value_copy = CopyAndMask(value, schema, scope);
  if (!convert_values_enabled_) {
    return value_copy;
  }
  if (value_copy.is_dict()) {
    return base::Value(ConvertValueToJSON(value_copy));
  }

  if (!value_copy.is_list()) {
    return value_copy;
  }

  base::ListValue result;
  for (const auto& element : value_copy.GetList()) {
    if (element.is_dict()) {
      result.Append(base::Value(ConvertValueToJSON(element)));
    } else {
      result.Append(element.Clone());
    }
  }
  return base::Value(std::move(result));
}

base::DictValue PolicyConversionsClient::GetPolicyValue(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    PolicyErrorMap* errors,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  const std::optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);
  base::DictValue value;
  value.Set("value", CopyAndMaybeConvert(*policy.value_unsafe(),
                                         known_policy_schema, policy.scope));
  if (convert_types_enabled_) {
    value.Set("scope", policy.scope == POLICY_SCOPE_USER ? "user" : "machine");
    value.Set("level", (policy.level == POLICY_LEVEL_RECOMMENDED)
                           ? "recommended"
                           : "mandatory");
    value.Set("source", GetPolicySourceName(policy));
  } else {
    value.Set("scope", policy.scope);
    value.Set("level", policy.level);
    value.Set("source", policy.source);
  }

  if (const std::optional<bool> all_sources_merged =
          GetAllSourcesMerged(policy)) {
    value.Set("allSourcesMerged", *all_sources_merged);
  }

  if (const std::u16string error =
          GetPolicyMessage(policy_name, policy, PolicyMap::MessageType::kError,
                           errors, known_policy_schema);
      !error.empty()) {
    value.Set("error", error);
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << policy_name << " has an error of type: " << error;
  }

  if (const std::u16string warning = GetPolicyMessage(
          policy_name, policy, PolicyMap::MessageType::kWarning, errors,
          known_policy_schema);
      !warning.empty()) {
    value.Set("warning", warning);
  }

  if (const std::u16string info =
          GetPolicyMessage(policy_name, policy, PolicyMap::MessageType::kInfo,
                           errors, known_policy_schema);
      !info.empty()) {
    value.Set("info", info);
  }

  if (policy.ignored()) {
    value.Set("ignored", true);
  }

  if (deprecated_policies.find(policy_name) != deprecated_policies.end()) {
    value.Set("deprecated", true);
  }

  if (future_policies.find(policy_name) != future_policies.end()) {
    value.Set("future", true);
  }

  if (IsRestartRequired(policy_name, policy, known_policy_schema)) {
    value.Set("restartRequired", true);
  }

  if (!policy.conflicts.empty()) {
    base::ListValue override_values;
    base::ListValue supersede_values;

    bool has_override_values = false;
    bool has_supersede_values = false;
    for (const auto& conflict : policy.conflicts) {
      base::DictValue conflicted_policy_value =
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

base::DictValue PolicyConversionsClient::GetPolicyValues(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  DVLOG_POLICY(2, POLICY_PROCESSING) << "Retrieving map of policy values";

  base::DictValue values;
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == POLICY_SCOPE_USER && !user_policies_enabled_) {
      continue;
    }
    if (policy.IsDefaultValue() && drop_default_values_enabled_) {
      continue;
    }
    base::DictValue value =
        GetPolicyValue(policy_name, policy, deprecated_policies,
                       future_policies, errors, known_policy_schemas);
    values.Set(policy_name, std::move(value));
  }
  return values;
}

mojom::PolicyPtr PolicyConversionsClient::GetPolicyMojo(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    PolicyErrorMap* errors,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  const std::optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);

  auto result = mojom::Policy::New();
  result->name = policy_name;
  // Values are sent as-is (only masked); the UI is responsible for formatting.
  result->value =
      CopyAndMask(*policy.value_unsafe(), known_policy_schema, policy.scope);
  result->level = ToMojoPolicyLevel(policy.level);
  result->scope = ToMojoPolicyScope(policy.scope);
  result->source = GetPolicySourceName(policy);
  result->all_sources_merged = GetAllSourcesMerged(policy);

  const std::u16string error =
      GetPolicyMessage(policy_name, policy, PolicyMap::MessageType::kError,
                       errors, known_policy_schema);
  if (!error.empty()) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << policy_name << " has an error of type: " << base::UTF16ToUTF8(error);
  }
  result->error = base::UTF16ToUTF8(error);
  result->warning = base::UTF16ToUTF8(
      GetPolicyMessage(policy_name, policy, PolicyMap::MessageType::kWarning,
                       errors, known_policy_schema));
  result->info = base::UTF16ToUTF8(
      GetPolicyMessage(policy_name, policy, PolicyMap::MessageType::kInfo,
                       errors, known_policy_schema));

  result->ignored = policy.ignored();
  result->deprecated = deprecated_policies.contains(policy_name);
  result->future = future_policies.contains(policy_name);
  result->restart_required =
      IsRestartRequired(policy_name, policy, known_policy_schema);

  for (const auto& conflict : policy.conflicts) {
    const PolicyMap::Entry& entry = conflict.entry();
    auto mojo_conflict = mojom::Conflict::New();
    mojo_conflict->level = ToMojoPolicyLevel(entry.level);
    mojo_conflict->scope = ToMojoPolicyScope(entry.scope);
    mojo_conflict->source = GetPolicySourceName(entry);
    mojo_conflict->value =
        CopyAndMask(*entry.value_unsafe(), known_policy_schema, entry.scope);
    switch (conflict.conflict_type()) {
      case PolicyMap::ConflictType::Supersede:
        result->superseded.push_back(std::move(mojo_conflict));
        break;
      case PolicyMap::ConflictType::Override:
        result->conflicts.push_back(std::move(mojo_conflict));
        break;
      default:
        break;
    }
  }

  return result;
}

base::flat_map<std::string, mojom::PolicyPtr>
PolicyConversionsClient::GetPolicyValuesMojo(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    const PoliciesSet& deprecated_policies,
    const PoliciesSet& future_policies,
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas) const {
  DVLOG_POLICY(2, POLICY_PROCESSING) << "Retrieving map of policy values";

  // Build a vector first to avoid O(n^2) insertion into the `flat_map`.
  std::vector<std::pair<std::string, mojom::PolicyPtr>> values;
  for (const auto& [policy_name, policy] : map) {
    if (policy.scope == POLICY_SCOPE_USER && !user_policies_enabled_) {
      continue;
    }
    if (policy.IsDefaultValue() && drop_default_values_enabled_) {
      continue;
    }
    values.emplace_back(
        policy_name,
        GetPolicyMojo(policy_name, policy, deprecated_policies, future_policies,
                      errors, known_policy_schemas));
  }
  return base::flat_map<std::string, mojom::PolicyPtr>(std::move(values));
}

bool PolicyConversionsClient::IsRestartRequired(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    const std::optional<Schema>& known_policy_schema) const {
  // Check dynamic refresh and policy change to set restartRequired status
  if (!known_policy_schema) {
    return false;
  }
  const policy::PolicyDetails* const policy_details =
      GetChromePolicyDetails(policy_name);
  if (!policy_details || policy_details->supports_dynamic_refresh) {
    return false;
  }
  // Check if value has changed or policy is newly set
  const PolicyService* const policy_service = GetPolicyService();
  if (!policy_service ||
      !policy_service->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME)) {
    return false;
  }
  const base::Value* const policy_value = policy.value_unsafe();
  std::optional<size_t> current_value_hash;
  if (policy_value) {
    current_value_hash = PolicyValueHash(*policy_value);
  }

  const std::optional<size_t> startup_value_hash =
      policy_service->GetInitialChromePolicyValueHash(policy_name);

  // A policy is considered changed if its current hash differs from its
  // hash at startup. This covers cases where the policy was added,
  // removed, or its value was modified.
  return current_value_hash != startup_value_hash;
}

std::optional<Schema> PolicyConversionsClient::GetKnownPolicySchema(
    const std::optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas,
    const std::string& policy_name) const {
  if (!known_policy_schemas.has_value()) {
    return std::nullopt;
  }
  const auto known_policy_iterator = known_policy_schemas->find(policy_name);
  if (known_policy_iterator == known_policy_schemas->end()) {
    return std::nullopt;
  }
  return known_policy_iterator->second;
}

std::optional<PolicyConversions::PolicyToSchemaMap>
PolicyConversionsClient::GetKnownPolicies(
    const scoped_refptr<SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace) const {
  const Schema* const schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid()) {
    return std::nullopt;
  }

  // Build a vector first and construct the PolicyToSchemaMap (which is a
  // `flat_map`) from that. The reason is that insertion into a `flat_map` is
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
base::DictValue PolicyConversionsClient::ConvertUpdaterPolicies(
    PolicyMap updater_policies,
    std::optional<PolicyConversions::PolicyToSchemaMap>
        updater_policy_schemas) {
  return GetPolicyValues(updater_policies, nullptr, PoliciesSet(),
                         PoliciesSet(), updater_policy_schemas);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

std::u16string PolicyConversionsClient::GetPolicyMessage(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    PolicyMap::MessageType message_type,
    PolicyErrorMap* errors,
    std::optional<Schema> known_policy_schema) const {
  if (IsMachineInfoHidden(policy.scope, show_machine_values_)) {
    return u"";
  }
  if (!known_policy_schema.has_value() &&
      message_type == PolicyMap::MessageType::kError) {
    // We don't know what this policy is. This is an important error to
    // show.
    return l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  }

  // The PolicyMap contains errors about retrieving the policy, while the
  // PolicyErrorMap contains validation errors. Concat the errors.
  auto policy_map_errors = policy.GetLocalizedMessages(
      message_type, base::BindRepeating(&l10n_util::GetStringUTF16));
  auto error_map_errors =
      errors ? errors->GetErrorMessages(policy_name, message_type)
             : std::u16string();
  if (policy_map_errors.empty()) {
    return error_map_errors;
  }

  if (error_map_errors.empty()) {
    return policy_map_errors;
  }

  return base::JoinString(
      {policy_map_errors, errors->GetErrorMessages(policy_name, message_type)},
      u"\n");
}

}  // namespace policy
