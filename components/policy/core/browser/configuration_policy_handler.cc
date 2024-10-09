// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

const size_t kMaxUrlFiltersPerPolicy = 1000;

// ConfigurationPolicyHandler implementation
// -----------------------------------

ConfigurationPolicyHandler::ConfigurationPolicyHandler() = default;

ConfigurationPolicyHandler::~ConfigurationPolicyHandler() = default;

void ConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {}

void ConfigurationPolicyHandler::ApplyPolicySettingsWithParameters(
    const PolicyMap& policies,
    const PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  ApplyPolicySettings(policies, prefs);
}

// NamedPolicyHandler implementation -------------------------------------------
NamedPolicyHandler::NamedPolicyHandler(const char* policy_name)
    : policy_name_(policy_name) {}

NamedPolicyHandler::~NamedPolicyHandler() = default;

const char* NamedPolicyHandler::policy_name() const {
  return policy_name_;
}

// TypeCheckingPolicyHandler implementation ------------------------------------

TypeCheckingPolicyHandler::TypeCheckingPolicyHandler(
    const char* policy_name,
    base::Value::Type value_type)
    : NamedPolicyHandler(policy_name), value_type_(value_type) {}

TypeCheckingPolicyHandler::~TypeCheckingPolicyHandler() = default;

bool TypeCheckingPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  return CheckAndGetValue(policy_name(), value_type_,
                          policies.Get(policy_name()), errors, &value);
}

bool TypeCheckingPolicyHandler::CheckPolicySettings(
    const char* policy,
    base::Value::Type value_type,
    const PolicyMap::Entry* entry,
    PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  return CheckAndGetValue(policy, value_type, entry, errors, &value);
}

bool TypeCheckingPolicyHandler::CheckAndGetValue(const PolicyMap& policies,
                                                 PolicyErrorMap* errors,
                                                 const base::Value** value) {
  return CheckAndGetValue(policy_name(), value_type_,
                          policies.Get(policy_name()), errors, value);
}

bool TypeCheckingPolicyHandler::CheckAndGetValue(const char* policy,
                                                 base::Value::Type value_type,
                                                 const PolicyMap::Entry* entry,
                                                 PolicyErrorMap* errors,
                                                 const base::Value** value) {
  // It is safe to use `value_unsafe()` as multiple policy types are handled.
  *value = entry ? entry->value_unsafe() : nullptr;
  if (*value && (*value)->type() != value_type) {
    if (errors) {
      errors->AddError(policy, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(value_type));
    }
    return false;
  }
  return true;
}

// StringListPolicyHandler implementation --------------------------------------

ListPolicyHandler::ListPolicyHandler(const char* policy_name,
                                     base::Value::Type list_entry_type)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      list_entry_type_(list_entry_type) {}

ListPolicyHandler::~ListPolicyHandler() = default;

bool ListPolicyHandler::CheckPolicySettings(const policy::PolicyMap& policies,
                                            policy::PolicyErrorMap* errors) {
  std::optional<base::Value::List> empty = std::nullopt;
  return CheckAndGetList(policies, errors, empty);
}

void ListPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  auto list = std::make_optional<base::Value::List>();
  if (CheckAndGetList(policies, nullptr, list) && list) {
    ApplyList(*std::move(list), prefs);
  }
}

bool ListPolicyHandler::CheckAndGetList(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors,
    std::optional<base::Value::List>& filtered_list) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value) {
    filtered_list = std::nullopt;  // nothing to apply
    return true;
  }

  // Filter the list, rejecting any invalid strings.
  const base::Value::List& list = value->GetList();
  for (size_t list_index = 0; list_index < list.size(); ++list_index) {
    const base::Value& entry = list[list_index];
    if (entry.type() != list_entry_type_) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(list_entry_type_),
                         PolicyErrorPath{list_index});
      }
      continue;
    }

    if (!CheckListEntry(entry)) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR,
                         PolicyErrorPath{list_index});
      }
      continue;
    }

    if (filtered_list)
      filtered_list->Append(entry.Clone());
  }

  return true;
}

bool ListPolicyHandler::CheckListEntry(const base::Value& value) {
  return true;
}

// IntRangePolicyHandlerBase implementation ------------------------------------

