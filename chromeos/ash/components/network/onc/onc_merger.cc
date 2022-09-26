// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_merger.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/onc/onc_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::onc {
namespace {

// Returns true if the field is the identifier of a configuration, i.e. the GUID
// of a network or a certificate, the ICCID of a cellular.
bool IsIdentifierField(const chromeos::onc::OncValueSignature& value_signature,
                       const std::string& field_name) {
  if (&value_signature == &chromeos::onc::kNetworkConfigurationSignature)
    return field_name == ::onc::network_config::kGUID;
  if (&value_signature == &chromeos::onc::kCertificateSignature)
    return field_name == ::onc::certificate::kGUID;
  if (&value_signature == &chromeos::onc::kCellularSignature)
    return field_name == ::onc::cellular::kICCID;
  return false;
}

// Identifier fields and other read-only fields (specifically Type) are
// handled specially during merging because they are always identical for the
// various setting sources.
bool IsReadOnlyField(const chromeos::onc::OncValueSignature& value_signature,
                     const std::string& field_name) {
  if (IsIdentifierField(value_signature, field_name))
    return true;
  if (&value_signature == &chromeos::onc::kNetworkConfigurationSignature)
    return field_name == ::onc::network_config::kType;
  return false;
}

// Inserts |true| at every field name in |result| that is recommended in
// |policy|.
void MarkRecommendedFieldnames(const base::Value& policy, base::Value* result) {
  const base::Value* recommended_value =
      policy.FindListKey(::onc::kRecommended);
  if (!recommended_value)
    return;
  for (const auto& value : recommended_value->GetList()) {
    if (value.is_string())
      result->SetBoolKey(value.GetString(), true);
  }
}

// Returns the default value for ONC field specified by |field|.
base::Value GetDefaultValue(const chromeos::onc::OncFieldSignature* field) {
  if (field->default_value_setter)
    return field->default_value_setter();

  DCHECK(field->value_signature);

  // Return the default base::Value for the field type.
  return base::Value(field->value_signature->onc_type);
}

// Returns a dictionary which contains |true| at each path that is editable by
// the user. No other fields are set.
base::Value GetEditableFlags(const base::Value& policy) {
  base::Value result(base::Value::Type::DICTIONARY);
  MarkRecommendedFieldnames(policy, &result);

  // Recurse into nested dictionaries.
  for (auto iter : policy.DictItems()) {
    if (iter.first == ::onc::kRecommended || !iter.second.is_dict())
      continue;
    result.SetKey(iter.first, GetEditableFlags(iter.second));
  }
  return result;
}

// If `dict` doesn't have key `key` yet, set it to `value`.
template <typename ValueType>
void SetIfNotSet(base::Value::Dict& dict,
                 base::StringPiece key,
                 ValueType value) {
  if (dict.Find(key))
    return;
  dict.Set(key, std::move(value));
}

// This is the base class for merging a list of Values in parallel. See
// MergeDictionaries function.
class MergeListOfDictionaries {
 public:
  using ValuePtrs = std::vector<const base::Value*>;

  MergeListOfDictionaries() = default;

  MergeListOfDictionaries(const MergeListOfDictionaries&) = delete;
  MergeListOfDictionaries& operator=(const MergeListOfDictionaries&) = delete;

  virtual ~MergeListOfDictionaries() = default;

  // For each path in any of the dictionaries |dicts|, either MergeListOfValues
  // or MergeNestedDictionaries is called with the list of values that are
  // located at that path in each of the dictionaries. This function returns a
  // new dictionary containing all results of those calls at the respective
  // paths. The resulting dictionary doesn't contain empty dictionaries.
  base::Value MergeDictionaries(const ValuePtrs& dicts) {
    base::Value result(base::Value::Type::DICTIONARY);
    std::set<std::string> visited;
    for (const base::Value* dict_outer : dicts) {
      if (!dict_outer || !dict_outer->is_dict())
        continue;

      for (auto field : dict_outer->DictItems()) {
        const std::string& key = field.first;
        if (key == ::onc::kRecommended || !visited.insert(key).second)
          continue;

        ValuePtrs values_for_key;
        for (const base::Value* dict_inner : dicts) {
          const base::Value* value = nullptr;
          if (dict_inner && dict_inner->is_dict())
            value = dict_inner->FindKey(key);
          values_for_key.push_back(value);
        }
        if (field.second.is_dict()) {
          base::Value merged_dict =
              MergeNestedDictionaries(key, values_for_key);
          if (!merged_dict.DictEmpty())
            result.SetKey(key, std::move(merged_dict));
        } else {
          base::Value merged_value = MergeListOfValues(key, values_for_key);
          if (!merged_value.is_none())
            result.SetKey(key, std::move(merged_value));
        }
      }
    }
    return result;
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. The order of the
  // values is the same as of the given dictionaries |dicts|. If a dictionary
  // doesn't contain a path then it's value is nullptr.
  virtual base::Value MergeListOfValues(const std::string& key,
                                        const ValuePtrs& values) = 0;

  virtual base::Value MergeNestedDictionaries(const std::string& key,
                                              const ValuePtrs& dicts) {
    return MergeDictionaries(dicts);
  }
};

// This is the base class for merging policies and user settings.
class MergeSettingsAndPolicies : public MergeListOfDictionaries {
 public:
  struct ValueParams {
    const base::Value* user_policy;
    const base::Value* device_policy;
    const base::Value* user_setting;
    const base::Value* shared_setting;
    const base::Value* active_setting;
    bool user_editable;
    bool device_editable;
  };

