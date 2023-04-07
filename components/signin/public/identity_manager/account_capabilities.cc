// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "components/signin/public/identity_manager/account_capabilities.h"

#include "base/no_destructor.h"
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

signin::Tribool AccountCapabilities::can_have_email_address_displayed() const {
  return GetCapabilityByName(kCanHaveEmailAddressDisplayedCapabilityName);
}

signin::Tribool AccountCapabilities::can_offer_extended_chrome_sync_promos()
    const {
  return GetCapabilityByName(kCanOfferExtendedChromeSyncPromosCapabilityName);
}

signin::Tribool AccountCapabilities::can_run_chrome_privacy_sandbox_trials()
    const {
  return GetCapabilityByName(kCanRunChromePrivacySandboxTrialsCapabilityName);
}

signin::Tribool AccountCapabilities::can_stop_parental_supervision() const {
  return GetCapabilityByName(kCanStopParentalSupervisionCapabilityName);
}

signin::Tribool AccountCapabilities::can_toggle_auto_updates() const {
  return GetCapabilityByName(kCanToggleAutoUpdatesName);
}

signin::Tribool AccountCapabilities::is_allowed_for_machine_learning() const {
  return GetCapabilityByName(kIsAllowedForMachineLearningCapabilityName);
}

// Temporary implementation that return true for accounts that are not allowed
// to run privacy sandbox trials and that are not subject for parental controls.
//
// TODO(crbug.com/1430845): Update to use the real account capability once it
// is defined server-side.
signin::Tribool AccountCapabilities::
    is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice() const {
  signin::Tribool can_run_chrome_privacy_sandbox_trials =
      AccountCapabilities::can_run_chrome_privacy_sandbox_trials();
  signin::Tribool is_subject_to_parental_controls =
      AccountCapabilities::is_subject_to_parental_controls();

  if (can_run_chrome_privacy_sandbox_trials == signin::Tribool::kUnknown ||
      is_subject_to_parental_controls == signin::Tribool::kUnknown) {
    return signin::Tribool::kUnknown;
  }
  if (can_run_chrome_privacy_sandbox_trials == signin::Tribool::kTrue ||
      is_subject_to_parental_controls == signin::Tribool::kTrue) {
    return signin::Tribool::kFalse;
  }
  return signin::Tribool::kTrue;
}

signin::Tribool AccountCapabilities::is_subject_to_enterprise_policies() const {
  return GetCapabilityByName(kIsSubjectToEnterprisePoliciesCapabilityName);
}

signin::Tribool AccountCapabilities::is_subject_to_parental_controls() const {
  return GetCapabilityByName(kIsSubjectToParentalControlsCapabilityName);
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
    if (GetCapabilityByName(name) != other.GetCapabilityByName(name))
      return false;
  }
  return true;
}

bool AccountCapabilities::operator!=(const AccountCapabilities& other) const {
  return !(*this == other);
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
  int capabilities_size = capabilities_map_.size();
  std::vector<std::string> capability_names;
  auto capability_values = std::make_unique<bool[]>(capabilities_size);
  int value_iterator = 0;
  for (const auto& kv : capabilities_map_) {
    capability_names.push_back(kv.first);
    capability_values[value_iterator] = kv.second;
    value_iterator++;
  }
  return signin::Java_AccountCapabilities_Constructor(
      env, base::android::ToJavaArrayOfStrings(env, capability_names),
      base::android::ToJavaBooleanArray(env, capability_values.get(),
                                        capabilities_size));
}
#endif

#if BUILDFLAG(IS_IOS)
AccountCapabilities::AccountCapabilities(
    base::flat_map<std::string, bool> capabilities)
    : capabilities_map_(std::move(capabilities)) {}
#endif
