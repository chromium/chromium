// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_KNOWN_USER_H_
#define COMPONENTS_USER_MANAGER_KNOWN_USER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/user_manager/user_manager_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;
enum class AccountType;
class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
class Value;
}

namespace user_manager {

class UserManagerBase;

// Enum describing whether a user's profile requires policy. If kPolicyRequired,
// the profile initialization code will ensure that valid policy is loaded
// before session initialization completes.
enum class ProfileRequiresPolicy {
  kUnknown,
  kPolicyRequired,
  kNoPolicyRequired
};

// Accessor for attributes of per-user properties stored in local_state.
class USER_MANAGER_EXPORT KnownUser final {
 public:
  // Constructing KnownUser is cheap.
  // |local_state| may not be nullptr. This is different from the legacy
  // accessors (user_manager::known_user::) which will return a default value if
  // local_state is not available.
  explicit KnownUser(PrefService* local_state);
  ~KnownUser();

  KnownUser(const KnownUser& other) = delete;
  KnownUser& operator=(const KnownUser& other) = delete;

  // Performs a lookup of properties associated with |account_id|. If found,
  // returns |true| and fills |out_value|. |out_value| can be NULL, if
  // only existence check is required.
  bool FindPrefs(const AccountId& account_id,
                 const base::DictionaryValue** out_value);

  // Updates (or creates) properties associated with |account_id| based
  // on |values|. |clear| defines if existing properties are cleared (|true|)
  // or if it is just a incremental update (|false|).
  void UpdatePrefs(const AccountId& account_id,
                   const base::DictionaryValue& values,
                   bool clear);

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetStringPref(const AccountId& account_id,
                     const std::string& path,
                     std::string* out_value);

  // Updates user's identified by |account_id| string preference |path|.
  void SetStringPref(const AccountId& account_id,
                     const std::string& path,
                     const std::string& in_value);

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetBooleanPref(const AccountId& account_id,
                      const std::string& path,
                      bool* out_value);

  // Updates user's identified by |account_id| boolean preference |path|.
  void SetBooleanPref(const AccountId& account_id,
                      const std::string& path,
                      const bool in_value);

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetIntegerPref(const AccountId& account_id,
                      const std::string& path,
                      int* out_value);

  // Updates user's identified by |account_id| integer preference |path|.
  void SetIntegerPref(const AccountId& account_id,
                      const std::string& path,
                      const int in_value);

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetPref(const AccountId& account_id,
               const std::string& path,
               const base::Value** out_value);

  // Updates user's identified by |account_id| value preference |path|.
  void SetPref(const AccountId& account_id,
               const std::string& path,
               base::Value in_value);

  // Removes user's identified by |account_id| preference |path|.
  void RemovePref(const AccountId& account_id, const std::string& path);

  // Returns the list of known AccountIds.
  std::vector<AccountId> GetKnownAccountIds();

  // This call forms full account id of a known user by email and (optionally)
  // gaia_id.
  // This is a temporary call while migrating to AccountId.
  AccountId GetAccountId(const std::string& user_email,
                         const std::string& id,
                         const AccountType& account_type);

  // Returns true if |subsystem| data was migrated to GaiaId for the
  // |account_id|.
  bool GetGaiaIdMigrationStatus(const AccountId& account_id,
                                const std::string& subsystem);

  // Marks |subsystem| migrated to GaiaId for the |account_id|.
  void SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                                    const std::string& subsystem);

  // Saves |account_id| into known users. Tries to commit the change on disk.
  // Use only if account_id is not yet in the known user list. Important if
  // Chrome crashes shortly after starting a session. Cryptohome should be able
  // to find known account_id on Chrome restart.
  void SaveKnownUser(const AccountId& account_id);

  // Updates |account_id.account_type_| and |account_id.GetGaiaId()| or
  // |account_id.GetObjGuid()| for user with |account_id|.
  void UpdateId(const AccountId& account_id);

  // Find GAIA ID for user with |account_id|, fill in |out_value| and return
  // true
  // if GAIA ID was found or false otherwise.
  // TODO(antrim): Update this once AccountId contains GAIA ID
  // (crbug.com/548926).
  bool FindGaiaID(const AccountId& account_id, std::string* out_value);

