// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

using ECPrivateKeyUniquePtr = std::unique_ptr<crypto::ECPrivateKey>;
using EncryptDataVectorUniquePtr = std::unique_ptr<std::vector<EncryptionData>>;
using EntryVectorType =
    leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

const char kFakeAppId[] = "my_app_id";
const char kSecondFakeAppId[] = "my_other_app_id";
const char kFakeAuthorizedEntity[] = "my_sender_id";
const char kSecondFakeAuthorizedEntity[] = "my_other_sender_id";
const char kPrivateEncrypted[] =
    "MIGxMBwGCiqGSIb3DQEMAQMwDgQIh9aZ3UvuDloCAggABIGQZ-T8CJZe-no4mOTDgX1Gm986"
    "Gsbe3mjJeABhA4KOmut_qJh5kt_DLqdNShiQr-afk3AdkX-fxLZdrcHiW9aWvBjnMAY65zg5"
    "oHsuUaoEuG88Ksbku2u193OENWTQTsYaYE2O44qmRfsX773UNVcWXg_omwIbhbgf6tLZUZH_"
    "dTC3YjzuxjbSP89HPEJ-eBXA";
const char kPrivateDecrypted[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgnCScek-QpEjmOOlT-rQ38nZz"
    "vdPlqa00Zy0i6m2OJvahRANCAATaEQ22_OCRpvIOWeQhcbq0qrF1iddSLX1xFmFSxPOWOwmJ"
    "A417CBHOGqsWGkNRvAapFwiegz6Q61rXVo_5roB1";
const char kPublicKey[] =
    "BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6D"
    "PpDrWtdWj_mugHU";

// Number of cryptographically secure random bytes to generate as a key pair's
// authentication secret. Must be at least 16 bytes.
const size_t kAuthSecretBytes = 16;

}  // namespace

class GCMKeyStoreTest : public ::testing::Test {
 public:
  GCMKeyStoreTest() {}
  ~GCMKeyStoreTest() override {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    CreateKeyStore();
  }

  void TearDown() override {
    gcm_key_store_.reset();

    // |gcm_key_store_| owns a ProtoDatabase whose destructor deletes the
    // underlying LevelDB database on the task runner.
    base::RunLoop().RunUntilIdle();
  }

  // Creates the GCM Key Store instance. May be called from within a test's body
  // to re-create the key store, causing the database to re-open.
  void CreateKeyStore() {
    gcm_key_store_.reset(
        new GCMKeyStore(scoped_temp_dir_.GetPath(),
                        task_environment_.GetMainThreadTaskRunner()));
  }

  // Callback to use with GCMKeyStore::{GetKeys, CreateKeys} calls.
  void GotKeys(ECPrivateKeyUniquePtr* key_out,
               std::string* auth_secret_out,
               const base::Closure& quit_closure,
               ECPrivateKeyUniquePtr key,
               const std::string& auth_secret) {
    *key_out = std::move(key);
    *auth_secret_out = auth_secret;
    if (quit_closure)
      quit_closure.Run();
  }

  void AddOldFormatEncryptionDataToKeyStoreDatabase(
      const std::string& app_id,
      const std::string& authorized_entity) {
    EncryptionData encryption_data;
    encryption_data.set_app_id(app_id);
    encryption_data.set_authorized_entity(authorized_entity);

    // Create the authentication secret, which has to be a cryptographically
    // secure random number of at least 128 bits (16 bytes).
    std::string auth_secret;
    crypto::RandBytes(base::WriteInto(&auth_secret, kAuthSecretBytes + 1),
                      kAuthSecretBytes);
    encryption_data.set_auth_secret(auth_secret);

    // Add keys.
    KeyPair* pair = encryption_data.add_keys();
    pair->set_type(KeyPair::ECDH_P256);
    std::string private_key;
    ASSERT_TRUE(base::Base64UrlDecode(
        kPrivateEncrypted, base::Base64UrlDecodePolicy::IGNORE_PADDING,
        &private_key));
    pair->set_private_key(private_key);
    std::string public_key;
    ASSERT_TRUE(base::Base64UrlDecode(
        kPublicKey, base::Base64UrlDecodePolicy::IGNORE_PADDING, &public_key));
    pair->set_public_key(public_key);

    // Add this to database.
    std::unique_ptr<EntryVectorType> entries_to_save =
        std::make_unique<EntryVectorType>();
    std::unique_ptr<std::vector<std::string>> keys_to_remove =
        std::make_unique<std::vector<std::string>>();
    entries_to_save->push_back(std::make_pair(
        encryption_data.app_id() + ',' + encryption_data.authorized_entity(),
        encryption_data));
    base::RunLoop run_loop;
    gcm_key_store_->database_->UpdateEntries(
        std::move(entries_to_save), std::move(keys_to_remove),
        base::BindOnce(&GCMKeyStoreTest::UpdatedEntries, base::Unretained(this),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  GCMKeyStore* gcm_key_store() { return gcm_key_store_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void UpdatedEntries(const base::Closure& quit_closure, bool success) {
    EXPECT_TRUE(success);
    if (quit_closure)
      quit_closure.Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<GCMKeyStore> gcm_key_store_;
};

TEST_F(GCMKeyStoreTest, EmptyByDefault) {
  // The key store is initialized lazily, so this histogram confirms that
  // calling the constructor does not in fact cause initialization.
  histogram_tester()->ExpectTotalCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 0);

  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  base::RunLoop run_loop;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret, run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_FALSE(key);
  EXPECT_EQ(0u, auth_secret.size());

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.GetKeySuccessRate", 0, 1);  // failure
}

TEST_F(GCMKeyStoreTest, CreateAndGetKeys) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  base::RunLoop run_loop;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret, run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_TRUE(key);
  std::string public_key, private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));

  EXPECT_GT(public_key.size(), 0u);
  EXPECT_GT(private_key.size(), 0u);

  ASSERT_GT(auth_secret.size(), 0u);
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.CreateKeySuccessRate", 1, 1);  // success

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  base::RunLoop first_get_run_loop;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret,
                     first_get_run_loop.QuitClosure()));

  first_get_run_loop.Run();

  ASSERT_TRUE(read_key);
  std::string read_public_key, read_private_key;
  ASSERT_TRUE(GetRawPrivateKey(*read_key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*read_key, &read_public_key));
  ASSERT_EQ(read_private_key, private_key);
  ASSERT_EQ(read_public_key, public_key);
  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success

  // GetKey should also succeed if fallback_to_empty_authorized_entity is true
  // (fallback should not occur, since an exact match is found).
  base::RunLoop second_get_run_loop;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      true /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret,
                     second_get_run_loop.QuitClosure()));

  second_get_run_loop.Run();

  ASSERT_TRUE(read_key);

  ASSERT_TRUE(GetRawPrivateKey(*read_key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*read_key, &read_public_key));
  ASSERT_EQ(read_private_key, private_key);
  ASSERT_EQ(read_public_key, public_key);
  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        2);  // another success
}

