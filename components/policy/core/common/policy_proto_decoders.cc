// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_proto_decoders.h"

#include <cstring>
#include <limits>
#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace em = enterprise_management;

namespace {

const char kValue[] = "Value";
const char kLevel[] = "Level";
const char kRecommended[] = "Recommended";

// Returns true and sets |level| to a PolicyLevel if the policy has been set
// at that level. Returns false if the policy is not set, or has been set at
// the level of PolicyOptions::UNSET.
template <class AnyPolicyProto>
bool GetPolicyLevel(const AnyPolicyProto& policy_proto, PolicyLevel* level) {
  if (!policy_proto.has_value()) {
    return false;
  }
  if (!policy_proto.has_policy_options()) {
    *level = POLICY_LEVEL_MANDATORY;  // Default level.
    return true;
  }
  switch (policy_proto.policy_options().mode()) {
    case em::PolicyOptions::MANDATORY:
      *level = POLICY_LEVEL_MANDATORY;
      return true;
    case em::PolicyOptions::RECOMMENDED:
      *level = POLICY_LEVEL_RECOMMENDED;
      return true;
    case em::PolicyOptions::UNSET:
      return false;
  }
}

// Convert a BooleanPolicyProto to a bool base::Value.
base::Value DecodeBooleanProto(const em::BooleanPolicyProto& proto) {
  return base::Value(proto.value());
}

// Convert an IntegerPolicyProto to an int base::Value.
base::Value DecodeIntegerProto(const em::IntegerPolicyProto& proto,
                               std::string* error) {
  google::protobuf::int64 value = proto.value();

  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG_POLICY(WARNING, POLICY_PROCESSING)
        << "Integer value " << value << " out of numeric limits";
    *error = "Number out of range - invalid int32";
    return base::Value(base::NumberToString(value));
  }

  return base::Value(static_cast<int>(value));
}

// Convert a StringPolicyProto to a string base::Value.
base::Value DecodeStringProto(const em::StringPolicyProto& proto) {
  return base::Value(proto.value());
}

// Convert a StringListPolicyProto to a List base::Value, where each list value
// is of Type::STRING.
base::Value DecodeStringListProto(const em::StringListPolicyProto& proto) {
  base::Value::List list_value;
  for (const auto& entry : proto.value().entries())
    list_value.Append(entry);
  return base::Value(std::move(list_value));
}

// Convert a StringPolicyProto to a base::Value of any type (for example,
// Type::DICT or Type::LIST) by parsing it as JSON.
base::Value DecodeJsonProto(const em::StringPolicyProto& proto,
                            std::string* error) {
  const std::string& json = proto.value();
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  if (!value_with_error.has_value()) {
    // Can't parse as JSON so return it as a string, and leave it to the handler
    // to validate.
    LOG_POLICY(WARNING, POLICY_PROCESSING) << "Invalid JSON: " << json;
    *error = value_with_error.error().message;
    return base::Value(json);
  }

  // Accept any Value type that parsed as JSON, and leave it to the handler to
  // convert and check the concrete type.
  error->clear();
  return std::move(*value_with_error);
}

bool PerProfileMatches(bool policy_per_profile,
                       PolicyPerProfileFilter per_profile_enum) {
  switch (per_profile_enum) {
    case PolicyPerProfileFilter::kTrue:
      return policy_per_profile;
    case PolicyPerProfileFilter::kFalse:
      return !policy_per_profile;
    case PolicyPerProfileFilter::kAny:
      return true;
  }
}

bool UseExternalDataFetcher(const char* policy_name,
                            StringPolicyType policy_type) {
  if (policy_type == StringPolicyType::EXTERNAL)
    return true;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (strcmp(policy_name, key::kWebAppInstallForceList) == 0)
    return true;
#endif
  return false;
}

}  // namespace