  // Setter and getter for DeviceId known user string preference.
  void SetDeviceId(const AccountId& account_id, const std::string& device_id);

  std::string GetDeviceId(const AccountId& account_id);

  // Setter and getter for GAPSCookie known user string preference.
  void SetGAPSCookie(const AccountId& account_id,
                     const std::string& gaps_cookie);

  std::string GetGAPSCookie(const AccountId& account_id);

  // Saves whether the user authenticates using SAML.
  void UpdateUsingSAML(const AccountId& account_id, const bool using_saml);

  // Returns if SAML needs to be used for authentication of the user with
  // |account_id|, if it is known (was set by a |UpdateUsingSaml| call).
  // Otherwise
  // returns false.
  bool IsUsingSAML(const AccountId& account_id);

  // Setter and getter for the known user preference that stores whether the
  // user authenticated via SAML using the principals API.
  void UpdateIsUsingSAMLPrincipalsAPI(const AccountId& account_id,
                                      bool is_using_saml_principals_api);

  bool GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id);

  // Returns whether the current profile requires policy or not (returns UNKNOWN
  // if the profile has never been initialized and so the policy status is
  // not yet known).
  ProfileRequiresPolicy GetProfileRequiresPolicy(const AccountId& account_id);

  // Sets whether the profile requires policy or not.
  void SetProfileRequiresPolicy(const AccountId& account_id,
                                ProfileRequiresPolicy policy_required);

  // Clears information whether profile requires policy.
  void ClearProfileRequiresPolicy(const AccountId& account_id);

  // Saves why the user has to go through re-auth flow.
  void UpdateReauthReason(const AccountId& account_id, const int reauth_reason);

  // Returns the reason why the user with |account_id| has to go through the
  // re-auth flow. Returns true if such a reason was recorded or false
  // otherwise.
  bool FindReauthReason(const AccountId& account_id, int* out_value);

  // Setter and getter for the information about challenge-response keys that
  // can be used by this user to authenticate. The getter returns a null value
  // when the property isn't present. For the format of the value, refer to
  // chromeos/login/auth/challenge_response/known_user_pref_utils.h.
  void SetChallengeResponseKeys(const AccountId& account_id, base::Value value);

  base::Value GetChallengeResponseKeys(const AccountId& account_id);

  void SetLastOnlineSignin(const AccountId& account_id, base::Time time);

  base::Time GetLastOnlineSignin(const AccountId& account_id);

  void SetOfflineSigninLimit(const AccountId& account_id,
                             absl::optional<base::TimeDelta> time_limit);

  absl::optional<base::TimeDelta> GetOfflineSigninLimit(
      const AccountId& account_id);

  void SetIsEnterpriseManaged(const AccountId& account_id,
                              bool is_enterprise_managed);

  bool GetIsEnterpriseManaged(const AccountId& account_id);

  void SetAccountManager(const AccountId& account_id,
                         const std::string& manager);
  bool GetAccountManager(const AccountId& account_id, std::string* manager);
  void SetUserLastLoginInputMethod(const AccountId& account_id,
                                   const std::string& input_method);

  bool GetUserLastInputMethod(const AccountId& account_id,
                              std::string* input_method);

  // Exposes the user's PIN length in local state for PIN auto submit.
  void SetUserPinLength(const AccountId& account_id, int pin_length);

  // Returns the user's PIN length if available, otherwise 0.
  int GetUserPinLength(const AccountId& account_id);

  // Whether the user needs to have their pin auto submit preferences
  // backfilled.
  // TODO(crbug.com/1104164) - Remove this once most users have their
  // preferences backfilled.
  bool PinAutosubmitIsBackfillNeeded(const AccountId& account_id);
  void PinAutosubmitSetBackfillNotNeeded(const AccountId& account_id);
  void PinAutosubmitSetBackfillNeededForTests(const AccountId& account_id);

  // Setter and getter for password sync token used for syncing SAML passwords
  // across multiple user devices.
  void SetPasswordSyncToken(const AccountId& account_id,
                            const std::string& token);

  std::string GetPasswordSyncToken(const AccountId& account_id);

  // Saves the current major version as the version in which the user completed
  // the onboarding flow.
  void SetOnboardingCompletedVersion(
      const AccountId& account_id,
      const absl::optional<base::Version> version);
  absl::optional<base::Version> GetOnboardingCompletedVersion(
      const AccountId& account_id);
  void RemoveOnboardingCompletedVersionForTests(const AccountId& account_id);

  // Setter and getter for the last screen shown in the onboarding flow. This
  // is used to resume the onboarding flow if it's not completed yet.
  void SetPendingOnboardingScreen(const AccountId& account_id,
                                  const std::string& screen);

  void RemovePendingOnboardingScreen(const AccountId& account_id);

  std::string GetPendingOnboardingScreen(const AccountId& account_id);

  // Register known user prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class UserManagerBase;

  FRIEND_TEST_ALL_PREFIXES(KnownUserTest,
                           CleanEphemeralUsersRemovesEphemeralAdOnly);
  FRIEND_TEST_ALL_PREFIXES(KnownUserTest, CleanObsoletePrefs);
  FRIEND_TEST_ALL_PREFIXES(KnownUserTest, MigrateOfflineSigninLimit);

  // Removes |path| from account_id's known user dictionary.
  void ClearPref(const AccountId& account_id, const std::string& path);

  // Removes all user preferences associated with |account_id|.
  // Not exported as code should not be calling this outside this component
  void RemovePrefs(const AccountId& account_id);

  // Removes all ephemeral users.
  void CleanEphemeralUsers();

  // Marks if user is ephemeral and should be removed on log out.
  void SetIsEphemeralUser(const AccountId& account_id, bool is_ephemeral);

  // Removes all obsolete prefs from all users.
  void CleanObsoletePrefs();

  PrefService* const local_state_;
};

