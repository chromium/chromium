// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

// Packs |salt| and |password_length| to a string.
std::string LengthAndSaltToString(const std::string& salt,
                                  size_t password_length) {
  return base::NumberToString(password_length) + "." + salt;
}

std::string EncryptString(const std::string& plain_text) {
  std::string encrypted_text;
  OSCrypt::EncryptString(plain_text, &encrypted_text);
  return base::Base64Encode(encrypted_text);
}

// Saves encrypted PasswordHashData to a preference.
void EncryptAndSave(const PasswordHashData& password_hash_data,
                    PrefService* pref,
                    const std::string& pref_path) {
  std::string encrypted_username = EncryptString(CanonicalizeUsername(
      password_hash_data.username, password_hash_data.is_gaia_password));
  std::string encrypted_hash =
      EncryptString(base::NumberToString(password_hash_data.hash));
  std::string encrypted_length_and_salt = EncryptString(LengthAndSaltToString(
      password_hash_data.salt, password_hash_data.length));
  std::string encrypted_is_gaia_value =
      EncryptString(password_hash_data.is_gaia_password ? "true" : "false");

  base::Value::Dict encrypted_password_hash_entry;
  encrypted_password_hash_entry.Set("username", encrypted_username);
  encrypted_password_hash_entry.Set("hash", encrypted_hash);
  encrypted_password_hash_entry.Set("salt_length", encrypted_length_and_salt);
  encrypted_password_hash_entry.Set("is_gaia", encrypted_is_gaia_value);
  encrypted_password_hash_entry.Set(
      "last_signin", base::Time::Now().InSecondsFSinceUnixEpoch());
  std::unique_ptr<ScopedListPrefUpdate> update =
      std::make_unique<ScopedListPrefUpdate>(pref, pref_path);

  base::Value::List& update_list = update->Get();
  update_list.Append(std::move(encrypted_password_hash_entry));
}

class HashPasswordManagerTest : public testing::Test {
 public:
  HashPasswordManagerTest() {
    prefs_.registry()->RegisterListPref(prefs::kPasswordHashDataList,
                                        PrefRegistry::NO_REGISTRATION_FLAGS);
    local_prefs_.registry()->RegisterListPref(
        prefs::kLocalPasswordHashDataList, PrefRegistry::NO_REGISTRATION_FLAGS);
    // Mock OSCrypt. There is a call to OSCrypt on initializling
    // PasswordReuseDetector, so it should be mocked.
    OSCryptMocker::SetUp();
  }

  ~HashPasswordManagerTest() override { OSCryptMocker::TearDown(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple prefs_;
  TestingPrefServiceSimple local_prefs_;
};

TEST_F(HashPasswordManagerTest, SavingPasswordHashData) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  std::u16string password(u"password");
  std::string username("user@example.com");

  // Verify |SavePasswordHash(const std::string,const std::u16string&)|
  // behavior.
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/true);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));

  // Saves the same password again won't change password hash, length or salt.
  std::optional<PasswordHashData> current_password_hash_data =
      hash_password_manager.RetrievePasswordHash(username,
                                                 /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/true);
  std::optional<PasswordHashData> existing_password_data =
      hash_password_manager.RetrievePasswordHash(username,
                                                 /*is_gaia_password=*/true);
  EXPECT_EQ(current_password_hash_data->hash, existing_password_data->hash);
  EXPECT_EQ(current_password_hash_data->salt, existing_password_data->salt);
  EXPECT_TRUE(current_password_hash_data->is_gaia_password);
  EXPECT_TRUE(existing_password_data->is_gaia_password);

  // Verify |SavePasswordHash(const PasswordHashData&)| behavior.
  std::u16string new_password(u"new_password");
  PasswordHashData new_password_data(username, new_password,
                                     /*force_update=*/true);
  EXPECT_TRUE(hash_password_manager.SavePasswordHash(new_password_data));
  EXPECT_NE(current_password_hash_data->hash,
            hash_password_manager
                .RetrievePasswordHash(username, /*is_gaia_password=*/true)
                ->hash);
}

