// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/identity_manager/tribool.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace ios {
class AccountCapabilitiesFetcherIOS;
}  // namespace ios

namespace supervised_user {
class FamilyLinkUserCapabilitiesObserver;
}  // namespace supervised_user

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

  explicit AccountCapabilities(base::flat_map<std::string, bool> capabilities);

#if BUILDFLAG(IS_IOS)
  const base::flat_map<std::string, bool>& ConvertToAccountCapabilitiesIOS();
#endif

  // clang-format off
  // keep-sorted start newline_separated=yes sticky_prefixes=#if group_prefixes=#endif,can,has,is,must
  // clang-format on
  // Chrome can fetch information related to the family
  // group for accounts with this capability.
  signin::Tribool can_fetch_family_member_info() const;

  // Chrome can display the email address for accounts with this capability.
  signin::Tribool can_have_email_address_displayed() const;

#if !BUILDFLAG(IS_ANDROID)
  // The primary account type is suitable for choice screens. Signals that are
  // not account-type specific should be checked separately.
  signin::Tribool can_make_chrome_search_engine_choice_screen_choice() const;
#endif

  // Chrome can run privacy sandbox trials for accounts with this capability.
  signin::Tribool can_run_chrome_privacy_sandbox_trials() const;

  // Chrome can show history sync opt in screens without minor mode
  // restrictions with this capability.
  signin::Tribool
  can_show_history_sync_opt_ins_without_minor_mode_restrictions() const;

  // Chrome can toggle auto updates with this capability.
  signin::Tribool can_toggle_auto_updates() const;

  // The user account is able to use IP Protection.
  signin::Tribool can_use_chrome_ip_protection() const;

#if BUILDFLAG(IS_CHROMEOS)
  // The user account is able to use generative AI features. Since many
  // generative AI features inherit the same capability (minor restrictions),
  // this one should be used for future generative AI features, instead of using
  // a separate one for each of them.
  signin::Tribool can_use_chromeos_generative_ai() const;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // The user account is able to use copyeditor feature.
  signin::Tribool can_use_copyeditor_feature() const;

  // The user account is able to use DevTools AI features.
  signin::Tribool can_use_devtools_generative_ai_features() const;

  // The user account is able to use edu features.
  signin::Tribool can_use_edu_features() const;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // The user account is able to use Gemini in Chrome.
  signin::Tribool can_use_gemini_in_chrome() const;
#endif

  // The user account is able to use generative AI in recorder app.
  signin::Tribool can_use_generative_ai_in_recorder_app() const;

  // The user account is able to use generative AI photo editing.
  signin::Tribool can_use_generative_ai_photo_editing() const;

  // The user account is able to use manta service.
  signin::Tribool can_use_manta_service() const;

  // The user account is able to use model execution features.
  signin::Tribool can_use_model_execution_features() const;

  // The user account is able to use speaker label in recorder app.
  signin::Tribool can_use_speaker_label_in_recorder_app() const;

  // Chrome can send user data to Google servers for machine learning purposes
  // with this capability.
  signin::Tribool is_allowed_for_machine_learning() const;

  // The user account has opted in to parental supervision (Geller account).
  // Chrome applies parental controls to accounts with this capability.
  signin::Tribool is_opted_in_to_parental_supervision() const;

  // Chrome applies account level enterprise policies to profiles signed in
  // with accounts with this capability.
  signin::Tribool is_subject_to_account_level_enterprise_policies() const;

  // Chrome must show the notice before using the privacy sandbox restricted
  // measurement API
  signin::Tribool
  is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice() const;

  // Chrome applies enterprise features to accounts with this capability.
  // This capability returns true all managed accounts and does not reflect
  // whether the account will receive account level enterprise policies or not
  // when signed in to Chrome.
  signin::Tribool is_subject_to_enterprise_features() const;

  // Chrome applies parental controls to accounts with this capability.
  signin::Tribool is_subject_to_parental_controls() const;

  // keep-sorted end

  // Whether at least one of the capabilities is not
  // `signin::Tribool::kUnknown`.
  bool AreAnyCapabilitiesKnown() const;

  // Whether none of the capabilities has `signin::Tribool::kUnknown`.
  bool AreAllCapabilitiesKnown() const;

  // Updates the capability state value for keys in `other`. If a value is
  // `signin::Tribool::kUnknown` in `other` the corresponding key will not
  // be updated in order to avoid overriding known values.
  bool UpdateWith(const AccountCapabilities& other);

  bool operator==(const AccountCapabilities& other) const;

 private:
  // Returns the list of account capability service names supported in Chrome.
  static base::span<const std::string_view>
  GetSupportedAccountCapabilityNames();

  // Returns the capability state using the service name.
  signin::Tribool GetCapabilityByName(std::string_view name) const;

  friend std::optional<AccountCapabilities>
  signin::AccountCapabilitiesFromServerResponse(
      const base::Value::Dict& account_capabilities);
  friend base::Value::Dict signin::SerializeAccountCapabilities(
      const AccountCapabilities& account_capabilities);
  friend AccountCapabilities signin::DeserializeAccountCapabilities(
      const base::Value::Dict& dict);
  friend class AccountCapabilitiesFetcherGaia;
#if BUILDFLAG(IS_IOS)
  friend base::span<const std::string_view>
  GetAccountCapabilityNamesForPrefetch();
  friend class ios::AccountCapabilitiesFetcherIOS;
#endif
  friend class AccountCapabilitiesTestMutator;
  friend class supervised_user::FamilyLinkUserCapabilitiesObserver;

  base::flat_map<std::string, bool> capabilities_map_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