IntRangePolicyHandlerBase::IntRangePolicyHandlerBase(const char* policy_name,
                                                     int min,
                                                     int max,
                                                     bool clamp)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::INTEGER),
      min_(min),
      max_(max),
      clamp_(clamp) {}

bool IntRangePolicyHandlerBase::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
         EnsureInRange(value, nullptr, errors);
}

IntRangePolicyHandlerBase::~IntRangePolicyHandlerBase() = default;

bool IntRangePolicyHandlerBase::EnsureInRange(const base::Value* input,
                                              int* output,
                                              PolicyErrorMap* errors) {
  if (!input)
    return true;

  DCHECK(input->is_int());
  int value = input->GetInt();

  if (value < min_ || value > max_) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(value));
    }

    if (!clamp_)
      return false;

    value = std::clamp(value, min_, max_);
  }

  if (output)
    *output = value;
  return true;
}

// StringMappingListPolicyHandler implementation -----------------------------

StringMappingListPolicyHandler::MappingEntry::MappingEntry(
    const char* policy_value,
    std::unique_ptr<base::Value> map)
    : enum_value(policy_value), mapped_value(std::move(map)) {}

StringMappingListPolicyHandler::MappingEntry::~MappingEntry() {}

StringMappingListPolicyHandler::StringMappingListPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    const GenerateMapCallback& callback)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path),
      map_getter_(callback) {}

StringMappingListPolicyHandler::~StringMappingListPolicyHandler() = default;

bool StringMappingListPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
         Convert(value, nullptr, errors);
}

void StringMappingListPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  base::Value::List list;
  if (value && Convert(value, &list, nullptr))
    prefs->SetValue(pref_path_, base::Value(std::move(list)));
}

bool StringMappingListPolicyHandler::Convert(const base::Value* input,
                                             base::Value::List* output,
                                             PolicyErrorMap* errors) {
  if (!input)
    return true;

  DCHECK(input->is_list());
  int index = -1;
  for (const auto& entry : input->GetList()) {
    ++index;
    if (!entry.is_string()) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING),
                         PolicyErrorPath{index});
      }
      continue;
    }

    std::unique_ptr<base::Value> mapped_value = Map(entry.GetString());
    if (mapped_value) {
      if (output) {
        output->Append(
            base::Value::FromUniquePtrValue(std::move(mapped_value)));
      }
    } else if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_OUT_OF_RANGE_ERROR,
                       entry.GetString(), PolicyErrorPath{index});
    }
  }

  return true;
}

std::unique_ptr<base::Value> StringMappingListPolicyHandler::Map(
    const std::string& entry_value) {
  // Lazily generate the map of policy strings to mapped values.
  if (map_.empty())
    map_getter_.Run(&map_);

  for (const auto& mapping_entry : map_) {
    if (mapping_entry->enum_value == entry_value) {
      return base::Value::ToUniquePtrValue(
          mapping_entry->mapped_value->Clone());
    }
  }
  return nullptr;
}

// IntRangePolicyHandler implementation ----------------------------------------

IntRangePolicyHandler::IntRangePolicyHandler(const char* policy_name,
                                             const char* pref_path,
                                             int min,
                                             int max,
                                             bool clamp)
    : IntRangePolicyHandlerBase(policy_name, min, max, clamp),
      pref_path_(pref_path) {}

IntRangePolicyHandler::~IntRangePolicyHandler() = default;

void IntRangePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr))
    prefs->SetInteger(pref_path_, value_in_range);
}

// IntPercentageToDoublePolicyHandler implementation ---------------------------

IntPercentageToDoublePolicyHandler::IntPercentageToDoublePolicyHandler(
    const char* policy_name,
    const char* pref_path,
    int min,
    int max,
    bool clamp)
    : IntRangePolicyHandlerBase(policy_name, min, max, clamp),
      pref_path_(pref_path) {}

IntPercentageToDoublePolicyHandler::~IntPercentageToDoublePolicyHandler() =
    default;

void IntPercentageToDoublePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int percentage;
  if (value && EnsureInRange(value, &percentage, nullptr))
    prefs->SetDouble(pref_path_, static_cast<double>(percentage) / 100.);
}