TEST_F(HashPasswordManagerTest, SavingPasswordHashDataNotCanonicalized) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  std::u16string password(u"password");
  std::string canonical_username("user@gmail.com");
  std::string username("US.ER@gmail.com");
  std::string gmail_prefix("user");

  // Verify |SavePasswordHash(const std::string,const std::u16string&)|
  // behavior.
  hash_password_manager.SavePasswordHash(canonical_username, password,
                                         /*is_gaia_password=*/true);
  ASSERT_TRUE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  EXPECT_EQ(1u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ(
      canonical_username,
      hash_password_manager
          .RetrievePasswordHash(canonical_username, /*is_gaia_password=*/true)
          ->username);

  // Saves the same password with not canonicalized username should not change
  // password hash.
  std::optional<PasswordHashData> current_password_hash_data =
      hash_password_manager.RetrievePasswordHash(username,
                                                 /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/true);
  std::optional<PasswordHashData> existing_password_data =
      hash_password_manager.RetrievePasswordHash(username,
                                                 /*is_gaia_password=*/true);
  EXPECT_EQ(current_password_hash_data->hash, existing_password_data->hash);
  EXPECT_EQ(1u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ(canonical_username,
            hash_password_manager
                .RetrievePasswordHash(username, /*is_gaia_password=*/true)
                ->username);
  hash_password_manager.SavePasswordHash(gmail_prefix, password,
                                         /*is_gaia_password=*/true);
  EXPECT_EQ(current_password_hash_data->hash,
            hash_password_manager
                .RetrievePasswordHash(gmail_prefix,
                                      /*is_gaia_password=*/true)
                ->hash);
  EXPECT_EQ(1u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ(canonical_username,
            hash_password_manager
                .RetrievePasswordHash(gmail_prefix, /*is_gaia_password=*/true)
                ->username);

  // Saves the password with gmail prefix only should be canonicalized into
  // full gmail user name.
  hash_password_manager.SavePasswordHash("user.name", password,
                                         /*is_gaia_password=*/true);
  EXPECT_EQ(2u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ("username@gmail.com",
            hash_password_manager
                .RetrievePasswordHash("user.name", /*is_gaia_password=*/true)
                ->username);
}

TEST_F(HashPasswordManagerTest, SavingGaiaPasswordAndNonGaiaPasswordOld) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  std::u16string password(u"password");
  std::string username("user@example.com");

  // Saves a Gaia password.
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/true);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  EXPECT_EQ(1u, hash_password_manager.RetrieveAllPasswordHashes().size());

  // Saves the same password again but this time it is not a Gaia password.
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/false);
  // Verifies that there should be two separate entry in the saved hash list.
  EXPECT_EQ(2u, hash_password_manager.RetrieveAllPasswordHashes().size());
}

TEST_F(HashPasswordManagerTest, SavingGaiaPasswordAndNonGaiaPassword) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);
  std::u16string password(u"password");
  std::string username("user@example.com");

  // Saves a Gaia password.
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/true);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  EXPECT_EQ(1u, hash_password_manager.RetrieveAllPasswordHashes().size());

  // Saves the same password again but this time it is not a Gaia password.
  hash_password_manager.SavePasswordHash(username, password,
                                         /*is_gaia_password=*/false);
  // Verifies that there should be two separate entry in the saved hash list.
  EXPECT_EQ(2u, hash_password_manager.RetrieveAllPasswordHashes().size());
}

TEST_F(HashPasswordManagerTest, SavingMultipleHashesAndRetrieveAllOld) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  std::u16string password(u"password");

  // Save password hash for 6 different users.
  hash_password_manager.SavePasswordHash("username1", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username2", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username4", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username5", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username6", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", password,
                                         /*is_gaia_password=*/false);

  // Since kMaxPasswordHashDataDictSize is set to 5, we will only save 5
  // password hashes that were most recently signed in.
  EXPECT_EQ(5u, hash_password_manager.RetrieveAllPasswordHashes().size());
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username1", /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username1", /*is_gaia_password=*/false));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username2", /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username2", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username3",
                                                    /*is_gaia_password=*/true));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash(
      "username3", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username4",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username4", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username5",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username5", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username6",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username6", /*is_gaia_password=*/false));
}

