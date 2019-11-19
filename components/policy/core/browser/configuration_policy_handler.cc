// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

namespace {
// UMA histogram name to count how often validation results with strategy
// SCHEMA_ALLOW_UNKNOWN differ from strategy SCHEMA_ALLOW_INVALID.
// See crbug.com/969706
const char* const kSchemaMismatchedValueIgnored =
    "Enterprise.SchemaMismatchedValueIgnored";
}  // namespace

// ConfigurationPolicyHandler implementation -----------------------------------

ConfigurationPolicyHandler::ConfigurationPolicyHandler() {}

ConfigurationPolicyHandler::~ConfigurationPolicyHandler() {}

void ConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {}

void ConfigurationPolicyHandler::ApplyPolicySettingsWithParameters(
    const PolicyMap& policies,
    const PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  ApplyPolicySettings(policies, prefs);
}

// TypeCheckingPolicyHandler implementation ------------------------------------

TypeCheckingPolicyHandler::TypeCheckingPolicyHandler(
    const char* policy_name,
    base::Value::Type value_type)
    : policy_name_(policy_name), value_type_(value_type) {}

TypeCheckingPolicyHandler::~TypeCheckingPolicyHandler() {}

const char* TypeCheckingPolicyHandler::policy_name() const {
  return policy_name_;
}

bool TypeCheckingPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  return CheckAndGetValue(policies, errors, &value);
}

bool TypeCheckingPolicyHandler::CheckAndGetValue(const PolicyMap& policies,
                                                 PolicyErrorMap* errors,
                                                 const base::Value** value) {
  *value = policies.GetValue(policy_name_);
  if (*value && (*value)->type() != value_type_) {
    errors->AddError(policy_name_, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(value_type_));
    return false;
  }
  return true;
}

// StringListPolicyHandler implementation --------------------------------------

ListPolicyHandler::ListPolicyHandler(const char* policy_name,
                                     base::Value::Type list_entry_type)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      list_entry_type_(list_entry_type) {}

ListPolicyHandler::~ListPolicyHandler() {}

bool ListPolicyHandler::CheckPolicySettings(const policy::PolicyMap& policies,
                                            policy::PolicyErrorMap* errors) {
  return CheckAndGetList(policies, errors, nullptr);
}

void ListPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  std::unique_ptr<base::ListValue> list;
  if (CheckAndGetList(policies, nullptr, &list) && list)
    ApplyList(std::move(list), prefs);
}

bool ListPolicyHandler::CheckAndGetList(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors,
    std::unique_ptr<base::ListValue>* filtered_list) {
  if (filtered_list)
    filtered_list->reset();

  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  // Filter the list, rejecting any invalid strings.
  base::span<const base::Value> list = value->GetList();
  if (filtered_list)
    *filtered_list = std::make_unique<base::ListValue>();
  for (size_t list_index = 0; list_index < list.size(); ++list_index) {
    const base::Value& entry = list[list_index];
    if (entry.type() != list_entry_type_) {
      if (errors) {
        errors->AddError(policy_name(), list_index, IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(list_entry_type_));
      }
      continue;
    }

    if (!CheckListEntry(entry)) {
      if (errors) {
        errors->AddError(policy_name(), list_index,
                         IDS_POLICY_VALUE_FORMAT_ERROR);
      }
      continue;
    }

    if (filtered_list)
      (*filtered_list)->Append(entry.CreateDeepCopy());
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

IntRangePolicyHandlerBase::~IntRangePolicyHandlerBase() {}

bool IntRangePolicyHandlerBase::EnsureInRange(const base::Value* input,
                                              int* output,
                                              PolicyErrorMap* errors) {
  if (!input)
    return true;

  int value;
  if (!input->GetAsInteger(&value)) {
    NOTREACHED();
    return false;
  }

  if (value < min_ || value > max_) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(value));
    }

    if (!clamp_)
      return false;

    value = base::ClampToRange(value, min_, max_);
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

StringMappingListPolicyHandler::~StringMappingListPolicyHandler() {}

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
  const base::Value* value = policies.GetValue(policy_name());
  base::ListValue list;
  if (value && Convert(value, &list, nullptr))
    prefs->SetValue(pref_path_, std::move(list));
}

