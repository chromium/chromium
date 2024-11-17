// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_KNOWN_USER_H_
#define COMPONENTS_USER_MANAGER_KNOWN_USER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/user_manager/common_types.h"
#include "components/user_manager/user_manager_export.h"

class AccountId;
enum class AccountType;
class PrefRegistrySimple;
class PrefService;

namespace user_manager {

class UserManagerImpl;

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

  // Updates (or creates) properties associated with |account_id|. Updates
  // value found by |path| with |opt_value|. If |opt_value| has no value it
  // clears the |path| in properties.
  void SetPath(const AccountId& account_id,
               const std::string& path,
               std::optional<base::Value> opt_value);

  // Returns `nullptr` if value is not found or not a string.
  const std::string* FindStringPath(const AccountId& account_id,
                                    std::string_view path) const;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetStringPrefForTest(const AccountId& account_id,
                            const std::string& path,
                            std::string* out_value);

  // Updates user's identified by |account_id| string preference |path|.
  void SetStringPref(const AccountId& account_id,
                     const std::string& path,
                     const std::string& in_value);

  std::optional<bool> FindBoolPath(const AccountId& account_id,
                                   std::string_view path) const;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetBooleanPrefForTest(const AccountId& account_id,
                             const std::string& path,
                             bool* out_value);

  // Updates user's identified by |account_id| boolean preference |path|.
  void SetBooleanPref(const AccountId& account_id,
                      const std::string& path,
                      const bool in_value);

  // Return std::nullopt if the value is not found or doesn't have the int
  // type.
  std::optional<int> FindIntPath(const AccountId& account_id,
                                 std::string_view path) const;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetIntegerPrefForTest(const AccountId& account_id,
                             const std::string& path,
                             int* out_value);

  // Updates user's identified by |account_id| integer preference |path|.
  void SetIntegerPref(const AccountId& account_id,
                      const std::string& path,
                      const int in_value);

  // Return std::nullopt if the value is not found or doesn't have the double
  // type.
  std::optional<double> FindDoublePath(const AccountId& account_id,
                                       std::string_view path) const;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetDoublePrefForTest(const AccountId& account_id,
                            const std::string& path,
                            double* out_value);

  // Updates user's identified by |account_id| double preference |path|.
  void SetDoublePref(const AccountId& account_id,
                     const std::string& path,
                     const double in_value);

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  bool GetPrefForTest(const AccountId& account_id,
                      const std::string& path,
                      const base::Value** out_value);

  const base::Value* FindPath(const AccountId& account_id,
                              const std::string& path) const;

  // Removes user's identified by |account_id| preference |path|.
  void RemovePref(const AccountId& account_id, const std::string& path);

  // Returns the list of known AccountIds.
  std::vector<AccountId> GetKnownAccountIds();

  // This call forms full account id of a known user by email and (optionally)
  // gaia_id.
  // This is a temporary call while migrating to AccountId.
  AccountId GetAccountId(const std::string& user_email,
                         const std::string& id,
                         const AccountType& account_type) const;

  AccountId GetAccountIdByCryptohomeId(const CryptohomeId& cryptohome_id);

  // Saves |account_id| into known users. Tries to commit the change on disk.
  // Use only if account_id is not yet in the known user list. Important if
  // Chrome crashes shortly after starting a session. Cryptohome should be able
  // to find known account_id on Chrome restart.
  void SaveKnownUser(const AccountId& account_id);

  // Updates |account_id.account_type_| and |account_id.GetGaiaId()| or
  // |account_id.GetObjGuid()| for user with |account_id|.
  void UpdateId(const AccountId& account_id);

  // Find GAIA ID for user with `account_id`, returns `nullptr` if not found.
  const std::string* FindGaiaID(const AccountId& account_id);

  // Setter and getter for DeviceId known user string preference.
  void SetDeviceId(const AccountId& account_id, const std::string& device_id);