  MergeSettingsAndPolicies() = default;

  MergeSettingsAndPolicies(const MergeSettingsAndPolicies&) = delete;
  MergeSettingsAndPolicies& operator=(const MergeSettingsAndPolicies&) = delete;

  // Merge the provided dictionaries. For each path in any of the dictionaries,
  // MergeValues is called. Its results are collected in a new dictionary which
  // is then returned. The resulting dictionary never contains empty
  // dictionaries.
  base::Value MergeDictionaries(const base::Value* user_policy,
                                const base::Value* device_policy,
                                const base::Value* user_settings,
                                const base::Value* shared_settings,
                                const base::Value* active_settings) {
    hasUserPolicy_ = (user_policy != nullptr);
    hasDevicePolicy_ = (device_policy != nullptr);

    // Note: The call to MergeListOfDictionaries::MergeDictionaries below will
    // ignore Value entries that are not Type::DICTIONARY.
    base::Value user_editable;
    if (user_policy)
      user_editable = GetEditableFlags(*user_policy);

    base::Value device_editable;
    if (device_policy)
      device_editable = GetEditableFlags(*device_policy);

    ValuePtrs dicts(kLastIndex, nullptr);
    dicts[kUserPolicyIndex] = user_policy;
    dicts[kDevicePolicyIndex] = device_policy;
    dicts[kUserSettingsIndex] = user_settings;
    dicts[kSharedSettingsIndex] = shared_settings;
    dicts[kActiveSettingsIndex] = active_settings;
    dicts[kUserEditableIndex] = &user_editable;
    dicts[kDeviceEditableIndex] = &device_editable;
    return MergeListOfDictionaries::MergeDictionaries(dicts);
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. Implementations
  // can use the Has*Policy functions.
  virtual base::Value MergeValues(const std::string& key,
                                  const ValueParams& values) = 0;

  // Whether a user policy was provided.
  bool HasUserPolicy() { return hasUserPolicy_; }

  // Whether a device policy was provided.
  bool HasDevicePolicy() { return hasDevicePolicy_; }

  // MergeListOfDictionaries override.
  base::Value MergeListOfValues(const std::string& key,
                                const ValuePtrs& values) override {
    bool user_editable = !HasUserPolicy();
    if (values[kUserEditableIndex] && values[kUserEditableIndex]->is_bool())
      user_editable = values[kUserEditableIndex]->GetBool();

    bool device_editable = !HasDevicePolicy();
    if (values[kDeviceEditableIndex] && values[kDeviceEditableIndex]->is_bool())
      device_editable = values[kDeviceEditableIndex]->GetBool();

    ValueParams params;
    params.user_policy = values[kUserPolicyIndex];
    params.device_policy = values[kDevicePolicyIndex];
    params.user_setting = values[kUserSettingsIndex];
    params.shared_setting = values[kSharedSettingsIndex];
    params.active_setting = values[kActiveSettingsIndex];
    params.user_editable = user_editable;
    params.device_editable = device_editable;
    return MergeValues(key, params);
  }

 private:
  enum {
    kUserPolicyIndex,
    kDevicePolicyIndex,
    kUserSettingsIndex,
    kSharedSettingsIndex,
    kActiveSettingsIndex,
    kUserEditableIndex,
    kDeviceEditableIndex,
    kLastIndex
  };

  bool hasUserPolicy_, hasDevicePolicy_;
};

// Call MergeDictionaries to merge policies and settings to the effective
// values. This ignores the active settings of Shill. See the description of
// MergeSettingsAndPoliciesToEffective.
class MergeToEffective : public MergeSettingsAndPolicies {
 public:
  MergeToEffective() = default;

  MergeToEffective(const MergeToEffective&) = delete;
  MergeToEffective& operator=(const MergeToEffective&) = delete;