TEST_F(GCMKeyStoreTest, GetKeysFallback) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(key);

  std::string public_key, private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));

  EXPECT_GT(public_key.size(), 0u);
  EXPECT_GT(private_key.size(), 0u);
  ASSERT_GT(auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.CreateKeySuccessRate", 1,
                                        1);  // success

  // GetKeys should fail when fallback_to_empty_authorized_entity is false, as
  // there is not an exact match for kFakeAuthorizedEntity.
  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_FALSE(read_key);
  EXPECT_EQ(0u, read_auth_secret.size());

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 0,
                                        1);  // failure

  // GetKey should succeed when fallback_to_empty_authorized_entity is true, as
  // falling back to empty authorized entity will match the created key.
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        true /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(read_key);

  std::string read_public_key, read_private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &read_public_key));
  EXPECT_EQ(private_key, read_private_key);
  EXPECT_EQ(public_key, read_public_key);

  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success
}

TEST_F(GCMKeyStoreTest, KeysPersistenceBetweenInstances) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(key);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 1);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 1);  // success

  // Create a new GCM Key Store instance.
  CreateKeyStore();

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(read_key);
  EXPECT_GT(read_auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 2);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 2);  // success
}

TEST_F(GCMKeyStoreTest, CreateAndRemoveKeys) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(read_key);

  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::DoNothing());

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.RemoveKeySuccessRate", 1, 1);  // success

  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_FALSE(read_key);
}

TEST_F(GCMKeyStoreTest, CreateGetAndRemoveKeysSynchronously) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret, base::Closure()));

  // Continue synchronously, without running RunUntilIdle first.
  ECPrivateKeyUniquePtr key_after_create;
  std::string auth_secret_after_create;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_create, &auth_secret_after_create,
                     base::Closure()));

  // Continue synchronously, without running RunUntilIdle first.
  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::DoNothing());

  // Continue synchronously, without running RunUntilIdle first.
  ECPrivateKeyUniquePtr key_after_remove;
  std::string auth_secret_after_remove;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_remove, &auth_secret_after_remove,
                     base::Closure()));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  ECPrivateKeyUniquePtr key_after_idle;
  std::string auth_secret_after_idle;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_idle, &auth_secret_after_idle,
                     base::Closure()));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);
  ASSERT_TRUE(key_after_create);
  EXPECT_FALSE(key_after_remove);
  EXPECT_FALSE(key_after_idle);

  std::string public_key, public_key_after_create;
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key_after_create));
  EXPECT_EQ(public_key, public_key_after_create);

  EXPECT_GT(auth_secret.size(), 0u);
  EXPECT_EQ(auth_secret, auth_secret_after_create);
  EXPECT_EQ("", auth_secret_after_remove);
  EXPECT_EQ("", auth_secret_after_idle);
}