TEST_F(HashPasswordManagerTest, SavingMultipleHashesAndRetrieveAll) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);
  std::u16string password(u"password");

  // Save password hash for 6 different users.
  hash_password_manager.SavePasswordHash("username1", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username2", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username4", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username5", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username6", password,
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", password,
                                         /*is_gaia_password=*/false);

  // Since kMaxPasswordHashDataDictSize is set to 5, we will only save 5
  // gaia password hashes that were most recently signed in. We will also
  // save 1 enterprise password hash for a total of 6 saved password hashes.
  EXPECT_EQ(6u, hash_password_manager.RetrieveAllPasswordHashes().size());
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username1", /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username1", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username2",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username2", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username3",
                                                    /*is_gaia_password=*/true));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash(
      "username3", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username4",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username4", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username5",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username5", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("username6",
                                                    /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username6", /*is_gaia_password=*/false));
}

TEST_F(HashPasswordManagerTest, ClearingPasswordHashDataOld) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.SavePasswordHash("username1", u"sync_password",
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username2", u"sync_password",
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", u"enterprise_password",
                                         /*is_gaia_password=*/false);
  hash_password_manager.SavePasswordHash("username4", u"enterprise_password",
                                         /*is_gaia_password=*/false);

  hash_password_manager.ClearSavedPasswordHash("other_username",
                                               /*is_gaia_password=*/true);
  EXPECT_EQ(4u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearSavedPasswordHash("username2",
                                               /*is_gaia_password=*/false);
  EXPECT_EQ(4u, hash_password_manager.RetrieveAllPasswordHashes().size());

  hash_password_manager.ClearSavedPasswordHash("username3",
                                               /*is_gaia_password=*/false);
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username3", /*is_gaia_password=*/false));
  EXPECT_EQ(3u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearAllPasswordHash(/*is_gaia_password=*/true);
  EXPECT_EQ(1u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearAllPasswordHash(/*is_gaia_password=*/false);
  EXPECT_EQ(0u, hash_password_manager.RetrieveAllPasswordHashes().size());
}

TEST_F(HashPasswordManagerTest, ClearingPasswordHashData) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);
  hash_password_manager.SavePasswordHash("username1", u"sync_password",
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username2", u"sync_password",
                                         /*is_gaia_password=*/true);
  hash_password_manager.SavePasswordHash("username3", u"enterprise_password",
                                         /*is_gaia_password=*/false);
  hash_password_manager.SavePasswordHash("username4", u"enterprise_password",
                                         /*is_gaia_password=*/false);

  hash_password_manager.ClearSavedPasswordHash("other_username",
                                               /*is_gaia_password=*/true);
  EXPECT_EQ(4u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearSavedPasswordHash("username2",
                                               /*is_gaia_password=*/false);
  EXPECT_EQ(4u, hash_password_manager.RetrieveAllPasswordHashes().size());

  hash_password_manager.ClearSavedPasswordHash("username3",
                                               /*is_gaia_password=*/false);
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username3", /*is_gaia_password=*/false));
  EXPECT_EQ(3u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearAllPasswordHash(/*is_gaia_password=*/true);
  EXPECT_EQ(1u, hash_password_manager.RetrieveAllPasswordHashes().size());
  hash_password_manager.ClearAllPasswordHash(/*is_gaia_password=*/false);
  EXPECT_EQ(0u, hash_password_manager.RetrieveAllPasswordHashes().size());
}

TEST_F(HashPasswordManagerTest, RetrievingPasswordHashData) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kPasswordHashDataList));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);
  hash_password_manager.SavePasswordHash("username@gmail.com", u"password",
                                         /*is_gaia_password=*/true);
  EXPECT_EQ(1u, hash_password_manager.RetrieveAllPasswordHashes().size());

  std::optional<PasswordHashData> password_hash_data =
      hash_password_manager.RetrievePasswordHash("username@gmail.com",
                                                 /*is_gaia_password=*/false);
  ASSERT_FALSE(password_hash_data);
  password_hash_data = hash_password_manager.RetrievePasswordHash(
      "username@gmail.com", /*is_gaia_password=*/true);
  ASSERT_TRUE(password_hash_data);
  EXPECT_EQ(8u, password_hash_data->length);
  EXPECT_EQ(16u, password_hash_data->salt.size());
  uint64_t expected_hash =
      CalculatePasswordHash(u"password", password_hash_data->salt);
  EXPECT_EQ(expected_hash, password_hash_data->hash);

  // Retrieve not canonicalized version of "username@gmail.com" should return
  // the same result.
  EXPECT_TRUE(
      hash_password_manager.RetrievePasswordHash("user.name@gmail.com",
                                                 /*is_gaia_password=*/true));
  EXPECT_TRUE(
      hash_password_manager.RetrievePasswordHash("USER.NAME@gmail.com",
                                                 /*is_gaia_password=*/true));

  std::optional<PasswordHashData> non_existing_data =
      hash_password_manager.RetrievePasswordHash("non_existing_user", true);
  ASSERT_FALSE(non_existing_data);
}