bool StringMappingListPolicyHandler::Convert(const base::Value* input,
                                             base::ListValue* output,
                                             PolicyErrorMap* errors) {
  if (!input)
    return true;

  const base::ListValue* list_value = nullptr;
  if (!input->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  for (auto entry = list_value->begin(); entry != list_value->end(); ++entry) {
    std::string entry_value;
    if (!entry->GetAsString(&entry_value)) {
      if (errors) {
        errors->AddError(policy_name(), entry - list_value->begin(),
                         IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING));
      }
      continue;
    }

    std::unique_ptr<base::Value> mapped_value = Map(entry_value);
    if (mapped_value) {
      if (output)
        output->Append(std::move(mapped_value));
    } else {
      if (errors) {
        errors->AddError(policy_name(), entry - list_value->begin(),
                         IDS_POLICY_OUT_OF_RANGE_ERROR);
      }
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
      return mapping_entry->mapped_value->CreateDeepCopy();
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

IntRangePolicyHandler::~IntRangePolicyHandler() {}

void IntRangePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
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

IntPercentageToDoublePolicyHandler::~IntPercentageToDoublePolicyHandler() {}

void IntPercentageToDoublePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
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

SimplePolicyHandler::~SimplePolicyHandler() {}

void SimplePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                              PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

// SchemaValidatingPolicyHandler implementation --------------------------------

SchemaValidatingPolicyHandler::SchemaValidatingPolicyHandler(
    const char* policy_name,
    Schema schema,
    SchemaOnErrorStrategy strategy)
    : policy_name_(policy_name), schema_(schema), strategy_(strategy) {
  DCHECK(schema_.valid());
}

SchemaValidatingPolicyHandler::~SchemaValidatingPolicyHandler() {}

const char* SchemaValidatingPolicyHandler::policy_name() const {
  return policy_name_;
}

bool SchemaValidatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return true;

  std::string error_path;
  std::string error;
  bool result = schema_.Validate(*value, strategy_, &error_path, &error);

  if (strategy_ == SCHEMA_ALLOW_INVALID) {
    bool allow_unknown_result =
        schema_.Validate(*value, SCHEMA_ALLOW_UNKNOWN, &error_path, &error);
    base::UmaHistogramBoolean(kSchemaMismatchedValueIgnored,
                              result != allow_unknown_result);
  }

  if (errors && !error.empty()) {
    if (error_path.empty())
      error_path = "(ROOT)";
    errors->AddError(policy_name_, error_path, error);
  }

  return result;
}

bool SchemaValidatingPolicyHandler::CheckAndGetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    std::unique_ptr<base::Value>* output) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return true;

  output->reset(value->DeepCopy());
  std::string error_path;
  std::string error;
  bool result =
      schema_.Normalize(output->get(), strategy_, &error_path, &error, nullptr);

  if (strategy_ == SCHEMA_ALLOW_INVALID) {
    bool allow_unknown_result =
        schema_.Validate(*value, SCHEMA_ALLOW_UNKNOWN, &error_path, &error);
    base::UmaHistogramBoolean(kSchemaMismatchedValueIgnored,
                              result != allow_unknown_result);
  }

  if (errors && !error.empty()) {
    if (error_path.empty())
      error_path = "(ROOT)";
    errors->AddError(policy_name_, error_path, error);
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

SimpleSchemaValidatingPolicyHandler::~SimpleSchemaValidatingPolicyHandler() {}

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
  const base::Value* value = policies.GetValue(policy_name());
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
    : policy_name_(policy_name),
      schema_(schema.GetKnownProperty(policy_name_)),
      pref_path_(pref_path),
      allow_recommended_(
          recommended_permission ==
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED),
      allow_mandatory_(mandatory_permission ==
                       SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {
}

SimpleJsonStringSchemaValidatingPolicyHandler::
    ~SimpleJsonStringSchemaValidatingPolicyHandler() {}

bool SimpleJsonStringSchemaValidatingPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* root_value = policies.GetValue(policy_name_);
  if (!root_value)
    return true;

  const PolicyMap::Entry* policy_entry = policies.Get(policy_name_);
  if ((policy_entry->level == policy::POLICY_LEVEL_MANDATORY &&
       !allow_mandatory_) ||
      (policy_entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
       !allow_recommended_)) {
    if (errors)
      errors->AddError(policy_name_, IDS_POLICY_LEVEL_ERROR);
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
      errors->AddError(policy_name_, "(ROOT)", IDS_POLICY_TYPE_ERROR,
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
      errors->AddError(policy_name_, "(ROOT)", IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    }
    return false;
  }

  // If that succeeds, validate all the list items are strings and validate
  // the JSON inside the strings.
  base::span<const base::Value> list = root_value->GetList();
  bool json_error_seen = false;

  for (size_t index = 0; index < list.size(); ++index) {
    const base::Value& entry = list[index];
    if (!entry.is_string()) {
      if (errors) {
        errors->AddError(policy_name_, index, IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING));
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
  std::string parse_error;
  std::unique_ptr<base::Value> parsed_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          json_string, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &parse_error);
  if (errors && !parse_error.empty()) {
    errors->AddError(policy_name_, ErrorPath(index, ""),
                     IDS_POLICY_INVALID_JSON_ERROR, parse_error);
  }
  if (!parsed_value)
    return false;

  std::string schema_error;
  std::string error_path;
  const Schema json_string_schema =
      IsListSchema() ? schema_.GetItems() : schema_;
  // Even though we are validating this schema here, we don't actually change
  // the policy if it fails to validate. This validation is just so we can show
  // the user errors.
  bool validated = json_string_schema.Validate(
      *parsed_value, SCHEMA_ALLOW_UNKNOWN, &error_path, &schema_error);
  if (errors && !schema_error.empty())
    errors->AddError(policy_name_, ErrorPath(index, error_path), schema_error);
  if (!validated)
    return false;

  return true;
}

std::string SimpleJsonStringSchemaValidatingPolicyHandler::ErrorPath(
    int index,
    std::string json_error_path) {
  if (IsListSchema()) {
    return json_error_path.empty()
               ? base::StringPrintf("items[%d]", index)
               : base::StringPrintf("items[%d].%s", index,
                                    json_error_path.c_str());
  }
  return json_error_path.empty() ? "(ROOT)" : json_error_path;
}

void SimpleJsonStringSchemaValidatingPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name_);
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

void SimpleJsonStringSchemaValidatingPolicyHandler::RecordJsonError() {
  const PolicyDetails* details = GetChromePolicyDetails(policy_name_);
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
    std::unique_ptr<SchemaValidatingPolicyHandler> new_policy_handler)
    : legacy_policy_handlers_(std::move(legacy_policy_handlers)),
      new_policy_handler_(std::move(new_policy_handler)) {}

LegacyPoliciesDeprecatingPolicyHandler::
    ~LegacyPoliciesDeprecatingPolicyHandler() {}

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
  NOTREACHED();
}

}  // namespace policy