// Legacy interface of KnownUsersDatabase.
// TODO(https://crbug.com/1150434): Migrate callers and remove this.
namespace known_user {
// Methods for storage/retrieval of per-user properties in Local State.

// Performs a lookup of properties associated with |account_id|. If found,
// returns |true| and fills |out_value|. |out_value| can be NULL, if
// only existence check is required.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::FindPrefs
// instead.
bool USER_MANAGER_EXPORT FindPrefs(const AccountId& account_id,
                                   const base::DictionaryValue** out_value);

// Updates (or creates) properties associated with |account_id| based
// on |values|. |clear| defines if existing properties are cleared (|true|)
// or if it is just a incremental update (|false|).
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser:: instead.
void USER_MANAGER_EXPORT UpdatePrefs(const AccountId& account_id,
                                     const base::DictionaryValue& values,
                                     bool clear);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetStringPref
// instead.
bool USER_MANAGER_EXPORT GetStringPref(const AccountId& account_id,
                                       const std::string& path,
                                       std::string* out_value);

// Updates user's identified by |account_id| string preference |path|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetStringPref
// instead.
void USER_MANAGER_EXPORT SetStringPref(const AccountId& account_id,
                                       const std::string& path,
                                       const std::string& in_value);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetBooleanPref
// instead.
bool USER_MANAGER_EXPORT GetBooleanPref(const AccountId& account_id,
                                        const std::string& path,
                                        bool* out_value);

// Updates user's identified by |account_id| boolean preference |path|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetBooleanPref
// instead.
void USER_MANAGER_EXPORT SetBooleanPref(const AccountId& account_id,
                                        const std::string& path,
                                        const bool in_value);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetIntegerPref
// instead.
bool USER_MANAGER_EXPORT GetIntegerPref(const AccountId& account_id,
                                        const std::string& path,
                                        int* out_value);

// Updates user's identified by |account_id| integer preference |path|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetIntegerPref
// instead.
void USER_MANAGER_EXPORT SetIntegerPref(const AccountId& account_id,
                                        const std::string& path,
                                        const int in_value);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetPref instead.
bool USER_MANAGER_EXPORT GetPref(const AccountId& account_id,
                                 const std::string& path,
                                 const base::Value** out_value);

// Updates user's identified by |account_id| value preference |path|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetPref instead.
void USER_MANAGER_EXPORT SetPref(const AccountId& account_id,
                                 const std::string& path,
                                 base::Value in_value);

// Removes user's identified by |account_id| preference |path|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::RemovePref
// instead.
void USER_MANAGER_EXPORT RemovePref(const AccountId& account_id,
                                    const std::string& path);

// Returns the list of known AccountIds.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetKnownAccountIds instead.
std::vector<AccountId> USER_MANAGER_EXPORT GetKnownAccountIds();

// This call forms full account id of a known user by email and (optionally)
// gaia_id.
// This is a temporary call while migrating to AccountId.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetAccountId
// instead.
AccountId USER_MANAGER_EXPORT GetAccountId(const std::string& user_email,
                                           const std::string& id,
                                           const AccountType& account_type);

// Returns true if |subsystem| data was migrated to GaiaId for the |account_id|.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetGaiaIdMigrationStatus instead.
bool USER_MANAGER_EXPORT GetGaiaIdMigrationStatus(const AccountId& account_id,
                                                  const std::string& subsystem);

// Marks |subsystem| migrated to GaiaId for the |account_id|.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetGaiaIdMigrationStatusDone instead.
void USER_MANAGER_EXPORT
SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                             const std::string& subsystem);

