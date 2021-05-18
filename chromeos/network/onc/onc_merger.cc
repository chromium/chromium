// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_merger.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/policy_util.h"
#include "components/onc/onc_constants.h"

namespace chromeos {
namespace onc {
namespace {

typedef std::unique_ptr<base::DictionaryValue> DictionaryPtr;

// Returns true if the field is the identifier of a configuration, i.e. the GUID
// of a network or a certificate.
bool IsIdentifierField(const OncValueSignature& value_signature,
                       const std::string& field_name) {
  if (&value_signature == &kNetworkConfigurationSignature)
    return field_name == ::onc::network_config::kGUID;
  if (&value_signature == &kCertificateSignature)
    return field_name == ::onc::certificate::kGUID;
  return false;
}

// Identifier fields and other read-only fields (specifically Type) are
// handled specially during merging because they are always identical for the
// various setting sources.
bool IsReadOnlyField(const OncValueSignature& value_signature,
                     const std::string& field_name) {
  if (IsIdentifierField(value_signature, field_name))
    return true;
  if (&value_signature == &kNetworkConfigurationSignature)
    return field_name == ::onc::network_config::kType;
  return false;
}

// Inserts |true| at every field name in |result| that is recommended in
// |policy|.
void MarkRecommendedFieldnames(const base::DictionaryValue& policy,
                               base::DictionaryValue* result) {
  const base::ListValue* recommended_value = NULL;
  if (!policy.GetListWithoutPathExpansion(::onc::kRecommended,
                                          &recommended_value))
    return;
  for (const auto& value : recommended_value->GetList()) {
    std::string entry;
    if (value.GetAsString(&entry))
      result->SetKey(entry, base::Value(true));
  }
}

// Returns a dictionary which contains |true| at each path that is editable by
// the user. No other fields are set.
DictionaryPtr GetEditableFlags(const base::DictionaryValue& policy) {
  DictionaryPtr result_editable(new base::DictionaryValue);
  MarkRecommendedFieldnames(policy, result_editable.get());

  // Recurse into nested dictionaries.
  for (base::DictionaryValue::Iterator it(policy); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* child_policy = NULL;
    if (it.key() == ::onc::kRecommended ||
        !it.value().GetAsDictionary(&child_policy)) {
      continue;
    }

    result_editable->SetKey(it.key(), base::Value::FromUniquePtrValue(
                                          GetEditableFlags(*child_policy)));
  }
  return result_editable;
}

// This is the base class for merging a list of DictionaryValues in
// parallel. See MergeDictionaries function.
class MergeListOfDictionaries {
 public:
  typedef std::vector<const base::DictionaryValue*> DictPtrs;

  MergeListOfDictionaries() = default;

  virtual ~MergeListOfDictionaries() = default;

