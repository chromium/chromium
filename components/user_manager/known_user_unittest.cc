// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {
namespace {
std::optional<std::string> GetStringPrefValue(KnownUser* known_user,
                                              const AccountId& account_id,
                                              const char* pref_name) {
  if (const std::string* value =
          known_user->FindStringPath(account_id, pref_name)) {
    return *value;
  }
  return std::nullopt;
}
}  // namespace

// Base class for tests of known_user.
// Sets up global objects necessary for known_user to be able to access
// local_state.
class KnownUserTest : public testing::Test {
 public:
  KnownUserTest() {
    auto fake_user_manager = std::make_unique<FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ =
        std::make_unique<ScopedUserManager>(std::move(fake_user_manager));

    UserManagerImpl::RegisterPrefs(local_state_.registry());
  }
  ~KnownUserTest() override = default;

  KnownUserTest(const KnownUserTest& other) = delete;
  KnownUserTest& operator=(const KnownUserTest& other) = delete;

 protected:
  const AccountId kDefaultAccountId =
      AccountId::FromUserEmailGaiaId("default_account@gmail.com",
                                     "fake-gaia-id");
  FakeUserManager* fake_user_manager() { return fake_user_manager_; }

  PrefService* local_state() { return &local_state_; }

  const base::Value::Dict* FindPrefs(const AccountId& account_id) {
    return KnownUser(local_state()).FindPrefs(account_id);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  // Owned by |scoped_user_manager_|.
  raw_ptr<FakeUserManager, DanglingUntriaged> fake_user_manager_ = nullptr;
  std::unique_ptr<ScopedUserManager> scoped_user_manager_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(KnownUserTest, FindPrefsNonExisting) {
  EXPECT_FALSE(FindPrefs(kDefaultAccountId));
}

TEST_F(KnownUserTest, FindPrefsExisting) {
  KnownUser known_user(local_state());
  const std::string kCustomPrefName = "custom_pref";
  known_user.SetStringPref(kDefaultAccountId, kCustomPrefName, "value");

  const base::Value::Dict* value = FindPrefs(kDefaultAccountId);
  ASSERT_TRUE(value);

  const std::string* pref_value = value->FindString(kCustomPrefName);
  ASSERT_TRUE(pref_value);
  EXPECT_EQ(*pref_value, "value");
}

TEST_F(KnownUserTest, FindPrefsIgnoresEphemeralGaiaUsers) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(
      kAccountIdEphemeralGaia,
      /*is_ephemeral=*/true);
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(kAccountIdEphemeralAd,
                                                         /*is_ephemeral=*/true);
  const std::string kCustomPrefName = "custom_pref";
  known_user.SetStringPref(kAccountIdEphemeralGaia, kCustomPrefName, "value");
  known_user.SetStringPref(kAccountIdEphemeralAd, kCustomPrefName, "value");

  EXPECT_FALSE(FindPrefs(kAccountIdEphemeralGaia));

  EXPECT_TRUE(FindPrefs(kAccountIdEphemeralAd));
}

TEST_F(KnownUserTest, FindPrefsMatchForUnknownAccountType) {
  KnownUser known_user(local_state());
  // All account ids have the same e-mail
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_2");
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");

  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");

  EXPECT_TRUE(FindPrefs(kAccountIdUnknown));
  EXPECT_TRUE(FindPrefs(kAccountIdGaia));
  EXPECT_TRUE(FindPrefs(kAccountIdAd));
}

TEST_F(KnownUserTest, FindPrefsMatchForGaiaAccountWithEmail) {
  KnownUser known_user(local_state());
  const char* kEmailA = "a@gmail.com";
  const char* kEmailB = "b@gmail.com";
  const char* kGaiaIdA = "a";
  const char* kGaiaIdB = "b";

  known_user.SaveKnownUser(AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdA));