// Saves |account_id| into known users. Tries to commit the change on disk. Use
// only if account_id is not yet in the known user list. Important if Chrome
// crashes shortly after starting a session. Cryptohome should be able to find
// known account_id on Chrome restart.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SaveKnownUser
// instead.
void USER_MANAGER_EXPORT SaveKnownUser(const AccountId& account_id);

// Updates |account_id.account_type_| and |account_id.GetGaiaId()| or
// |account_id.GetObjGuid()| for user with |account_id|.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::UpdateId instead.
void USER_MANAGER_EXPORT UpdateId(const AccountId& account_id);

// Find GAIA ID for user with |account_id|, fill in |out_value| and return
// true
// if GAIA ID was found or false otherwise.
// TODO(antrim): Update this once AccountId contains GAIA ID
// (crbug.com/548926).
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::FindGaiaID
// instead.
bool USER_MANAGER_EXPORT FindGaiaID(const AccountId& account_id,
                                    std::string* out_value);

// Setter and getter for DeviceId known user string preference.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetDeviceId
// instead.
void USER_MANAGER_EXPORT SetDeviceId(const AccountId& account_id,
                                     const std::string& device_id);

// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetDeviceId
// instead.
std::string USER_MANAGER_EXPORT GetDeviceId(const AccountId& account_id);

// Setter and getter for GAPSCookie known user string preference.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetGAPSCookie
// instead.
void USER_MANAGER_EXPORT SetGAPSCookie(const AccountId& account_id,
                                       const std::string& gaps_cookie);

// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetGAPSCookie
// instead.
std::string USER_MANAGER_EXPORT GetGAPSCookie(const AccountId& account_id);

// Saves whether the user authenticates using SAML.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::UpdateUsingSAML
// instead.
void USER_MANAGER_EXPORT UpdateUsingSAML(const AccountId& account_id,
                                         const bool using_saml);

// Returns if SAML needs to be used for authentication of the user with
// |account_id|, if it is known (was set by a |UpdateUsingSaml| call).
// Otherwise
// returns false.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::IsUsingSAML
// instead.
bool USER_MANAGER_EXPORT IsUsingSAML(const AccountId& account_id);

// Setter and getter for the known user preference that stores whether the user
// authenticated via SAML using the principals API.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::UpdateIsUsingSAMLPrincipalsAPI instead.
void USER_MANAGER_EXPORT
UpdateIsUsingSAMLPrincipalsAPI(const AccountId& account_id,
                               bool is_using_saml_principals_api);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetIsUsingSAMLPrincipalsAPI instead.
bool USER_MANAGER_EXPORT
GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id);

// Returns whether the current profile requires policy or not (returns UNKNOWN
// if the profile has never been initialized and so the policy status is
// not yet known).
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetProfileRequiresPolicy instead.
ProfileRequiresPolicy USER_MANAGER_EXPORT
GetProfileRequiresPolicy(const AccountId& account_id);

// Sets whether the profile requires policy or not.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetProfileRequiresPolicy instead.
void USER_MANAGER_EXPORT
SetProfileRequiresPolicy(const AccountId& account_id,
                         ProfileRequiresPolicy policy_required);

// Clears information whether profile requires policy.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::ClearProfileRequiresPolicy instead.
void USER_MANAGER_EXPORT
ClearProfileRequiresPolicy(const AccountId& account_id);