// SimplePolicyHandler implementation ------------------------------------------

SimplePolicyHandler::SimplePolicyHandler(const char* policy_name,
                                         const char* pref_path,
                                         base::Value::Type value_type)
    : TypeCheckingPolicyHandler(policy_name, value_type),
      pref_path_(pref_path) {}

SimplePolicyHandler::~SimplePolicyHandler() = default;

void SimplePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                              PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

// PolicyWithDependencyHandler implementation ------------------------------------------

PolicyWithDependencyHandler::PolicyWithDependencyHandler(
    const char* required_policy_name,
    DependencyRequirement dependency_requirement,
    base::Value expected_dependency_value,
    std::unique_ptr<NamedPolicyHandler> handler)
    : NamedPolicyHandler(handler->policy_name()),
      required_policy_name_(required_policy_name),
      dependency_requirement_(std::move(dependency_requirement)),
      expected_dependency_value_(std::move(expected_dependency_value)),
      handler_(std::move(handler)) {}

PolicyWithDependencyHandler::~PolicyWithDependencyHandler() = default;

bool PolicyWithDependencyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                      PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* required_value =
      policies.GetValueUnsafe(required_policy_name_);
  const base::Value* value = policies.GetValueUnsafe(handler_->policy_name());
  if (!value) {
      return true;
  }
  switch (dependency_requirement_) {
    case DependencyRequirement::kPolicyUnsetOrSetWithvalue:
      if (!required_value) {
        return handler_->CheckPolicySettings(policies, errors);
      }
      [[fallthrough]];
    case DependencyRequirement::kPolicySetWithValue:
      if (expected_dependency_value_ != *required_value) {
        std::string value_str;
        JSONStringValueSerializer serializer(&value_str);
        CHECK(serializer.Serialize(expected_dependency_value_));
        if (errors) {
          errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                           required_policy_name_, value_str);
        }
        return false;
      }
      break;
    case DependencyRequirement::kPolicySet:
      if (!required_value) {
        if (errors) {
          errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                           required_policy_name_);
        }
        return false;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported dependency requirement";
  }

  return handler_->CheckPolicySettings(policies, errors);
}

void PolicyWithDependencyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  handler_->ApplyPolicySettingsWithParameters(policies, parameters, prefs);
}

void PolicyWithDependencyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}
// SchemaValidatingPolicyHandler implementation --------------------------------

SchemaValidatingPolicyHandler::SchemaValidatingPolicyHandler(
    const char* policy_name,
    Schema schema,
    SchemaOnErrorStrategy strategy)
    : NamedPolicyHandler(policy_name), schema_(schema), strategy_(strategy) {
  DCHECK(schema_.valid());
}

SchemaValidatingPolicyHandler::~SchemaValidatingPolicyHandler() = default;

bool SchemaValidatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value)
    return true;

  PolicyErrorPath error_path;
  std::string error;
  std::vector<PolicyMap::PolicyMapType::value_type> errors_with_path;
  bool result = schema_.Validate(*value, strategy_, &error_path, &error);

  if (errors && !error.empty()) {
    // Set error_level based on whether strategy_ tolerates this error without
    // failure.
    errors->AddError(policy_name(), IDS_POLICY_SCHEMA_VALIDATION_ERROR, error,
                     error_path,
                     /*error_level=*/
                     result ? PolicyMap::MessageType::kWarning
                            : PolicyMap::MessageType::kError);
  }

  return result;
}

bool SchemaValidatingPolicyHandler::CheckAndGetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    std::unique_ptr<base::Value>* output) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value)
    return true;

  *output = base::Value::ToUniquePtrValue(value->Clone());
  PolicyErrorPath error_path;
  std::string error;
  bool result =
      schema_.Normalize(output->get(), strategy_, &error_path, &error, nullptr);

  if (errors && !error.empty()) {
    // Set error_level based on whether strategy_ tolerates this error without
    // failure.
    errors->AddError(policy_name(), IDS_POLICY_SCHEMA_VALIDATION_ERROR, error,
                     error_path,
                     /*error_level=*/
                     result ? PolicyMap::MessageType::kWarning
                            : PolicyMap::MessageType::kError);
  }

  return result;
}