  // Finding by itself should work
  EXPECT_TRUE(FindPrefs(AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdA)));
  // Finding by gaia id should also work even if the e-mail doesn't match.
  EXPECT_TRUE(FindPrefs(AccountId::FromUserEmailGaiaId(kEmailB, kGaiaIdA)));
  // Finding by e-mail should also work even if the gaia id doesn't match.
  // TODO(https://crbug.com/1190902): This should likely be EXPECT_FALSE going
  // forward.
  EXPECT_TRUE(FindPrefs(AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdB)));

  // An unrelated gaia AccountId with the same Account Type doesn't find
  // anything.
  EXPECT_FALSE(FindPrefs(AccountId::FromUserEmailGaiaId(kEmailB, kGaiaIdB)));

  // Looking up an AccountId stored as gaia by an unknown-type AccountId with
  // the same e-mail address succeeds.
  EXPECT_TRUE(FindPrefs(AccountId::FromUserEmail(kEmailA)));

  // Looking up an AccountId stored as gaia by an AccountId with type Ad fails.
  EXPECT_FALSE(FindPrefs(AccountId::AdFromUserEmailObjGuid(kEmailA, "guid")));
}

TEST_F(KnownUserTest, FindPrefsMatchForAdAccountWithEmail) {
  KnownUser known_user(local_state());
  const std::string kEmailA = "a@gmail.com";
  const std::string kEmailB = "b@gmail.com";

  known_user.SaveKnownUser(AccountId::AdFromUserEmailObjGuid(kEmailA, "a"));

  // Finding by itself should work
  EXPECT_TRUE(FindPrefs(AccountId::AdFromUserEmailObjGuid(kEmailA, "a")));
  // Finding by guid should also work even if the e-mail doesn't match.
  EXPECT_TRUE(FindPrefs(AccountId::AdFromUserEmailObjGuid(kEmailB, "a")));
  // Finding by e-mail should also work even if the guid doesn't match.
  EXPECT_TRUE(FindPrefs(AccountId::AdFromUserEmailObjGuid(kEmailA, "b")));

  // An unrelated AD AccountId with the same Account Type doesn't find
  // anything.
  EXPECT_FALSE(FindPrefs(AccountId::AdFromUserEmailObjGuid(kEmailB, "b")));

  // Looking up an AccountId stored as AD by an unknown-type AccountId with
  // the same e-mail address succeeds.
  EXPECT_TRUE(FindPrefs(AccountId::FromUserEmail(kEmailA)));

  // Looking up an AccountId stored as AD by an AccountId with type gaia fails.
  EXPECT_FALSE(FindPrefs(AccountId::FromUserEmailGaiaId(kEmailA, "gaia_id")));
}

TEST_F(KnownUserTest, UpdatePrefsWithoutClear) {
  KnownUser known_user(local_state());
  constexpr char kPrefName1[] = "pref1";
  constexpr char kPrefName2[] = "pref2";

  known_user.SetPath(kDefaultAccountId, kPrefName1,
                     base::Value("pref1_value1"));

  known_user.SetPath(kDefaultAccountId, kPrefName1,
                     base::Value("pref1_value2"));

  known_user.SetPath(kDefaultAccountId, kPrefName2,
                     base::Value("pref2_value1"));

  EXPECT_EQ(std::make_optional(std::string("pref1_value2")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName1));
  EXPECT_EQ(std::make_optional(std::string("pref2_value1")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName2));
}

TEST_F(KnownUserTest, UpdatePrefsWithClear) {
  KnownUser known_user(local_state());
  constexpr char kPrefName1[] = "pref1";
  constexpr char kPrefName2[] = "pref2";

  known_user.SetPath(kDefaultAccountId, kPrefName1,
                     base::Value("pref1_value1"));

  known_user.SetPath(kDefaultAccountId, kPrefName2,
                     base::Value("pref2_value1"));

  known_user.SetPath(kDefaultAccountId, kPrefName1, std::nullopt);

  EXPECT_EQ(std::nullopt,
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName1));
  EXPECT_EQ(std::make_optional(std::string("pref2_value1")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName2));
}