 protected:
  // Merges |values| to the effective value (Mandatory policy overwrites user
  // settings overwrites shared settings overwrites recommended policy). |which|
  // is set to the respective onc::kAugmentation* constant that indicates which
  // source of settings is effective. Note that this function may return nullptr
  // and set |which| to ::onc::kAugmentationUserPolicy, which means that the
  // user policy didn't set a value but also didn't recommend it, thus enforcing
  // the empty value.
  base::Value MergeValues(const std::string& key,
                          const ValueParams& values,
                          std::string* which) {
    const base::Value* result = nullptr;
    which->clear();
    if (!values.user_editable) {
      result = values.user_policy;
      *which = ::onc::kAugmentationUserPolicy;
    } else if (!values.device_editable) {
      result = values.device_policy;
      *which = ::onc::kAugmentationDevicePolicy;
    } else if (values.user_setting) {
      result = values.user_setting;
      *which = ::onc::kAugmentationUserSetting;
    } else if (values.shared_setting) {
      result = values.shared_setting;
      *which = ::onc::kAugmentationSharedSetting;
    } else if (values.user_policy) {
      result = values.user_policy;
      *which = ::onc::kAugmentationUserPolicy;
    } else if (values.device_policy) {
      result = values.device_policy;
      *which = ::onc::kAugmentationDevicePolicy;
    } else {
      // Can be reached if the current field is recommended, but none of the
      // dictionaries contained a value for it.
    }
    return result ? result->Clone() : base::Value();
  }

  // MergeSettingsAndPolicies override.
  base::Value MergeValues(const std::string& key,
                          const ValueParams& values) override {
    std::string which;
    return MergeValues(key, values, &which);
  }
};

namespace {

// Returns true if all not-null values in |values| are equal to |value|.
bool AllPresentValuesEqual(const MergeSettingsAndPolicies::ValueParams& values,
                           const base::Value& value) {
  if (values.user_policy && value != *values.user_policy)
    return false;
  if (values.device_policy && value != *values.device_policy)
    return false;
  if (values.user_setting && value != *values.user_setting)
    return false;
  if (values.shared_setting && value != *values.shared_setting)
    return false;
  if (values.active_setting && value != *values.active_setting)
    return false;
  return true;
}

}  // namespace

// Call MergeDictionaries to merge policies and settings to an augmented
// dictionary which contains a dictionary for each value in the original
// dictionaries. See the description of MergeSettingsAndPoliciesToAugmented.
class MergeToAugmented : public MergeToEffective {
 public:
  MergeToAugmented() = default;

  MergeToAugmented(const MergeToAugmented&) = delete;
  MergeToAugmented& operator=(const MergeToAugmented&) = delete;

  base::Value MergeDictionaries(
      const chromeos::onc::OncValueSignature& signature,
      const base::Value* user_policy,
      const base::Value* device_policy,
      const base::Value* user_settings,
      const base::Value* shared_settings,
      const base::Value* active_settings) {
    signature_ = &signature;
    return MergeToEffective::MergeDictionaries(user_policy, device_policy,
                                               user_settings, shared_settings,
                                               active_settings);
  }