// SimpleSchemaValidatingPolicyHandler implementation --------------------------

SimpleSchemaValidatingPolicyHandler::SimpleSchemaValidatingPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    Schema schema,
    SchemaOnErrorStrategy strategy,
    RecommendedPermission recommended_permission,
    MandatoryPermission mandatory_permission)
    : SchemaValidatingPolicyHandler(policy_name,
                                    schema.GetKnownProperty(policy_name),
                                    strategy),
      pref_path_(pref_path),
      allow_recommended_(recommended_permission == RECOMMENDED_ALLOWED),
      allow_mandatory_(mandatory_permission == MANDATORY_ALLOWED) {}

SimpleSchemaValidatingPolicyHandler::~SimpleSchemaValidatingPolicyHandler() =
    default;

bool SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const PolicyMap::Entry* policy_entry = policies.Get(policy_name());
  if (!policy_entry)
    return true;
  if ((policy_entry->level == policy::POLICY_LEVEL_MANDATORY &&
       !allow_mandatory_) ||
      (policy_entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
       !allow_recommended_)) {
    if (errors)
      errors->AddError(policy_name(), IDS_POLICY_LEVEL_ERROR);
    return false;
  }

  return SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors);
}

void SimpleSchemaValidatingPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

// SimpleJsonStringSchemaValidatingPolicyHandler implementation ----------------

SimpleJsonStringSchemaValidatingPolicyHandler::
    SimpleJsonStringSchemaValidatingPolicyHandler(
        const char* policy_name,
        const char* pref_path,
        Schema schema,
        SimpleSchemaValidatingPolicyHandler::RecommendedPermission
            recommended_permission,
        SimpleSchemaValidatingPolicyHandler::MandatoryPermission
            mandatory_permission)
    : NamedPolicyHandler(policy_name),
      schema_(schema.GetKnownProperty(policy_name)),
      pref_path_(pref_path),
      allow_recommended_(
          recommended_permission ==
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED),
      allow_mandatory_(mandatory_permission ==
                       SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {
}

SimpleJsonStringSchemaValidatingPolicyHandler::
    ~SimpleJsonStringSchemaValidatingPolicyHandler() = default;

bool SimpleJsonStringSchemaValidatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* root_value = policies.GetValueUnsafe(policy_name());
  if (!root_value)
    return true;

  const PolicyMap::Entry* policy_entry = policies.Get(policy_name());
  if ((policy_entry->level == policy::POLICY_LEVEL_MANDATORY &&
       !allow_mandatory_) ||
      (policy_entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
       !allow_recommended_)) {
    if (errors)
      errors->AddError(policy_name(), IDS_POLICY_LEVEL_ERROR);
    return false;
  }

  if (IsListSchema())
    return CheckListOfJsonStrings(root_value, errors);

  return CheckSingleJsonString(root_value, errors);
}

bool SimpleJsonStringSchemaValidatingPolicyHandler::CheckSingleJsonString(
    const base::Value* root_value,
    PolicyErrorMap* errors) {
  // First validate the root value is a string.
  if (!root_value->is_string()) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING));
    }
    return false;
  }

  // If that succeeds, validate the JSON inside the string.
  const std::string& json_string = root_value->GetString();
  if (!ValidateJsonString(json_string, errors, 0))
    RecordJsonError();

  // Very lenient - return true as long as the root value is a string.
  return true;
}

bool SimpleJsonStringSchemaValidatingPolicyHandler::CheckListOfJsonStrings(
    const base::Value* root_value,
    PolicyErrorMap* errors) {
  // First validate the root value is a list.
  if (!root_value->is_list()) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    }
    return false;
  }

  // If that succeeds, validate all the list items are strings and validate
  // the JSON inside the strings.
  const base::Value::List& list = root_value->GetList();
  bool json_error_seen = false;

  for (size_t index = 0; index < list.size(); ++index) {
    const base::Value& entry = list[index];
    if (!entry.is_string()) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING),
                         PolicyErrorPath{index});
      }
      continue;
    }

    const std::string& json_string = entry.GetString();
    if (!ValidateJsonString(json_string, errors, index))
      json_error_seen = true;
  }

  if (json_error_seen)
    RecordJsonError();

  // Very lenient - return true as long as the root value is a list.
  return true;
}