TEST_F(KnownUserTest, GetKnownAccountIdsNoAccounts) {
  KnownUser known_user(local_state());
  EXPECT_THAT(known_user.GetKnownAccountIds(), testing::IsEmpty());
}

TEST_F(KnownUserTest, GetKnownAccountIdsWithAccounts) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id");
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "obj_guid");

  known_user.SaveKnownUser(kAccountIdGaia);
  known_user.SaveKnownUser(kAccountIdAd);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdGaia, kAccountIdAd));
}

TEST_F(KnownUserTest, SaveKnownUserIgnoresUnknownType) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account2@gmail.com");

  known_user.SaveKnownUser(kAccountIdUnknown);

  EXPECT_THAT(known_user.GetKnownAccountIds(), testing::IsEmpty());
}

TEST_F(KnownUserTest, SaveKnownUserIgnoresEphemeralGaiaUsers) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdNonEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_1");
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdNonEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "guid_3");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");

  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(
      kAccountIdEphemeralGaia,
      /*is_ephemeral=*/true);
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(kAccountIdEphemeralAd,
                                                         /*is_ephemeral=*/true);

  known_user.SaveKnownUser(kAccountIdNonEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdNonEphemeralAd);
  known_user.SaveKnownUser(kAccountIdEphemeralAd);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdNonEphemeralGaia,
                                            kAccountIdNonEphemeralAd,
                                            kAccountIdEphemeralAd));
}

TEST_F(KnownUserTest, UpdateIdForGaiaAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdUnknown));

  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id");
  known_user.UpdateId(kAccountIdGaia);
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdGaia));
}

TEST_F(KnownUserTest, UpdateIdForAdAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdUnknown));

  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");
  known_user.UpdateId(kAccountIdAd);
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdAd));
}

TEST_F(KnownUserTest, FindGaiaIdForGaiaAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id");
  known_user.SaveKnownUser(kAccountIdGaia);

  const std::string* gaia_id = known_user.FindGaiaID(kAccountIdGaia);
  ASSERT_TRUE(gaia_id);
  EXPECT_EQ(*gaia_id, "gaia_id");
}

TEST_F(KnownUserTest, FindGaiaIdForAdAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");
  known_user.SaveKnownUser(kAccountIdAd);

  EXPECT_FALSE(known_user.FindGaiaID(kAccountIdAd));
}

// TODO(crbug.com/40731309): Add tests for GetAccountId.

TEST_F(KnownUserTest, RemovePrefOnCustomPref) {
  KnownUser known_user(local_state());
  const std::string kCustomPrefName = "custom_pref";

  known_user.SetStringPref(kDefaultAccountId, kCustomPrefName, "value");
  EXPECT_TRUE(known_user.FindStringPath(kDefaultAccountId, kCustomPrefName));

  known_user.RemovePref(kDefaultAccountId, kCustomPrefName);
  EXPECT_FALSE(known_user.FindStringPath(kDefaultAccountId, kCustomPrefName));
}

TEST_F(KnownUserTest, RemovePrefOnReservedPref) {
  KnownUser known_user(local_state());
  const std::string kReservedPrefName = "device_id";

  known_user.SetStringPref(kDefaultAccountId, kReservedPrefName, "value");
  // Don't verify the message because on some builds CHECK failures do not print
  // debug messages (https://crbug.com/1198519).
  ASSERT_DEATH(known_user.RemovePref(kDefaultAccountId, kReservedPrefName), "");
}

TEST_F(KnownUserTest, DeviceId) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetDeviceId(kDefaultAccountId), std::string());

  known_user.SetDeviceId(kDefaultAccountId, "test");

  EXPECT_EQ(known_user.GetDeviceId(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, GAPSCookie) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetGAPSCookie(kDefaultAccountId), std::string());

  known_user.SetGAPSCookie(kDefaultAccountId, "test");

  EXPECT_EQ(known_user.GetGAPSCookie(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, UsingSAML) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.IsUsingSAML(kDefaultAccountId));

  known_user.UpdateUsingSAML(kDefaultAccountId, /*using_saml=*/true);
  EXPECT_TRUE(known_user.IsUsingSAML(kDefaultAccountId));
}