void DecodeProtoFields(
    const em::CloudPolicySettings& policy,
    base::WeakPtr<CloudExternalDataManager> external_data_manager,
    PolicySource source,
    PolicyScope scope,
    PolicyMap* map,
    PolicyPerProfileFilter per_profile) {
  PolicyLevel level;

  for (const BooleanPolicyAccess& access : kBooleanPolicyAccess) {
    if (!PerProfileMatches(access.per_profile, per_profile) ||
        !access.has_proto(policy))
      continue;

    const em::BooleanPolicyProto& proto = access.get_proto(policy);
    if (!GetPolicyLevel(proto, &level))
      continue;

    map->Set(access.policy_key, level, scope, source, DecodeBooleanProto(proto),
             nullptr);
  }

  for (const IntegerPolicyAccess& access : kIntegerPolicyAccess) {
    if (!PerProfileMatches(access.per_profile, per_profile) ||
        !access.has_proto(policy))
      continue;

    const em::IntegerPolicyProto& proto = access.get_proto(policy);
    if (!GetPolicyLevel(proto, &level))
      continue;

    std::string error;
    map->Set(access.policy_key, level, scope, source,
             DecodeIntegerProto(proto, &error), nullptr);
    if (!error.empty())
      map->AddMessage(access.policy_key, PolicyMap::MessageType::kError,
                      IDS_POLICY_PROTO_PARSING_ERROR,
                      {base::UTF8ToUTF16(error)});
  }

  for (const StringPolicyAccess& access : kStringPolicyAccess) {
    if (!PerProfileMatches(access.per_profile, per_profile) ||
        !access.has_proto(policy))
      continue;

    const em::StringPolicyProto& proto = access.get_proto(policy);
    if (!GetPolicyLevel(proto, &level))
      continue;

    std::string error;
    base::Value value = (access.type == StringPolicyType::STRING)
                            ? DecodeStringProto(proto)
                            : DecodeJsonProto(proto, &error);

    // EXTERNAL policies represent a single piece of external data that is
    // retrieved by an ExternalDataFetcher.
    // kWebAppInstallForceList is currently the only policy that is a JSON
    // policy (containing mostly non-external data) which can contain
    // references to multiple pieces of external data as well. For that it
    // needs an ExternalDataFetcher. If we ever create a second such policy,
    // create a new type for it instead of special-casing the policies here.
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher =
        UseExternalDataFetcher(access.policy_key, access.type)
            ? std::make_unique<ExternalDataFetcher>(external_data_manager,
                                                    access.policy_key)
            : nullptr;

    map->Set(access.policy_key, level, scope, source, std::move(value),
             std::move(external_data_fetcher));
    if (!error.empty())
      map->AddMessage(access.policy_key, PolicyMap::MessageType::kError,
                      IDS_POLICY_PROTO_PARSING_ERROR,
                      {base::UTF8ToUTF16(error)});
  }

  for (const StringListPolicyAccess& access : kStringListPolicyAccess) {
    if (!PerProfileMatches(access.per_profile, per_profile) ||
        !access.has_proto(policy))
      continue;

    const em::StringListPolicyProto& proto = access.get_proto(policy);
    if (!GetPolicyLevel(proto, &level))
      continue;

    map->Set(access.policy_key, level, scope, source,
             DecodeStringListProto(proto), nullptr);
  }
}

bool ParseComponentPolicy(base::Value::Dict json_dict,
                          PolicyScope scope,
                          PolicySource source,
                          PolicyMap* policy,
                          std::string* error) {
  // Each top-level key maps a policy name to its description.
  //
  // Each description is an object that contains the policy value under the
  // "Value" key. The optional "Level" key is either "Mandatory" (default) or
  // "Recommended".
  for (auto [policy_name, description] : json_dict) {
    if (!description.is_dict()) {
      *error = "The JSON blob dictionary value is not a dictionary.";
      return false;
    }

    base::Value::Dict& description_dict = description.GetDict();
    std::optional<base::Value> value = description_dict.Extract(kValue);
    if (!value.has_value()) {
      *error = base::StrCat(
          {"The JSON blob dictionary value doesn't contain the required ",
           kValue, " field."});
      return false;
    }

    PolicyLevel level = POLICY_LEVEL_MANDATORY;
    const std::string* level_string = description_dict.FindString(kLevel);
    if (level_string && *level_string == kRecommended)
      level = POLICY_LEVEL_RECOMMENDED;

    policy->Set(policy_name, level, scope, source, std::move(value.value()),
                nullptr);
  }

  return true;
}

}  // namespace policy
