// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_merger.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/onc/onc_constants.h"

namespace ash::onc {
namespace {

// Returns true if the field is the identifier of a configuration, i.e. the GUID
// of a network or a certificate, the ICCID of a cellular.
bool IsIdentifierField(const chromeos::onc::OncValueSignature& value_signature,
                       const std::string& field_name) {
  if (&value_signature == &chromeos::onc::kNetworkConfigurationSignature) {
    return field_name == ::onc::network_config::kGUID;
  }
  if (&value_signature == &chromeos::onc::kCertificateSignature) {
    return field_name == ::onc::certificate::kGUID;
  }
  if (&value_signature == &chromeos::onc::kCellularSignature) {
    return field_name == ::onc::cellular::kICCID;
  }
  return false;
}

// Identifier fields and other read-only fields (specifically Type) are
// handled specially during merging because they are always identical for the
// various setting sources.
bool IsReadOnlyField(const chromeos::onc::OncValueSignature& value_signature,
                     const std::string& field_name) {
  if (IsIdentifierField(value_signature, field_name)) {
    return true;
  }
  if (&value_signature == &chromeos::onc::kNetworkConfigurationSignature) {
    return field_name == ::onc::network_config::kType;
  }
  return false;
}

// Inserts |true| at every field name in |result| that is recommended in
// |policy|.
void MarkRecommendedFieldnames(const base::Value::Dict& policy,
                               base::Value::Dict* result) {
  const base::Value::List* recommended_value =
      policy.FindList(::onc::kRecommended);
  if (!recommended_value) {
    return;
  }
  for (const auto& value : *recommended_value) {
    if (value.is_string()) {
      result->Set(value.GetString(), true);
    }
  }
}

// Returns the default value for ONC field specified by |field|.
base::Value GetDefaultValue(const chromeos::onc::OncFieldSignature* field) {
  if (field->default_value_setter) {
    return field->default_value_setter();
  }

  DCHECK(field->value_signature);

  // Return the default base::Value for the field type.
  return base::Value(field->value_signature->onc_type);
}

// Returns a dictionary which contains |true| at each path that is editable by
// the user. No other fields are set.
base::Value::Dict GetEditableFlags(const base::Value::Dict& policy) {
  base::Value::Dict result;
  MarkRecommendedFieldnames(policy, &result);

  // Recurse into nested dictionaries.
  for (auto iter : policy) {
    if (iter.first == ::onc::kRecommended || !iter.second.is_dict()) {
      continue;
    }
    result.Set(iter.first, GetEditableFlags(iter.second.GetDict()));
  }
  return result;
}

// If `dict` doesn't have key `key` yet, set it to `value`.
template <typename ValueType>
void SetIfNotSet(base::Value::Dict& dict,
                 std::string_view key,
                 ValueType value) {
  if (dict.Find(key)) {
    return;
  }
  dict.Set(key, std::move(value));
}

// This is the base class for merging a list of Values in parallel. See
// MergeDictionaries function.
class MergeListOfDictionaries {
 public:
  enum {
    // The policies and settings that are being merged.
    kUserPolicyIndex,
    kDevicePolicyIndex,
    kUserSettingsIndex,
    kSharedSettingsIndex,
    kActiveSettingsIndex,
    // Bool values indicating whether the policies/settings are editable.
    kUserEditableIndex,
    kDeviceEditableIndex,
    kLength,
  };
  using DictPointers = std::array<const base::Value::Dict*, kLength>;
  using ValuePointers = std::array<const base::Value*, kLength>;

  MergeListOfDictionaries() = default;
  virtual ~MergeListOfDictionaries() = default;

  MergeListOfDictionaries(const MergeListOfDictionaries&) = delete;
  MergeListOfDictionaries& operator=(const MergeListOfDictionaries&) = delete;

