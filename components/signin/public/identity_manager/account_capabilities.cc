// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"

#include <map>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/tribool.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/signin/public/android/jni_headers/AccountCapabilities_jni.h"
#endif

AccountCapabilities::AccountCapabilities() = default;
AccountCapabilities::~AccountCapabilities() = default;
AccountCapabilities::AccountCapabilities(const AccountCapabilities& other) =
    default;
AccountCapabilities::AccountCapabilities(AccountCapabilities&& other) noexcept =
    default;
AccountCapabilities& AccountCapabilities::operator=(
    const AccountCapabilities& other) = default;
AccountCapabilities& AccountCapabilities::operator=(
    AccountCapabilities&& other) noexcept = default;

// static
const std::vector<std::string>&
AccountCapabilities::GetSupportedAccountCapabilityNames() {
  static base::NoDestructor<std::vector<std::string>> kCapabilityNames{{
#define ACCOUNT_CAPABILITY(cpp_label, java_label, value) cpp_label,
#include "components/signin/internal/identity_manager/account_capabilities_list.h"
#undef ACCOUNT_CAPABILITY
  }};
  return *kCapabilityNames;
}

bool AccountCapabilities::AreAnyCapabilitiesKnown() const {
  for (const std::string& capability_name :
       GetSupportedAccountCapabilityNames()) {
    if (GetCapabilityByName(capability_name) != signin::Tribool::kUnknown) {
      return true;
    }
  }
  return false;
}

bool AccountCapabilities::AreAllCapabilitiesKnown() const {
  for (const std::string& capability_name :
       GetSupportedAccountCapabilityNames()) {
    if (GetCapabilityByName(capability_name) == signin::Tribool::kUnknown) {
      return false;
    }
  }
  return true;
}

signin::Tribool AccountCapabilities::GetCapabilityByName(
    const std::string& name) const {
  const auto iterator = capabilities_map_.find(name);
  if (iterator == capabilities_map_.end()) {
    return signin::Tribool::kUnknown;
  }
  return iterator->second ? signin::Tribool::kTrue : signin::Tribool::kFalse;
}

signin::Tribool AccountCapabilities::can_fetch_family_member_info() const {
  return GetCapabilityByName(kCanFetchFamilyMemberInfoCapabilityName);
}

signin::Tribool AccountCapabilities::can_have_email_address_displayed() const {
  return GetCapabilityByName(kCanHaveEmailAddressDisplayedCapabilityName);
}

signin::Tribool AccountCapabilities::
    can_show_history_sync_opt_ins_without_minor_mode_restrictions() const {
  return GetCapabilityByName(
      kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName);
}

signin::Tribool AccountCapabilities::can_run_chrome_privacy_sandbox_trials()
    const {
  return GetCapabilityByName(kCanRunChromePrivacySandboxTrialsCapabilityName);
}

signin::Tribool AccountCapabilities::is_opted_in_to_parental_supervision()
    const {
  return GetCapabilityByName(kIsOptedInToParentalSupervisionCapabilityName);
}

signin::Tribool AccountCapabilities::can_toggle_auto_updates() const {
  return GetCapabilityByName(kCanToggleAutoUpdatesName);
}

signin::Tribool AccountCapabilities::can_use_chrome_ip_protection() const {
  return GetCapabilityByName(kCanUseChromeIpProtectionName);
}

signin::Tribool AccountCapabilities::can_use_devtools_generative_ai_features()
    const {
  return GetCapabilityByName(kCanUseDevToolsGenerativeAiFeaturesCapabilityName);
}

signin::Tribool AccountCapabilities::can_use_edu_features() const {
  return GetCapabilityByName(kCanUseEduFeaturesCapabilityName);
}

signin::Tribool AccountCapabilities::can_use_manta_service() const {
  return GetCapabilityByName(kCanUseMantaServiceName);
}

signin::Tribool AccountCapabilities::can_use_copyeditor_feature() const {
  return GetCapabilityByName(kCanUseCopyEditorFeatureName);
}