 protected:
  // MergeSettingsAndPolicies override.
  base::Value MergeValues(const std::string& key,
                          const ValueParams& values) override {
    const chromeos::onc::OncFieldSignature* field = nullptr;
    if (signature_)
      field = chromeos::onc::GetFieldSignature(*signature_, key);

    if (!field) {
      // This field is not part of the provided ONCSignature, thus it cannot be
      // controlled by policy. Return the plain active value instead of an
      // augmented dictionary.
      if (values.active_setting)
        return values.active_setting->Clone();
      return base::Value();
    }

    // This field is part of the provided ONCSignature, thus it can be
    // controlled by policy.
    std::string which_effective;
    base::Value effective_value =
        MergeToEffective::MergeValues(key, values, &which_effective);

    if (IsReadOnlyField(*signature_, key)) {
      // Don't augment read-only fields (GUID and Type).
      if (!effective_value.is_none()) {
        // DCHECK that all provided fields are identical.
        DCHECK(AllPresentValuesEqual(values, effective_value))
            << "Values do not match: " << key
            << " Effective: " << effective_value;
        // Return the un-augmented field.
        return effective_value;
      }
      if (values.active_setting) {
        // Unmanaged networks have assigned (active) values.
        return values.active_setting->Clone();
      }
      LOG(ERROR) << "Field has no effective value: " << key;
      return base::Value();
    }

    base::Value augmented_value(base::Value::Type::DICTIONARY);

    if (values.active_setting) {
      augmented_value.SetKey(::onc::kAugmentationActiveSetting,
                             values.active_setting->Clone());
    }

    if (!which_effective.empty()) {
      augmented_value.SetKey(::onc::kAugmentationEffectiveSetting,
                             base::Value(which_effective));
    }

    // Prevent credentials from being forwarded in cleartext to UI.
    // User/shared credentials are not stored separately, so they cannot
    // leak here.
    // User and Shared settings are already replaced with |kFakeCredential|.
    bool is_credential = chromeos::onc::FieldIsCredential(*signature_, key);
    if (is_credential) {
      // Set |kFakeCredential| to notify UI that credential is saved.
      if (values.user_policy) {
        augmented_value.SetKey(::onc::kAugmentationUserPolicy,
                               base::Value(policy_util::kFakeCredential));
      }
      if (values.device_policy) {
        augmented_value.SetKey(::onc::kAugmentationDevicePolicy,
                               base::Value(policy_util::kFakeCredential));
      }
      if (values.active_setting) {
        augmented_value.SetKey(::onc::kAugmentationActiveSetting,
                               base::Value(policy_util::kFakeCredential));
      }
    } else {
      if (values.user_policy) {
        augmented_value.SetKey(::onc::kAugmentationUserPolicy,
                               values.user_policy->Clone());
      }
      if (values.device_policy) {
        augmented_value.SetKey(::onc::kAugmentationDevicePolicy,
                               values.device_policy->Clone());
      }
    }
    if (values.user_setting) {
      augmented_value.SetKey(::onc::kAugmentationUserSetting,
                             values.user_setting->Clone());
    }
    if (values.shared_setting) {
      augmented_value.SetKey(::onc::kAugmentationSharedSetting,
                             values.shared_setting->Clone());
    }

    base::Value::Dict& augmented_value_dict = augmented_value.GetDict();

    if (HasUserPolicy() && values.user_editable) {
      augmented_value.SetKey(::onc::kAugmentationUserEditable,
                             base::Value(true));

      // Ensure that a property that is editable and has a user policy (which
      // indicates that the policy recommends a value) always has the
      // appropriate default user policy value provided.
      // TODO(b/245885527): Come up with a better handling for ONC defaults.
      SetIfNotSet(augmented_value_dict, ::onc::kAugmentationUserPolicy,
                  GetDefaultValue(field));
      SetIfNotSet(augmented_value_dict, ::onc::kAugmentationEffectiveSetting,
                  ::onc::kAugmentationUserPolicy);
    }
    if (HasDevicePolicy() && values.device_editable) {
      augmented_value.SetKey(::onc::kAugmentationDeviceEditable,
                             base::Value(true));

      // Ensure that a property that is editable and has a device policy (which
      // indicates that the policy recommends a value) always has the
      // appropriate default device policy value provided.
      // TODO(b/245885527): Come up with a better handling for ONC defaults.
      SetIfNotSet(augmented_value_dict, ::onc::kAugmentationDevicePolicy,
                  GetDefaultValue(field));
      SetIfNotSet(augmented_value_dict, ::onc::kAugmentationEffectiveSetting,
                  ::onc::kAugmentationDevicePolicy);
    }
    if (!augmented_value.DictEmpty())
      return augmented_value;

    return base::Value();
  }

  // MergeListOfDictionaries override.
  base::Value MergeNestedDictionaries(const std::string& key,
                                      const ValuePtrs& dicts) override {
    if (signature_) {
      const chromeos::onc::OncValueSignature* enclosing_signature = signature_;
      signature_ = nullptr;

      const chromeos::onc::OncFieldSignature* field =
          chromeos::onc::GetFieldSignature(*enclosing_signature, key);
      if (field)
        signature_ = field->value_signature;
      base::Value result =
          MergeToEffective::MergeNestedDictionaries(key, dicts);
      signature_ = enclosing_signature;
      return result;
    }
    return MergeToEffective::MergeNestedDictionaries(key, dicts);
  }

 private:
  const chromeos::onc::OncValueSignature* signature_;
};

}  // namespace

base::Value MergeSettingsAndPoliciesToEffective(
    const base::Value* user_policy,
    const base::Value* device_policy,
    const base::Value* user_settings,
    const base::Value* shared_settings) {
  MergeToEffective merger;
  return merger.MergeDictionaries(user_policy, device_policy, user_settings,
                                  shared_settings, nullptr);
}

base::Value MergeSettingsAndPoliciesToAugmented(
    const chromeos::onc::OncValueSignature& signature,
    const base::Value* user_policy,
    const base::Value* device_policy,
    const base::Value* user_settings,
    const base::Value* shared_settings,
    const base::Value* active_settings) {
  MergeToAugmented merger;
  return merger.MergeDictionaries(signature, user_policy, device_policy,
                                  user_settings, shared_settings,
                                  active_settings);
}

}  // namespace ash::onc