  // For each path in any of the dictionaries |dicts|, either MergeListOfValues
  // or MergeNestedDictionaries is called with the list of values that are
  // located at that path in each of the dictionaries. This function returns a
  // new dictionary containing all results of those calls at the respective
  // paths. The resulting dictionary doesn't contain empty dictionaries.
  base::Value::Dict MergeDictionaries(const DictPointers& dicts) {
    base::Value::Dict result;
    std::set<std::string> visited;
    for (const base::Value::Dict* dict_outer : dicts) {
      if (!dict_outer) {
        continue;
      }

      for (auto field : *dict_outer) {
        const std::string& key = field.first;
        if (key == ::onc::kRecommended || !visited.insert(key).second) {
          continue;
        }

        // Merge as dictionaries or values depending on the type of the value.
        if (field.second.is_dict()) {
          DictPointers dicts_for_key = {0};
          for (size_t i = 0; i < dicts.size(); ++i) {
            dicts_for_key[i] = dicts[i] ? dicts[i]->FindDict(key) : nullptr;
          }

          base::Value::Dict merged_dict =
              MergeNestedDictionaries(key, dicts_for_key);
          if (!merged_dict.empty()) {
            result.Set(key, std::move(merged_dict));
          }
        } else {
          ValuePointers values_for_key = {0};
          for (size_t i = 0; i < dicts.size(); ++i) {
            values_for_key[i] = dicts[i] ? dicts[i]->Find(key) : nullptr;
          }

          base::Value merged_value = MergeListOfValues(key, values_for_key);
          if (!merged_value.is_none()) {
            result.Set(key, std::move(merged_value));
          }
        }
      }
    }
    return result;
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. If a dictionary
  // doesn't contain a path then its value is nullptr.
  virtual base::Value MergeListOfValues(const std::string& key,
                                        const ValuePointers& values) = 0;

  virtual base::Value::Dict MergeNestedDictionaries(const std::string& key,
                                                    const DictPointers& dicts) {
    return MergeDictionaries(dicts);
  }
};

// This is the base class for merging policies and user settings.
class MergeSettingsAndPolicies : public MergeListOfDictionaries {
 public:
  struct ValueParams {
    raw_ptr<const base::Value> user_policy;
    raw_ptr<const base::Value> device_policy;
    raw_ptr<const base::Value> user_setting;
    raw_ptr<const base::Value> shared_setting;
    raw_ptr<const base::Value> active_setting;
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
  base::Value::Dict MergeDictionaries(
      const base::Value::Dict* user_policy,
      const base::Value::Dict* device_policy,
      const base::Value::Dict* user_settings,
      const base::Value::Dict* shared_settings,
      const base::Value::Dict* active_settings) {
    has_user_policy_ = (user_policy != nullptr);
    has_device_policy_ = (device_policy != nullptr);

    base::Value::Dict user_editable;
    if (user_policy) {
      user_editable = GetEditableFlags(*user_policy);
    }

    base::Value::Dict device_editable;
    if (device_policy) {
      device_editable = GetEditableFlags(*device_policy);
    }

    DictPointers dicts;
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
  bool HasUserPolicy() { return has_user_policy_; }

  // Whether a device policy was provided.
  bool HasDevicePolicy() { return has_device_policy_; }

  // MergeListOfDictionaries override.
  base::Value MergeListOfValues(const std::string& key,
                                const ValuePointers& values) override {
    bool user_editable = !HasUserPolicy();
    if (values[kUserEditableIndex] && values[kUserEditableIndex]->is_bool()) {
      user_editable = values[kUserEditableIndex]->GetBool();
    }

    bool device_editable = !HasDevicePolicy();
    if (values[kDeviceEditableIndex] &&
        values[kDeviceEditableIndex]->is_bool()) {
      device_editable = values[kDeviceEditableIndex]->GetBool();
    }

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
  bool has_user_policy_;
  bool has_device_policy_;
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
  // Merges |values| to the effective value (mandatory policy overwrites user
  // settings overwrites shared settings overwrites recommended policy). Returns
  // the effective value, as well as the respective onc::kAugmentation* constant
  // that indicates which source of settings is effective. Note that this
  // function may return nullopt and ::onc::kAugmentationUserPolicy, which means
  // that the user policy didn't set a value but also didn't recommend it, thus
  // enforcing the empty value.
  struct MergeResult {
    base::Value effective_value;
    const char* effective_source = nullptr;
  };
  MergeResult MergeValuesImpl(const std::string& key,
                              const ValueParams& values) {
    const base::Value* value = nullptr;
    MergeResult result;
    if (!values.user_editable) {
      value = values.user_policy;
      result.effective_source = ::onc::kAugmentationUserPolicy;
    } else if (!values.device_editable) {
      value = values.device_policy;
      result.effective_source = ::onc::kAugmentationDevicePolicy;
    } else if (values.user_setting) {
      value = values.user_setting;
      result.effective_source = ::onc::kAugmentationUserSetting;
    } else if (values.shared_setting) {
      value = values.shared_setting;
      result.effective_source = ::onc::kAugmentationSharedSetting;
    } else if (values.user_policy) {
      value = values.user_policy;
      result.effective_source = ::onc::kAugmentationUserPolicy;
    } else if (values.device_policy) {
      value = values.device_policy;
      result.effective_source = ::onc::kAugmentationDevicePolicy;
    } else {
      // Can be reached if the current field is recommended, but none of the
      // dictionaries contained a value for it.
    }

    if (value) {
      result.effective_value = value->Clone();
    }
    return result;
  }

  // MergeSettingsAndPolicies override.
  base::Value MergeValues(const std::string& key,
                          const ValueParams& values) override {
    std::string which;
    return MergeValuesImpl(key, values).effective_value;
  }
};

// Returns true if all not-null values in |values| are equal to |value|.
bool AllPresentValuesEqual(const MergeSettingsAndPolicies::ValueParams& values,
                           const base::Value& value) {
  if (values.user_policy && value != *values.user_policy) {
    return false;
  }
  if (values.device_policy && value != *values.device_policy) {
    return false;
  }
  if (values.user_setting && value != *values.user_setting) {
    return false;
  }
  if (values.shared_setting && value != *values.shared_setting) {
    return false;
  }
  if (values.active_setting && value != *values.active_setting) {
    return false;
  }
  return true;
}

// Call MergeDictionaries to merge policies and settings to an augmented
// dictionary which contains a dictionary for each value in the original
// dictionaries. See the description of MergeSettingsAndPoliciesToAugmented.
class MergeToAugmented : public MergeToEffective {
 public:
  MergeToAugmented() = default;

  MergeToAugmented(const MergeToAugmented&) = delete;
  MergeToAugmented& operator=(const MergeToAugmented&) = delete;

  base::Value::Dict MergeDictionaries(
      const chromeos::onc::OncValueSignature& signature,
      const base::Value::Dict* user_policy,
      const base::Value::Dict* device_policy,
      const base::Value::Dict* user_settings,
      const base::Value::Dict* shared_settings,
      const base::Value::Dict* active_settings) {
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
    if (signature_) {
      field = chromeos::onc::GetFieldSignature(*signature_, key);
    }

    if (!field) {
      // This field is not part of the provided ONCSignature, thus it cannot be
      // controlled by policy. Return the plain active value instead of an
      // augmented dictionary.
      if (values.active_setting) {
        return values.active_setting->Clone();
      }
      return base::Value();
    }

    // This field is part of the provided ONCSignature, thus it can be
    // controlled by policy.
    MergeResult merge_result = MergeToEffective::MergeValuesImpl(key, values);

    if (IsReadOnlyField(*signature_, key)) {
      // Don't augment read-only fields (GUID and Type).
      if (!merge_result.effective_value.is_none()) {
        // DCHECK that all provided fields are identical.
        DCHECK(AllPresentValuesEqual(values, merge_result.effective_value))
            << "Values do not match: " << key
            << " Effective: " << merge_result.effective_value;
        // Return the un-augmented field.
        return std::move(merge_result.effective_value);
      }
      if (values.active_setting) {
        // Unmanaged networks have assigned (active) values.
        return values.active_setting->Clone();
      }
      LOG(ERROR) << "Field has no effective value: " << key;
      return base::Value();
    }

    base::Value::Dict augmented_value;

    if (values.active_setting) {
      augmented_value.Set(::onc::kAugmentationActiveSetting,
                          values.active_setting->Clone());
    }

    if (merge_result.effective_source) {
      augmented_value.Set(::onc::kAugmentationEffectiveSetting,
                          merge_result.effective_source);
    }

    // Prevent credentials from being forwarded in cleartext to UI. User/shared
    // credentials are not stored separately, so they cannot leak here. User and
    // Shared settings are already replaced with |kFakeCredential|.
    bool is_credential = chromeos::onc::FieldIsCredential(*signature_, key);
    if (is_credential) {
      // Set |kFakeCredential| to notify UI that credential is saved.
      if (values.user_policy) {
        augmented_value.Set(::onc::kAugmentationUserPolicy,
                            policy_util::kFakeCredential);
      }
      if (values.device_policy) {
        augmented_value.Set(::onc::kAugmentationDevicePolicy,
                            policy_util::kFakeCredential);
      }
      if (values.active_setting) {
        augmented_value.Set(::onc::kAugmentationActiveSetting,
                            policy_util::kFakeCredential);
      }
    } else {
      if (values.user_policy) {
        augmented_value.Set(::onc::kAugmentationUserPolicy,
                            values.user_policy->Clone());
      }
      if (values.device_policy) {
        augmented_value.Set(::onc::kAugmentationDevicePolicy,
                            values.device_policy->Clone());
      }
    }
    if (values.user_setting) {
      augmented_value.Set(::onc::kAugmentationUserSetting,
                          values.user_setting->Clone());
    }
    if (values.shared_setting) {
      augmented_value.Set(::onc::kAugmentationSharedSetting,
                          values.shared_setting->Clone());
    }

    if (HasUserPolicy() && values.user_editable) {
      augmented_value.Set(::onc::kAugmentationUserEditable, true);

      // Ensure that a property that is editable and has a user policy (which
      // indicates that the policy recommends a value) always has the
      // appropriate default user policy value provided.
      // TODO(b/245885527): Come up with a better handling for ONC defaults.
      SetIfNotSet(augmented_value, ::onc::kAugmentationUserPolicy,
                  GetDefaultValue(field));
      SetIfNotSet(augmented_value, ::onc::kAugmentationEffectiveSetting,
                  ::onc::kAugmentationUserPolicy);
    }
    if (HasDevicePolicy() && values.device_editable) {
      augmented_value.Set(::onc::kAugmentationDeviceEditable, true);

      // Ensure that a property that is editable and has a device policy (which
      // indicates that the policy recommends a value) always has the
      // appropriate default device policy value provided.
      // TODO(b/245885527): Come up with a better handling for ONC defaults.
      SetIfNotSet(augmented_value, ::onc::kAugmentationDevicePolicy,
                  GetDefaultValue(field));
      SetIfNotSet(augmented_value, ::onc::kAugmentationEffectiveSetting,
                  ::onc::kAugmentationDevicePolicy);
    }
    if (!augmented_value.empty()) {
      return base::Value(std::move(augmented_value));
    }

    return base::Value();
  }

  // MergeListOfDictionaries override.
  base::Value::Dict MergeNestedDictionaries(
      const std::string& key,
      const DictPointers& dicts) override {
    if (signature_) {
      const chromeos::onc::OncValueSignature* enclosing_signature = signature_;
      signature_ = nullptr;

      const chromeos::onc::OncFieldSignature* field =
          chromeos::onc::GetFieldSignature(*enclosing_signature, key);
      if (field) {
        signature_ = field->value_signature;
      }
      base::Value::Dict result =
          MergeToEffective::MergeNestedDictionaries(key, dicts);
      signature_ = enclosing_signature;
      return result;
    }
    return MergeToEffective::MergeNestedDictionaries(key, dicts);
  }

 private:
  raw_ptr<const chromeos::onc::OncValueSignature> signature_;
};

}  // namespace

base::Value::Dict MergeSettingsAndPoliciesToEffective(
    const base::Value::Dict* user_policy,
    const base::Value::Dict* device_policy,
    const base::Value::Dict* user_settings,
    const base::Value::Dict* shared_settings) {
  MergeToEffective merger;
  return merger.MergeDictionaries(user_policy, device_policy, user_settings,
                                  shared_settings, nullptr);
}

base::Value::Dict MergeSettingsAndPoliciesToAugmented(
    const chromeos::onc::OncValueSignature& signature,
    const base::Value::Dict* user_policy,
    const base::Value::Dict* device_policy,
    const base::Value::Dict* user_settings,
    const base::Value::Dict* shared_settings,
    const base::Value::Dict* active_settings) {
  MergeToAugmented merger;
  return merger.MergeDictionaries(signature, user_policy, device_policy,
                                  user_settings, shared_settings,
                                  active_settings);
}

}  // namespace ash::onc