TEST_F(KnownUserTest, UsingSAMLPrincipalsAPI) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetIsUsingSAMLPrincipalsAPI(kDefaultAccountId));

  known_user.UpdateIsUsingSAMLPrincipalsAPI(kDefaultAccountId,
                                            /*using_saml=*/true);
  EXPECT_TRUE(known_user.GetIsUsingSAMLPrincipalsAPI(kDefaultAccountId));
}

TEST_F(KnownUserTest, ProfileRequiresPolicy) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kUnknown);

  known_user.SetProfileRequiresPolicy(kDefaultAccountId,
                                      ProfileRequiresPolicy::kPolicyRequired);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kPolicyRequired);

  known_user.SetProfileRequiresPolicy(kDefaultAccountId,
                                      ProfileRequiresPolicy::kNoPolicyRequired);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kNoPolicyRequired);

  known_user.ClearProfileRequiresPolicy(kDefaultAccountId);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kUnknown);
}

TEST_F(KnownUserTest, ReauthReason) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.FindReauthReason(kDefaultAccountId).has_value());

  known_user.UpdateReauthReason(kDefaultAccountId, 3);
  EXPECT_EQ(known_user.FindReauthReason(kDefaultAccountId), 3);
}

TEST_F(KnownUserTest, ChallengeResponseKeys) {
  KnownUser known_user(local_state());
  EXPECT_TRUE(known_user.GetChallengeResponseKeys(kDefaultAccountId).empty());

  base::Value::List challenge_response_keys;
  challenge_response_keys.Append("key1");
  known_user.SetChallengeResponseKeys(kDefaultAccountId,
                                      challenge_response_keys.Clone());

  EXPECT_EQ(known_user.GetChallengeResponseKeys(kDefaultAccountId),
            challenge_response_keys);
}

TEST_F(KnownUserTest, LastOnlineSignin) {
  KnownUser known_user(local_state());
  EXPECT_TRUE(known_user.GetLastOnlineSignin(kDefaultAccountId).is_null());

  base::Time last_online_signin = base::Time::Now();
  known_user.SetLastOnlineSignin(kDefaultAccountId, last_online_signin);

  EXPECT_EQ(known_user.GetLastOnlineSignin(kDefaultAccountId),
            last_online_signin);
}

TEST_F(KnownUserTest, OfflineSigninLimit) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetOfflineSigninLimit(kDefaultAccountId).has_value());

  base::TimeDelta offline_signin_limit = base::Minutes(80);
  known_user.SetOfflineSigninLimit(kDefaultAccountId, offline_signin_limit);

  EXPECT_EQ(known_user.GetOfflineSigninLimit(kDefaultAccountId).value(),
            offline_signin_limit);
}

TEST_F(KnownUserTest, IsEnterpriseManaged) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));

  known_user.SetIsEnterpriseManaged(kDefaultAccountId, true);

  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));
}

TEST_F(KnownUserTest, AccountManager) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetAccountManager(kDefaultAccountId));

  known_user.SetAccountManager(kDefaultAccountId, "test");

  EXPECT_TRUE(known_user.GetAccountManager(kDefaultAccountId));
}

TEST_F(KnownUserTest, UserLastLoginInputMethodId) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetUserLastInputMethodId(kDefaultAccountId));

  known_user.SetUserLastLoginInputMethodId(kDefaultAccountId, "test");

  EXPECT_TRUE(known_user.GetUserLastInputMethodId(kDefaultAccountId));
}

TEST_F(KnownUserTest, UserPinLength) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetUserPinLength(kDefaultAccountId), 0);

  known_user.SetUserPinLength(kDefaultAccountId, 8);

  EXPECT_EQ(known_user.GetUserPinLength(kDefaultAccountId), 8);
}