  // For each path in any of the dictionaries |dicts|, the function
  // MergeListOfValues is called with the list of values that are located at
  // that path in each of the dictionaries. This function returns a new
  // dictionary containing all results of MergeListOfValues at the respective
  // paths. The resulting dictionary doesn't contain empty dictionaries.
  DictionaryPtr MergeDictionaries(const DictPtrs &dicts) {
    DictionaryPtr result(new base::DictionaryValue);
    std::set<std::string> visited;
    for (DictPtrs::const_iterator it_outer = dicts.begin();
         it_outer != dicts.end(); ++it_outer) {
      if (!*it_outer)
        continue;

      for (base::DictionaryValue::Iterator field(**it_outer); !field.IsAtEnd();
           field.Advance()) {
        const std::string& key = field.key();
        if (key == ::onc::kRecommended || !visited.insert(key).second)
          continue;

        std::unique_ptr<base::Value> merged_value;
        if (field.value().is_dict()) {
          DictPtrs nested_dicts;
          for (DictPtrs::const_iterator it_inner = dicts.begin();
               it_inner != dicts.end(); ++it_inner) {
            const base::DictionaryValue* nested_dict = NULL;
            if (*it_inner)
              (*it_inner)->GetDictionaryWithoutPathExpansion(key, &nested_dict);
            nested_dicts.push_back(nested_dict);
          }
          DictionaryPtr merged_dict(MergeNestedDictionaries(key, nested_dicts));
          if (!merged_dict->DictEmpty())
            merged_value = std::move(merged_dict);
        } else {
          std::vector<const base::Value*> values;
          for (DictPtrs::const_iterator it_inner = dicts.begin();
               it_inner != dicts.end(); ++it_inner) {
            const base::Value* value = NULL;
            if (*it_inner)
              (*it_inner)->GetWithoutPathExpansion(key, &value);
            values.push_back(value);
          }
          merged_value = MergeListOfValues(key, values);
        }

        if (merged_value)
          result->SetKey(
              key, base::Value::FromUniquePtrValue(std::move(merged_value)));
      }
    }
    return result;
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. The order of the
  // values is the same as of the given dictionaries |dicts|. If a dictionary
  // doesn't contain a path then it's value is NULL.
  virtual std::unique_ptr<base::Value> MergeListOfValues(
      const std::string& key,
      const std::vector<const base::Value*>& values) = 0;

  virtual DictionaryPtr MergeNestedDictionaries(const std::string& key,
                                                const DictPtrs &dicts) {
    return MergeDictionaries(dicts);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MergeListOfDictionaries);
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

  // Merge the provided dictionaries. For each path in any of the dictionaries,
  // MergeValues is called. Its results are collected in a new dictionary which
  // is then returned. The resulting dictionary never contains empty
  // dictionaries.
  DictionaryPtr MergeDictionaries(
      const base::DictionaryValue* user_policy,
      const base::DictionaryValue* device_policy,
      const base::DictionaryValue* user_settings,
      const base::DictionaryValue* shared_settings,
      const base::DictionaryValue* active_settings) {
    hasUserPolicy_ = (user_policy != NULL);
    hasDevicePolicy_ = (device_policy != NULL);

    DictionaryPtr user_editable;
    if (user_policy != NULL)
      user_editable = GetEditableFlags(*user_policy);

    DictionaryPtr device_editable;
    if (device_policy != NULL)
      device_editable = GetEditableFlags(*device_policy);

    std::vector<const base::DictionaryValue*> dicts(kLastIndex, NULL);
    dicts[kUserPolicyIndex] = user_policy;
    dicts[kDevicePolicyIndex] = device_policy;
    dicts[kUserSettingsIndex] = user_settings;
    dicts[kSharedSettingsIndex] = shared_settings;
    dicts[kActiveSettingsIndex] = active_settings;
    dicts[kUserEditableIndex] = user_editable.get();
    dicts[kDeviceEditableIndex] = device_editable.get();
    return MergeListOfDictionaries::MergeDictionaries(dicts);
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. Implementations
  // can use the Has*Policy functions.
  virtual std::unique_ptr<base::Value> MergeValues(
      const std::string& key,
      const ValueParams& values) = 0;

  // Whether a user policy was provided.
  bool HasUserPolicy() {
    return hasUserPolicy_;
  }

  // Whether a device policy was provided.
  bool HasDevicePolicy() {
    return hasDevicePolicy_;
  }

  // MergeListOfDictionaries override.
  std::unique_ptr<base::Value> MergeListOfValues(
      const std::string& key,
      const std::vector<const base::Value*>& values) override {
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

  DISALLOW_COPY_AND_ASSIGN(MergeSettingsAndPolicies);
};

// Call MergeDictionaries to merge policies and settings to the effective
// values. This ignores the active settings of Shill. See the description of
// MergeSettingsAndPoliciesToEffective.
class MergeToEffective : public MergeSettingsAndPolicies {
 public:
  MergeToEffective() = default;

 protected:
  // Merges |values| to the effective value (Mandatory policy overwrites user
  // settings overwrites shared settings overwrites recommended policy). |which|
  // is set to the respective onc::kAugmentation* constant that indicates which
  // source of settings is effective. Note that this function may return a NULL
  // pointer and set |which| to ::onc::kAugmentationUserPolicy, which means that
  // the
  // user policy didn't set a value but also didn't recommend it, thus enforcing
  // the empty value.
  std::unique_ptr<base::Value> MergeValues(const std::string& key,
                                           const ValueParams& values,
                                           std::string* which) {
    const base::Value* result = NULL;
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
    if (result)
      return base::Value::ToUniquePtrValue(result->Clone());
    return nullptr;
  }

  // MergeSettingsAndPolicies override.
  std::unique_ptr<base::Value> MergeValues(const std::string& key,
                                           const ValueParams& values) override {
    std::string which;
    return MergeValues(key, values, &which);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MergeToEffective);
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

  DictionaryPtr MergeDictionaries(
      const OncValueSignature& signature,
      const base::DictionaryValue* user_policy,
      const base::DictionaryValue* device_policy,
      const base::DictionaryValue* user_settings,
      const base::DictionaryValue* shared_settings,
      const base::DictionaryValue* active_settings) {
    signature_ = &signature;
    return MergeToEffective::MergeDictionaries(user_policy,
                                               device_policy,
                                               user_settings,
                                               shared_settings,
                                               active_settings);
  }

 protected:
  // MergeSettingsAndPolicies override.
  std::unique_ptr<base::Value> MergeValues(const std::string& key,
                                           const ValueParams& values) override {
    const OncFieldSignature* field = NULL;
    if (signature_)
      field = GetFieldSignature(*signature_, key);

    if (!field) {
      // This field is not part of the provided ONCSignature, thus it cannot be
      // controlled by policy. Return the plain active value instead of an
      // augmented dictionary.
      if (values.active_setting)
        return base::Value::ToUniquePtrValue(values.active_setting->Clone());
      return nullptr;
    }

    // This field is part of the provided ONCSignature, thus it can be
    // controlled by policy.
    std::string which_effective;
    std::unique_ptr<base::Value> effective_value =
        MergeToEffective::MergeValues(key, values, &which_effective);

    if (IsReadOnlyField(*signature_, key)) {
      // Don't augment read-only fields (GUID and Type).
      if (effective_value) {
        // DCHECK that all provided fields are identical.
        DCHECK(AllPresentValuesEqual(values, *effective_value))
            << "Values do not match: " << key
            << " Effective: " << *effective_value;
        // Return the un-augmented field.
        return effective_value;
      }
      if (values.active_setting) {
        // Unmanaged networks have assigned (active) values.
        return base::Value::ToUniquePtrValue(values.active_setting->Clone());
      }
      LOG(ERROR) << "Field has no effective value: " << key;
      return nullptr;
    }

    std::unique_ptr<base::DictionaryValue> augmented_value(
        new base::DictionaryValue);

    if (values.active_setting) {
      augmented_value->SetKey(::onc::kAugmentationActiveSetting,
                              values.active_setting->Clone());
    }

    if (!which_effective.empty()) {
      augmented_value->SetKey(::onc::kAugmentationEffectiveSetting,
                              base::Value(which_effective));
    }

    // Prevent credentials from being forwarded in cleartext to UI.
    // User/shared credentials are not stored separately, so they cannot
    // leak here.
    // User and Shared settings are already replaced with |kFakeCredential|.
    bool is_credential = onc::FieldIsCredential(*signature_, key);
    if (is_credential) {
      // Set |kFakeCredential| to notify UI that credential is saved.
      if (values.user_policy) {
        augmented_value->SetKey(
            ::onc::kAugmentationUserPolicy,
            base::Value(chromeos::policy_util::kFakeCredential));
      }
      if (values.device_policy) {
        augmented_value->SetKey(
            ::onc::kAugmentationDevicePolicy,
            base::Value(chromeos::policy_util::kFakeCredential));
      }
      if (values.active_setting) {
        augmented_value->SetKey(
            ::onc::kAugmentationActiveSetting,
            base::Value(chromeos::policy_util::kFakeCredential));
      }
    } else {
      if (values.user_policy) {
        augmented_value->SetKey(::onc::kAugmentationUserPolicy,
                                values.user_policy->Clone());
      }
      if (values.device_policy) {
        augmented_value->SetKey(::onc::kAugmentationDevicePolicy,
                                values.device_policy->Clone());
      }
    }
    if (values.user_setting) {
      augmented_value->SetKey(::onc::kAugmentationUserSetting,
                              values.user_setting->Clone());
    }
    if (values.shared_setting) {
      augmented_value->SetKey(::onc::kAugmentationSharedSetting,
                              values.shared_setting->Clone());
    }
    if (HasUserPolicy() && values.user_editable) {
      augmented_value->SetKey(::onc::kAugmentationUserEditable,
                              base::Value(true));
    }
    if (HasDevicePolicy() && values.device_editable) {
      augmented_value->SetKey(::onc::kAugmentationDeviceEditable,
                              base::Value(true));
    }
    if (augmented_value->DictEmpty())
      augmented_value.reset();
    return std::move(augmented_value);
  }

  // MergeListOfDictionaries override.
  DictionaryPtr MergeNestedDictionaries(const std::string& key,
                                        const DictPtrs& dicts) override {
    DictionaryPtr result;
    if (signature_) {
      const OncValueSignature* enclosing_signature = signature_;
      signature_ = NULL;

      const OncFieldSignature* field =
          GetFieldSignature(*enclosing_signature, key);
      if (field)
        signature_ = field->value_signature;
      result = MergeToEffective::MergeNestedDictionaries(key, dicts);

      signature_ = enclosing_signature;
    } else {
      result = MergeToEffective::MergeNestedDictionaries(key, dicts);
    }
    return result;
  }

 private:
  const OncValueSignature* signature_;
  DISALLOW_COPY_AND_ASSIGN(MergeToAugmented);
};

}  // namespace

DictionaryPtr MergeSettingsAndPoliciesToEffective(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings) {
  MergeToEffective merger;
  return merger.MergeDictionaries(
      user_policy, device_policy, user_settings, shared_settings, NULL);
}

DictionaryPtr MergeSettingsAndPoliciesToAugmented(
    const OncValueSignature& signature,
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings,
    const base::DictionaryValue* active_settings) {
  MergeToAugmented merger;
  return merger.MergeDictionaries(
      signature, user_policy, device_policy, user_settings, shared_settings,
      active_settings);
}

}  // namespace onc
}  // namespace chromeos