TEST_F(HashPasswordManagerTest,
       EnterprisePasswordHashesAreNotMigratedToLocalState) {
  scoped_feature_list_.InitAndDisableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);

  std::u16string password(u"password");
  PasswordHashData phd1("user1", password, /*force_update=*/true);
  PasswordHashData phd2("user2", password, /*force_update=*/true,
                        /*is_gaia_password=*/false);
  PasswordHashData phd3("user3", password, /*force_update=*/true);
  PasswordHashData phd4("user4", password, /*force_update=*/true,
                        /*is_gaia_password=*/false);
  EncryptAndSave(phd1, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd2, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd3, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd4, &prefs_, prefs::kPasswordHashDataList);

  // Verify that all password hashes are saved under the profile pref.
  EXPECT_EQ(4u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  // Try migrating enterprise password hashes to the local state pref.
  hash_password_manager.MigrateEnterprisePasswordHashes();
  // Verify that enterprise password hashes have not been moved.
  EXPECT_EQ(4u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ(0u, local_prefs_.GetList(prefs::kLocalPasswordHashDataList).size());
}

TEST_F(HashPasswordManagerTest,
       EnterprisePasswordHashesAreMigratedToLocalState) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);

  std::u16string password(u"password");
  PasswordHashData phd1("user1", password, /*force_update=*/true);
  PasswordHashData phd2("user2", password, /*force_update=*/true,
                        /*is_gaia_password=*/false);
  PasswordHashData phd3("user3", password, /*force_update=*/true);
  PasswordHashData phd4("user4", password, /*force_update=*/true,
                        /*is_gaia_password=*/false);
  EncryptAndSave(phd1, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd2, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd3, &prefs_, prefs::kPasswordHashDataList);
  EncryptAndSave(phd4, &prefs_, prefs::kPasswordHashDataList);

  // Verify that all password hashes are saved under the profile pref.
  EXPECT_EQ(4u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  // Migrate enterprise password hashes to the local state pref.
  hash_password_manager.MigrateEnterprisePasswordHashes();
  // Verify that enterprise password hashes have been moved.
  EXPECT_EQ(2u, prefs_.GetList(prefs::kPasswordHashDataList).size());
  EXPECT_EQ(2u, local_prefs_.GetList(prefs::kLocalPasswordHashDataList).size());
  hash_password_manager.ClearAllPasswordHash(/*is_gaia_password=*/false);
  EXPECT_EQ(0u, local_prefs_.GetList(prefs::kLocalPasswordHashDataList).size());
}

TEST_F(HashPasswordManagerTest, QueryingDefaultEmptyPrefListDoesNotCrash) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLocalStateEnterprisePasswordHashes);
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.set_local_prefs(&local_prefs_);
  std::string username("user@example.com");
  EXPECT_EQ(0u, hash_password_manager.RetrieveAllPasswordHashes().size());
  EXPECT_TRUE(std::nullopt == hash_password_manager.RetrievePasswordHash(
                                  username, /*is_gaia_password=*/true));
  EXPECT_TRUE(std::nullopt == hash_password_manager.RetrievePasswordHash(
                                  username, /*is_gaia_password=*/false));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      username, /*is_gaia_password=*/true));
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      username, /*is_gaia_password=*/false));
}

}  // namespace
}  // namespace password_manager