  std::string GetDeviceId(const AccountId& account_id) const;

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
  // re-auth flow. Returns std::nullopt if value is not set.
  std::optional<int> FindReauthReason(const AccountId& account_id) const;

  // Setter and getter for the information about challenge-response keys that
  // can be used by this user to authenticate. The getter returns a null value
  // when the property isn't present. For the format of the value, refer to
  // chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h.
  void SetChallengeResponseKeys(const AccountId& account_id,
                                base::Value::List value);

  base::Value::List GetChallengeResponseKeys(const AccountId& account_id);

  void SetLastOnlineSignin(const AccountId& account_id, base::Time time);

  base::Time GetLastOnlineSignin(const AccountId& account_id);

  void SetOfflineSigninLimit(const AccountId& account_id,
                             std::optional<base::TimeDelta> time_limit);

  std::optional<base::TimeDelta> GetOfflineSigninLimit(
      const AccountId& account_id);

  void SetIsEnterpriseManaged(const AccountId& account_id,
                              bool is_enterprise_managed);

  bool GetIsEnterpriseManaged(const AccountId& account_id);

  void SetAccountManager(const AccountId& account_id,
                         const std::string& manager);
  const std::string* GetAccountManager(const AccountId& account_id);
  void SetUserLastLoginInputMethodId(const AccountId& account_id,
                                     const std::string& input_method_id);

  const std::string* GetUserLastInputMethodId(const AccountId& account_id);

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

  base::Value::Dict GetAuthFactorCache(const AccountId& account_id);
  void SetAuthFactorCache(const AccountId& account_id,
                          const base::Value::Dict cache);

  // Setter and getter for password sync token used for syncing SAML passwords
  // across multiple user devices.
  void SetPasswordSyncToken(const AccountId& account_id,
                            const std::string& token);

  const std::string* GetPasswordSyncToken(const AccountId& account_id) const;

  // Removes password sync token associated with the user. Can be relevant e.g.
  // if user switches from SAML to GAIA.
  void ClearPasswordSyncToken(const AccountId& account_id);

  // Saves the current major version as the version in which the user completed
  // the onboarding flow.
  void SetOnboardingCompletedVersion(
      const AccountId& account_id,
      const std::optional<base::Version> version);
  std::optional<base::Version> GetOnboardingCompletedVersion(
      const AccountId& account_id);
  void RemoveOnboardingCompletedVersionForTests(const AccountId& account_id);

  // Setter and getter for the last screen shown in the onboarding flow. This
  // is used to resume the onboarding flow if it's not completed yet.
  void SetPendingOnboardingScreen(const AccountId& account_id,
                                  const std::string& screen);

  void RemovePendingOnboardingScreen(const AccountId& account_id);

  std::string GetPendingOnboardingScreen(const AccountId& account_id);

  bool UserExists(const AccountId& account_id);

  // Register known user prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class KnownUserTest;
  friend class UserManagerImpl;

  FRIEND_TEST_ALL_PREFIXES(KnownUserTest,
                           CleanEphemeralUsersRemovesEphemeralAdOnly);
  FRIEND_TEST_ALL_PREFIXES(KnownUserTest, CleanObsoletePrefs);
  FRIEND_TEST_ALL_PREFIXES(KnownUserTest, MigrateOfflineSigninLimit);

  // Performs a lookup of properties associated with |account_id|. Returns
  // nullptr if not found.
  const base::Value::Dict* FindPrefs(const AccountId& account_id) const;

  // Removes all user preferences associated with |account_id|.
  // Not exported as code should not be calling this outside this component
  void RemovePrefs(const AccountId& account_id);

  // Removes all ephemeral users.
  void CleanEphemeralUsers();

  // Marks if user is ephemeral and should be removed on log out.
  void SetIsEphemeralUser(const AccountId& account_id, bool is_ephemeral);

  // Removes all obsolete prefs from all users.
  void CleanObsoletePrefs();

  const raw_ptr<PrefService> local_state_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_KNOWN_USER_H_