TEST_F(GCMKeyStoreTest, RemoveKeysWildcardAuthorizedEntity) {
  ECPrivateKeyUniquePtr key1, key2, key3;
  std::string auth_secret1, auth_secret2, auth_secret3;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key1,
                     &auth_secret1, base::Closure()));
  gcm_key_store()->CreateKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key2,
                     &auth_secret2, base::Closure()));
  gcm_key_store()->CreateKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key3,
                     &auth_secret3, base::Closure()));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key1);
  ASSERT_TRUE(key2);
  ASSERT_TRUE(key3);

  ECPrivateKeyUniquePtr read_key1, read_key2, read_key3;
  std::string read_auth_secret1, read_auth_secret2, read_auth_secret3;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key1, &read_auth_secret1, base::Closure()));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key2, &read_auth_secret2, base::Closure()));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key3, &read_auth_secret3, base::Closure()));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key1);
  ASSERT_TRUE(read_key2);
  ASSERT_TRUE(read_key3);

  gcm_key_store()->RemoveKeys(kFakeAppId, "*" /* authorized_entity */,
                              base::DoNothing());

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key1, &read_auth_secret1, base::Closure()));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key2, &read_auth_secret2, base::Closure()));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key3, &read_auth_secret3, base::Closure()));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(read_key1);
  EXPECT_FALSE(read_key2);
  ASSERT_TRUE(read_key3);
}

TEST_F(GCMKeyStoreTest, GetKeysMultipleAppIds) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(key);

  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kSecondFakeAppId, kSecondFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &read_key, &read_auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }

  ASSERT_TRUE(read_key);
}

TEST_F(GCMKeyStoreTest, SuccessiveCallsBeforeInitialization) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret, base::Closure()));

  // Deliberately do not run the message loop, so that the callback has not
  // been resolved yet. The following EXPECT() ensures this.
  EXPECT_FALSE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret, base::Closure()));

  EXPECT_FALSE(read_key);

  // Now run the message loop. Both tasks should have finished executing. Due
  // to the asynchronous nature of operations, however, we can't rely on the
  // write to have finished before the read begins.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(key);
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromGCMToInstanceID) {
  ECPrivateKeyUniquePtr key_unused;
  std::string auth_secret_unused;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused,
                       run_loop.QuitClosure()));

    run_loop.Run();
  }

  EXPECT_DCHECK_DEATH({
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused,
                       run_loop.QuitClosure()));

    run_loop.Run();
  });
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromInstanceIDToGCM) {
  ECPrivateKeyUniquePtr key_unused;
  std::string auth_secret_unused;
  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused,
                       run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, kSecondFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused,
                       run_loop.QuitClosure()));

    run_loop.Run();
  }

  EXPECT_DCHECK_DEATH({
    base::RunLoop run_loop;
    gcm_key_store()->CreateKeys(
        kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused,
                       run_loop.QuitClosure()));

    run_loop.Run();
  });
}

TEST_F(GCMKeyStoreTest, TestUpgradePathForKeyStorageDeprecation) {
  // Expect Upgrade count to  be 0.
  histogram_tester()->ExpectTotalCount("GCM.Crypto.GCMDatabaseUpgradeResult",
                                       0);
  // Initialize GCM store and the underlying levelDB database by trying
  // to fetch keys.
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));

    run_loop.Run();
  }
  ASSERT_FALSE(key);
  histogram_tester()->ExpectTotalCount("GCM.Crypto.GCMDatabaseUpgradeResult",
                                       0);

  // Add old format Encryption Data.
  ASSERT_NO_FATAL_FAILURE(AddOldFormatEncryptionDataToKeyStoreDatabase(
      kFakeAppId, kFakeAuthorizedEntity));

  // Create a new GCM Key Store instance, so we can initialize again.
  CreateKeyStore();

  // GetKeys again, verify private key is decrypted and we have upgraded
  // database exactly once
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));
    run_loop.Run();
  }

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GCMDatabaseUpgradeResult",
                                        1, 1);
  ASSERT_TRUE(key);
  ASSERT_GT(auth_secret.size(), 0u);

  // Verify also that the private key is decrypted.
  std::string read_private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &read_private_key));
  std::string decrypted_private_key;
  ASSERT_TRUE(base::Base64UrlDecode(kPrivateDecrypted,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &decrypted_private_key));
  ASSERT_EQ(decrypted_private_key, read_private_key);

  // AddOldFormatEncryptionDataToKeyStoreDatabase() again, different keys
  ASSERT_NO_FATAL_FAILURE(AddOldFormatEncryptionDataToKeyStoreDatabase(
      kSecondFakeAppId, kSecondFakeAuthorizedEntity));

  // GetKeys on this one, should return nullptr
  {
    base::RunLoop run_loop;
    gcm_key_store()->GetKeys(
        kSecondFakeAppId, kSecondFakeAuthorizedEntity,
        false /* fallback_to_empty_authorized_entity */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                       &auth_secret, run_loop.QuitClosure()));
    run_loop.Run();
  }
  ASSERT_FALSE(key);
  ASSERT_EQ(auth_secret.size(), 0u);
  // GCMDatabaseUpgradeResult should not have increased.
  histogram_tester()->ExpectBucketCount("GCM.Crypto.GCMDatabaseUpgradeResult",
                                        1, 1);
}

}  // namespace gcm