signin::Tribool AccountCapabilities::can_use_model_execution_features() const {
  return GetCapabilityByName(kCanUseModelExecutionFeaturesName);
}

signin::Tribool AccountCapabilities::is_allowed_for_machine_learning() const {
  return GetCapabilityByName(kIsAllowedForMachineLearningCapabilityName);
}

signin::Tribool AccountCapabilities::
    is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice() const {
  return GetCapabilityByName(
      kIsSubjectToChromePrivacySandboxRestrictedMeasurementNotice);
}

signin::Tribool AccountCapabilities::is_subject_to_enterprise_policies() const {
  return GetCapabilityByName(kIsSubjectToEnterprisePoliciesCapabilityName);
}

signin::Tribool AccountCapabilities::is_subject_to_parental_controls() const {
  return GetCapabilityByName(kIsSubjectToParentalControlsCapabilityName);
}

signin::Tribool AccountCapabilities::can_use_speaker_label_in_recorder_app()
    const {
  return GetCapabilityByName(kCanUseSpeakerLabelInRecorderApp);
}

signin::Tribool AccountCapabilities::can_use_generative_ai_in_recorder_app()
    const {
  return GetCapabilityByName(kCanUseGenerativeAiInRecorderApp);
}

bool AccountCapabilities::UpdateWith(const AccountCapabilities& other) {
  bool modified = false;

  for (const std::string& name : GetSupportedAccountCapabilityNames()) {
    signin::Tribool other_capability = other.GetCapabilityByName(name);
    signin::Tribool current_capability = GetCapabilityByName(name);
    if (other_capability != signin::Tribool::kUnknown &&
        other_capability != current_capability) {
      capabilities_map_[name] = other_capability == signin::Tribool::kTrue;
      modified = true;
    }
  }

  return modified;
}

bool AccountCapabilities::operator==(const AccountCapabilities& other) const {
  for (const std::string& name : GetSupportedAccountCapabilityNames()) {
    if (GetCapabilityByName(name) != other.GetCapabilityByName(name)) {
      return false;
    }
  }
  return true;
}

#if BUILDFLAG(IS_ANDROID)
// static
AccountCapabilities AccountCapabilities::ConvertFromJavaAccountCapabilities(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& account_capabilities) {
  AccountCapabilities capabilities;
  for (const std::string& name : GetSupportedAccountCapabilityNames()) {
    signin::Tribool capability_state = static_cast<signin::Tribool>(
        signin::Java_AccountCapabilities_getCapabilityByName(
            env, account_capabilities,
            base::android::ConvertUTF8ToJavaString(env, name)));
    if (capability_state != signin::Tribool::kUnknown) {
      capabilities.capabilities_map_[name] =
          capability_state == signin::Tribool::kTrue;
    }
  }
  return capabilities;
}

base::android::ScopedJavaLocalRef<jobject>
AccountCapabilities::ConvertToJavaAccountCapabilities(JNIEnv* env) const {
  const size_t num_caps = capabilities_map_.size();
  std::vector<std::string> capability_names;
  capability_names.reserve(num_caps);
  auto capability_values = base::HeapArray<bool>::WithSize(num_caps);
  size_t value_iterator = 0u;
  for (const auto& [name, value] : capabilities_map_) {
    capability_names.push_back(name);
    capability_values[value_iterator] = value;
    value_iterator++;
  }
  return signin::Java_AccountCapabilities_Constructor(
      env, base::android::ToJavaArrayOfStrings(env, capability_names),
      base::android::ToJavaBooleanArray(env, capability_values));
}
#endif

#if BUILDFLAG(IS_IOS)
AccountCapabilities::AccountCapabilities(
    base::flat_map<std::string, bool> capabilities)
    : capabilities_map_(std::move(capabilities)) {}

const base::flat_map<std::string, bool>&
AccountCapabilities::ConvertToAccountCapabilitiesIOS() {
  return capabilities_map_;
}
#endif