TEST_F(KnownUserTest, PinAutosubmitBackfillNeeded) {
  KnownUser known_user(local_state());
  // If the pref is not set, returns true.
  EXPECT_TRUE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));

  known_user.PinAutosubmitSetBackfillNotNeeded(kDefaultAccountId);

  EXPECT_FALSE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));

  known_user.PinAutosubmitSetBackfillNeededForTests(kDefaultAccountId);

  EXPECT_TRUE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));
}

TEST_F(KnownUserTest, PasswordSyncToken) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetPasswordSyncToken(kDefaultAccountId));

  known_user.SetPasswordSyncToken(kDefaultAccountId, "test");

  EXPECT_EQ(*known_user.GetPasswordSyncToken(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, CleanEphemeralUsersRemovesEphemeralAdOnly) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdNonEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_1");
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdNonEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "guid_3");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");

  known_user.SaveKnownUser(kAccountIdNonEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdNonEphemeralAd);
  known_user.SaveKnownUser(kAccountIdEphemeralAd);
  known_user.SetIsEphemeralUser(kAccountIdEphemeralGaia,
                                /*is_ephemeral=*/true);
  known_user.SetIsEphemeralUser(kAccountIdEphemeralAd, /*is_ephemeral=*/true);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(
                  kAccountIdNonEphemeralGaia, kAccountIdEphemeralGaia,
                  kAccountIdNonEphemeralAd, kAccountIdEphemeralAd));

  known_user.CleanEphemeralUsers();

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdNonEphemeralGaia,
                                            kAccountIdEphemeralGaia,
                                            kAccountIdNonEphemeralAd));
}

TEST_F(KnownUserTest, CleanObsoletePrefs) {
  KnownUser known_user(local_state());
  const std::string kObsoletePrefName = "minimal_migration_attempted";
  const std::string kCustomPrefName = "custom_pref";

  // Set an obsolete pref.
  known_user.SetBooleanPref(kDefaultAccountId, kObsoletePrefName, true);
  // Set a custom pref.
  known_user.SetBooleanPref(kDefaultAccountId, kCustomPrefName, true);
  // Set a reserved, non-obsolete pref.
  known_user.SetIsEnterpriseManaged(kDefaultAccountId, true);

  known_user.CleanObsoletePrefs();

  // Verify that only the obsolete pref has been removed.
  EXPECT_FALSE(known_user.FindBoolPath(kDefaultAccountId, kObsoletePrefName)
                   .has_value());

  std::optional<bool> custom_pref_value =
      known_user.FindBoolPath(kDefaultAccountId, kCustomPrefName);

  EXPECT_TRUE(custom_pref_value.has_value());
  EXPECT_TRUE(custom_pref_value.value());

  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));
}

//
// =============================================================================
// Type-parametrized unittests for Set{String,Boolean,Integer,Double,}Pref and
// Get{String,Boolean,Integer,Double}Pref.
// For every type (string, boolean, integer, double, raw base::Value) a
// PrefTypeInfo struct is declared which is then referenced in the generic test
// code.

// Test type holder for known_user string prefs.
struct PrefTypeInfoString {
  using PrefType = std::string;
  using PrefTypeForReading = std::string;

  static constexpr auto SetFunc = &KnownUser::SetStringPref;
  static constexpr auto GetFunc = &KnownUser::GetStringPrefForTest;

  static PrefType CreatePrefValue() { return std::string("test"); }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == "test";
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_string() && read_value.GetString() == "test";
  }
};

// Test type holder for known_user integer prefs.
struct PrefTypeInfoInteger {
  using PrefType = int;
  using PrefTypeForReading = int;

  static constexpr auto SetFunc = &KnownUser::SetIntegerPref;
  static constexpr auto GetFunc = &KnownUser::GetIntegerPrefForTest;

  static PrefType CreatePrefValue() { return 7; }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == 7;
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_int() && read_value.GetInt() == 7;
  }
};

// Test type holder for known_user double prefs.
struct PrefTypeInfoDouble {
  using PrefType = double;
  using PrefTypeForReading = double;

