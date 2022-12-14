// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace ios {
class AccountCapabilitiesFetcherIOS;
}  // namespace ios

// Stores the information about account capabilities. Capabilities provide
// information about state and features of Gaia accounts.
class AccountCapabilities {
 public:
  AccountCapabilities();
  ~AccountCapabilities();
  AccountCapabilities(const AccountCapabilities& other);
  AccountCapabilities(AccountCapabilities&& other) noexcept;
  AccountCapabilities& operator=(const AccountCapabilities& other);
  AccountCapabilities& operator=(AccountCapabilities&& other) noexcept;

#if BUILDFLAG(IS_ANDROID)
  static AccountCapabilities ConvertFromJavaAccountCapabilities(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& accountCapabilities);

  base::android::ScopedJavaLocalRef<jobject> ConvertToJavaAccountCapabilities(
      JNIEnv* env) const;
#endif

#if BUILDFLAG(IS_IOS)
  AccountCapabilities(base::flat_map<std::string, bool> capabilities);
#endif
  // Keep sorted alphabetically.

  // Chrome can display the email address for accounts with this capability.
  signin::Tribool can_have_email_address_displayed() const;

  // Chrome can offer extended promos for turning on Sync to accounts with this
  // capability.
  signin::Tribool can_offer_extended_chrome_sync_promos() const;

  // Chrome can run privacy sandbox trials for accounts with this capability.
  signin::Tribool can_run_chrome_privacy_sandbox_trials() const;

  // Chrome can stop parental supervision if the user chooses to do so with
  // this capability.
  signin::Tribool can_stop_parental_supervision() const;

  // Chrome can toggle auto updates with this capability.
  signin::Tribool can_toggle_auto_updates() const;

  // Chrome can send user data to Google servers for machine learning purposes
  // with this capability.
  signin::Tribool is_allowed_for_machine_learning() const;

  // Chrome applies parental controls to accounts with this capability.
  signin::Tribool is_subject_to_parental_controls() const;

  // Whether none of the capabilities has `signin::Tribool::kUnknown`.
  bool AreAllCapabilitiesKnown() const;

  // Updates the capability state value for keys in `other`. If a value is
  // `signin::Tribool::kUnknown` in `other` the corresponding key will not
  // be updated in order to avoid overriding known values.
  bool UpdateWith(const AccountCapabilities& other);

  bool operator==(const AccountCapabilities& other) const;
  bool operator!=(const AccountCapabilities& other) const;

 private:
  friend absl::optional<AccountCapabilities> AccountCapabilitiesFromValue(
      const base::Value::Dict& account_capabilities);
  friend class AccountCapabilitiesFetcherGaia;
#if BUILDFLAG(IS_IOS)
  friend class ios::AccountCapabilitiesFetcherIOS;
#endif
  friend class AccountCapabilitiesTestMutator;
  friend class AccountTrackerService;

  // Returns the capability state using the service name.
  signin::Tribool GetCapabilityByName(const std::string& name) const;

  // Returns the list of account capability service names supported in Chrome.
  static const std::vector<std::string>& GetSupportedAccountCapabilityNames();

  base::flat_map<std::string, bool> capabilities_map_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