bool SimpleJsonStringSchemaValidatingPolicyHandler::ValidateJsonString(
    const std::string& json_string,
    PolicyErrorMap* errors,
    int index) {
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json_string, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    if (errors) {
      PolicyErrorPath error_path = {index};
      errors->AddError(policy_name(), IDS_POLICY_INVALID_JSON_ERROR,
                       value_with_error.error().message, error_path);
    }
    return false;
  }

  std::string schema_error;
  PolicyErrorPath error_path;
  const Schema json_string_schema =
      IsListSchema() ? schema_.GetItems() : schema_;
  // Even though we are validating this schema here, we don't actually change
  // the policy if it fails to validate. This validation is just so we can show
  // the user errors.
  bool validated = json_string_schema.Validate(
      *value_with_error, SCHEMA_ALLOW_UNKNOWN, &error_path, &schema_error);
  if (errors && !schema_error.empty()) {
    error_path.emplace(error_path.begin(), index);
    errors->AddError(policy_name(), IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                     schema_error, error_path);
  }
  if (!validated)
    return false;

  return true;
}

void SimpleJsonStringSchemaValidatingPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

void SimpleJsonStringSchemaValidatingPolicyHandler::RecordJsonError() {
  const PolicyDetails* details = GetChromePolicyDetails(policy_name());
  if (details) {
    base::UmaHistogramSparse("EnterpriseCheck.InvalidJsonPolicies",
                             details->id);
  }
}

bool SimpleJsonStringSchemaValidatingPolicyHandler::IsListSchema() const {
  return schema_.type() == base::Value::Type::LIST;
}

// LegacyPoliciesDeprecatingPolicyHandler implementation -----------------------

LegacyPoliciesDeprecatingPolicyHandler::LegacyPoliciesDeprecatingPolicyHandler(
    std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
        legacy_policy_handlers,
    std::unique_ptr<NamedPolicyHandler> new_policy_handler)
    : legacy_policy_handlers_(std::move(legacy_policy_handlers)),
      new_policy_handler_(std::move(new_policy_handler)) {}

LegacyPoliciesDeprecatingPolicyHandler::
    ~LegacyPoliciesDeprecatingPolicyHandler() = default;

bool LegacyPoliciesDeprecatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (policies.Get(new_policy_handler_->policy_name()))
    return new_policy_handler_->CheckPolicySettings(policies, errors);

  // The new policy is not set, fall back to legacy ones.
  bool valid_policy_found = false;
  for (const auto& handler : legacy_policy_handlers_) {
    if (handler->CheckPolicySettings(policies, errors))
      valid_policy_found = true;
  }
  return valid_policy_found;
}

void LegacyPoliciesDeprecatingPolicyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  if (policies.Get(new_policy_handler_->policy_name())) {
    new_policy_handler_->ApplyPolicySettingsWithParameters(policies, parameters,
                                                           prefs);
    return;
  }

  // The new policy is not set, fall back to legacy ones.
  PolicyErrorMap scoped_errors;
  for (const auto& handler : legacy_policy_handlers_) {
    if (handler->CheckPolicySettings(policies, &scoped_errors))
      handler->ApplyPolicySettingsWithParameters(policies, parameters, prefs);
  }
}

void LegacyPoliciesDeprecatingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}

// SimpleDeprecatingPolicyHandler implementation -----------------------

SimpleDeprecatingPolicyHandler::SimpleDeprecatingPolicyHandler(
    std::unique_ptr<NamedPolicyHandler> legacy_policy_handler,
    std::unique_ptr<NamedPolicyHandler> new_policy_handler)
    : legacy_policy_handler_(std::move(legacy_policy_handler)),
      new_policy_handler_(std::move(new_policy_handler)) {}

SimpleDeprecatingPolicyHandler::~SimpleDeprecatingPolicyHandler() = default;