  static constexpr auto SetFunc = &KnownUser::SetDoublePref;
  static constexpr auto GetFunc = &KnownUser::GetDoublePrefForTest;

  static PrefType CreatePrefValue() { return 5.25; }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == 5.25;
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_double() && read_value.GetDouble() == 5.25;
  }
};

// Test type holder for known_user boolean prefs.
struct PrefTypeInfoBoolean {
  using PrefType = bool;
  using PrefTypeForReading = bool;

  static constexpr auto SetFunc = &KnownUser::SetBooleanPref;
  static constexpr auto GetFunc = &KnownUser::GetBooleanPrefForTest;

  static PrefType CreatePrefValue() { return true; }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == true;
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_bool() && read_value.GetBool() == true;
  }
};

// Test type holder for known_user base::Value prefs.
struct PrefTypeInfoValue {
  using PrefType = base::Value;
  using PrefTypeForReading = const base::Value*;

  static constexpr auto SetFunc = &KnownUser::SetPath;
  static constexpr auto GetFunc = &KnownUser::GetPrefForTest;

  static PrefType CreatePrefValue() { return base::Value("test"); }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return *read_value == CreatePrefValue();
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value == CreatePrefValue();
  }
};

template <typename PrefTypeInfo>
class KnownUserWithPrefTypeTest : public KnownUserTest {
 public:
  KnownUserWithPrefTypeTest() = default;
  ~KnownUserWithPrefTypeTest() = default;
};

TYPED_TEST_SUITE_P(KnownUserWithPrefTypeTest);

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadOnNonExistingUser) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kNonExistingUser =
      AccountId::FromUserEmail("account1@gmail.com");

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success = (known_user.*TypeParam::GetFunc)(kNonExistingUser,
                                                       kPrefName, &read_result);
  EXPECT_FALSE(read_success);
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadMissingPrefOnExistingUser) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");
  known_user.SaveKnownUser(kUser);

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success =
      (known_user.*TypeParam::GetFunc)(kUser, kPrefName, &read_result);
  EXPECT_FALSE(read_success);
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadExistingPref) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");

  // Set* implicitly creates the known_user user entry.
  (known_user.*TypeParam::SetFunc)(kUser, kPrefName,
                                   TypeParam::CreatePrefValue());

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success =
      (known_user.*TypeParam::GetFunc)(kUser, kPrefName, &read_result);
  EXPECT_TRUE(read_success);
  EXPECT_TRUE(TypeParam::CheckPrefValue(read_result));
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadExistingPrefAsValue) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");

  // Set* implicitly creates the known_user user entry.
  (known_user.*TypeParam::SetFunc)(kUser, kPrefName,
                                   TypeParam::CreatePrefValue());

  const base::Value* read_result;
  bool read_success = known_user.GetPrefForTest(kUser, kPrefName, &read_result);
  EXPECT_TRUE(read_success);
  ASSERT_TRUE(read_result);
  EXPECT_TRUE(TypeParam::CheckPrefValueAsBaseValue(*read_result));
}

REGISTER_TYPED_TEST_SUITE_P(KnownUserWithPrefTypeTest,
                            // All test functions must be listed:
                            ReadOnNonExistingUser,
                            ReadMissingPrefOnExistingUser,
                            ReadExistingPref,
                            ReadExistingPrefAsValue);

// This must be an alias because the preprocessor does not understand <> so if
// it was directly embedded in the INSTANTIATE_TYPED_TEST_SUITE_P macro the
// prepocessor would be confused on the comma.
using AllTypeInfos = testing::Types<PrefTypeInfoString,
                                    PrefTypeInfoInteger,
                                    PrefTypeInfoDouble,
                                    PrefTypeInfoBoolean,
                                    PrefTypeInfoValue>;

INSTANTIATE_TYPED_TEST_SUITE_P(AllTypes,
                               KnownUserWithPrefTypeTest,
                               AllTypeInfos);

}  // namespace user_manager