// Saves why the user has to go through re-auth flow.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::UpdateReauthReason instead.
void USER_MANAGER_EXPORT UpdateReauthReason(const AccountId& account_id,
                                            const int reauth_reason);

// Returns the reason why the user with |account_id| has to go through the
// re-auth flow. Returns true if such a reason was recorded or false
// otherwise.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::FindReauthReason
// instead.
bool USER_MANAGER_EXPORT FindReauthReason(const AccountId& account_id,
                                          int* out_value);

// Setter and getter for the information about challenge-response keys that can
// be used by this user to authenticate.
// The getter returns a null value when the property isn't present.
// For the format of the value, refer to
// chromeos/login/auth/challenge_response/known_user_pref_utils.h.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetChallengeResponseKeys instead.
void USER_MANAGER_EXPORT SetChallengeResponseKeys(const AccountId& account_id,
                                                  base::Value value);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetChallengeResponseKeys instead.
base::Value USER_MANAGER_EXPORT
GetChallengeResponseKeys(const AccountId& account_id);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetLastOnlineSignin instead.
void USER_MANAGER_EXPORT SetLastOnlineSignin(const AccountId& account_id,
                                             base::Time time);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetLastOnlineSignin instead.
base::Time USER_MANAGER_EXPORT GetLastOnlineSignin(const AccountId& account_id);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetOfflineSigninLimit instead.
void USER_MANAGER_EXPORT
SetOfflineSigninLimit(const AccountId& account_id,
                      absl::optional<base::TimeDelta> time_limit);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetOfflineSigninLimit instead.
absl::optional<base::TimeDelta> USER_MANAGER_EXPORT
GetOfflineSigninLimit(const AccountId& account_id);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetIsEnterpriseManaged instead.
void USER_MANAGER_EXPORT SetIsEnterpriseManaged(const AccountId& account_id,
                                                bool is_enterprise_managed);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetIsEnterpriseManaged instead.
bool USER_MANAGER_EXPORT GetIsEnterpriseManaged(const AccountId& account_id);

// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetAccountManager
// instead.
void USER_MANAGER_EXPORT SetAccountManager(const AccountId& account_id,
                                           const std::string& manager);
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetAccountManager
// instead.
bool USER_MANAGER_EXPORT GetAccountManager(const AccountId& account_id,
                                           std::string* manager);
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetUserLastLoginInputMethod instead.
void USER_MANAGER_EXPORT
SetUserLastLoginInputMethod(const AccountId& account_id,
                            const std::string& input_method);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetUserLastInputMethod instead.
bool USER_MANAGER_EXPORT GetUserLastInputMethod(const AccountId& account_id,
                                                std::string* input_method);

// Exposes the user's PIN length in local state for PIN auto submit.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::SetUserPinLength
// instead.
void USER_MANAGER_EXPORT SetUserPinLength(const AccountId& account_id,
                                          int pin_length);

// Returns the user's PIN length if available, otherwise 0.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser::GetUserPinLength
// instead.
int USER_MANAGER_EXPORT GetUserPinLength(const AccountId& account_id);

// Whether the user needs to have their pin auto submit preferences backfilled.
// TODO(crbug.com/1104164) - Remove this once most users have their
// preferences backfilled.
// TODO(https://crbug.com/1150434): Deprecated, use KnownUser:: equivalents
// instead.
bool USER_MANAGER_EXPORT
PinAutosubmitIsBackfillNeeded(const AccountId& account_id);
void USER_MANAGER_EXPORT
PinAutosubmitSetBackfillNotNeeded(const AccountId& account_id);
void USER_MANAGER_EXPORT
PinAutosubmitSetBackfillNeededForTests(const AccountId& account_id);

// Setter and getter for password sync token used for syncing SAML passwords
// across multiple user devices.
// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::SetPasswordSyncToken instead.
void USER_MANAGER_EXPORT SetPasswordSyncToken(const AccountId& account_id,
                                              const std::string& token);

// TODO(https://crbug.com/1150434): Deprecated, use
// KnownUser::GetPasswordSyncToken instead.
std::string USER_MANAGER_EXPORT
GetPasswordSyncToken(const AccountId& account_id);

}  // namespace known_user
}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_KNOWN_USER_H_