bool SimpleDeprecatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (policies.Get(new_policy_handler_->policy_name())) {
    if (policies.Get(legacy_policy_handler_->policy_name()) && errors) {
      errors->AddError(legacy_policy_handler_->policy_name(),
                       IDS_POLICY_OVERRIDDEN,
                       new_policy_handler_->policy_name());
    }
    return new_policy_handler_->CheckPolicySettings(policies, errors);
  }

  // The new policy is not set, fall back to legacy ones.
  return legacy_policy_handler_->CheckPolicySettings(policies, errors);
}

void SimpleDeprecatingPolicyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  if (policies.Get(new_policy_handler_->policy_name())) {
    new_policy_handler_->ApplyPolicySettingsWithParameters(policies, parameters,
                                                           prefs);
  } else {
    legacy_policy_handler_->ApplyPolicySettingsWithParameters(
        policies, parameters, prefs);
  }
}

void SimpleDeprecatingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}

// CloudOnlyPolicyHandler implementation ---------------------------------------

namespace {

bool IsCloudOnlyPolicy(const policy::PolicyMap::Entry& policy) {
  return policy.source == policy::POLICY_SOURCE_CLOUD ||
         policy.source == policy::POLICY_SOURCE_CLOUD_FROM_ASH;
}

}  // namespace

CloudOnlyPolicyHandler::CloudOnlyPolicyHandler(const char* policy_name,
                                               Schema schema,
                                               SchemaOnErrorStrategy strategy)
    : SchemaValidatingPolicyHandler(policy_name, schema, strategy) {}

CloudOnlyPolicyHandler::~CloudOnlyPolicyHandler() = default;

// static
bool CloudOnlyPolicyHandler::CheckCloudOnlyPolicySettings(
    const char* policy_name,
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const PolicyMap::Entry* policy = policies.Get(policy_name);
  if (!policy) {
    return true;
  }

  // If the policy source is POLICY_SOURCE_MERGED, it is still cloud-only if all
  // policy values merged into it are cloud-only.
  if (policy->source == policy::POLICY_SOURCE_MERGED) {
    for (const auto& conflict : policy->conflicts) {
      if (!IsCloudOnlyPolicy(conflict.entry())) {
        if (errors) {
          errors->AddError(policy_name, IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
        }
        return false;
      }
    }
  } else if (!IsCloudOnlyPolicy(*policy)) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
    }
    return false;
  }

  return true;
}

bool CloudOnlyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                 PolicyErrorMap* errors) {
  return CheckCloudOnlyPolicySettings(policy_name(), policies, errors)
             ? SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors)
             : false;
}

// CloudUserOnlyPolicyHandler implementation
// ---------------------------------------

CloudUserOnlyPolicyHandler::CloudUserOnlyPolicyHandler(
    std::unique_ptr<NamedPolicyHandler> policy_handler)
    : NamedPolicyHandler(policy_handler->policy_name()),
      policy_handler_(std::move(policy_handler)) {}

CloudUserOnlyPolicyHandler::~CloudUserOnlyPolicyHandler() = default;

// static
bool CloudUserOnlyPolicyHandler::CheckUserOnlyPolicySettings(
    const char* policy_name,
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const PolicyMap::Entry* policy = policies.Get(policy_name);
  if (!policy) {
    return true;
  }

  if (policy->scope != policy::POLICY_SCOPE_USER ||
      !IsCloudOnlyPolicy(*policy)) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_CLOUD_USER_ONLY_ERROR);
    }
    return false;
  }

  return true;
}

bool CloudUserOnlyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  return CheckUserOnlyPolicySettings(policy_name(), policies, errors) &&
         policy_handler_->CheckPolicySettings(policies, errors);
}

void CloudUserOnlyPolicyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  policy_handler_->ApplyPolicySettingsWithParameters(policies, parameters,
                                                     prefs);
}

void CloudUserOnlyPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}

URLPolicyHandler::URLPolicyHandler(const char* policy_name,
                                   const char* pref_path)
    : SimplePolicyHandler(policy_name, pref_path, base::Value::Type::STRING) {}

URLPolicyHandler::~URLPolicyHandler() = default;

bool URLPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                           PolicyErrorMap* errors) {
  if (!SimplePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value) {
    return true;
  }

  const std::string& value_as_string = value->GetString();
  if (GURL(value_as_string).is_valid()) {
    return true;
  }

  errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR);
  return false;
}

}  // namespace policy
