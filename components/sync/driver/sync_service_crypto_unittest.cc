// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_crypto.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/engine/mock_sync_engine.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;

sync_pb::EncryptedData MakeEncryptedData(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params, passphrase);

  std::string nigori_name;
  EXPECT_TRUE(
      nigori->Permute(Nigori::Type::Password, kNigoriKeyName, &nigori_name));

  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori_name);
  EXPECT_TRUE(nigori->Encrypt(unencrypted, encrypted.mutable_blob()));
  return encrypted;
}

CoreAccountInfo MakeAccountInfoWithGaia(const std::string& gaia) {
  CoreAccountInfo result;
  result.gaia = gaia;
  return result;
}

class MockCryptoSyncPrefs : public CryptoSyncPrefs {
 public:
  MockCryptoSyncPrefs() = default;
  ~MockCryptoSyncPrefs() override = default;

  MOCK_CONST_METHOD0(GetEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetEncryptionBootstrapToken, void(const std::string&));
  MOCK_CONST_METHOD0(GetKeystoreEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetKeystoreEncryptionBootstrapToken, void(const std::string&));
};

class MockTrustedVaultClient : public TrustedVaultClient {
 public:
  MockTrustedVaultClient() = default;
  ~MockTrustedVaultClient() override = default;

  MOCK_METHOD2(
      FetchKeys,
      void(const std::string& gaia_id,
           base::OnceCallback<void(const std::vector<std::string>&)> cb));
  MOCK_METHOD2(StoreKeys,
               void(const std::string& gaia_id,
                    const std::vector<std::string>& keys));
};

class SyncServiceCryptoTest : public testing::Test {
 protected:
  SyncServiceCryptoTest()
      : crypto_(notify_observers_cb_.Get(),
                reconfigure_cb_.Get(),
                &prefs_,
                &trusted_vault_client_) {}

  ~SyncServiceCryptoTest() override = default;

  bool VerifyAndClearExpectations() {
    return testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&trusted_vault_client_) &&
           testing::Mock::VerifyAndClearExpectations(&engine_);
  }

  testing::NiceMock<base::MockCallback<base::RepeatingClosure>>
      notify_observers_cb_;
  testing::NiceMock<
      base::MockCallback<base::RepeatingCallback<void(ConfigureReason)>>>
      reconfigure_cb_;
  testing::NiceMock<MockCryptoSyncPrefs> prefs_;
  testing::NiceMock<MockTrustedVaultClient> trusted_vault_client_;
  testing::NiceMock<MockSyncEngine> engine_;
  SyncServiceCrypto crypto_;
};

TEST_F(SyncServiceCryptoTest, ShouldExposePassphraseRequired) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnPassphraseRequired(
      REASON_DECRYPTION, KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the wrong passphrase should be rejected.
  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  EXPECT_CALL(engine_, SetDecryptionPassphrase(_)).Times(0);
  EXPECT_FALSE(crypto_.SetDecryptionPassphrase("wrongpassphrase"));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetDecryptionPassphrase(kTestPassphrase))
      .WillOnce([&](const std::string&) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO)).Times(2);
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldStoreTrustedVaultKeysBeforeEngineInitialization) {
  const std::string kAccount = "account1";
  const std::vector<std::string> kKeys = {"key1"};
  EXPECT_CALL(trusted_vault_client_, StoreKeys("account1", kKeys));
  crypto_.AddTrustedVaultDecryptionKeys(kAccount, kKeys);
}

TEST_F(SyncServiceCryptoTest,
       ShouldStoreTrustedVaultKeysAfterEngineInitialization) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const CoreAccountInfo kOtherAccount = MakeAccountInfoWithGaia("otheraccount");
  const std::vector<std::string> kSyncingAccountKeys = {"key1"};
  const std::vector<std::string> kOtherAccountKeys = {"key2"};

  crypto_.SetSyncEngine(kSyncingAccount, &engine_);

  EXPECT_CALL(trusted_vault_client_,
              StoreKeys(kOtherAccount.gaia, kOtherAccountKeys));
  EXPECT_CALL(trusted_vault_client_,
              StoreKeys(kSyncingAccount.gaia, kSyncingAccountKeys));

  // Only the sync-ing account should be propagated to the engine.
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kOtherAccountKeys, _))
      .Times(0);
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kSyncingAccountKeys, _));
  crypto_.AddTrustedVaultDecryptionKeys(kOtherAccount.gaia, kOtherAccountKeys);
  crypto_.AddTrustedVaultDecryptionKeys(kSyncingAccount.gaia,
                                        kSyncingAccountKeys);
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientBeforeInitialization) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // OnTrustedVaultKeyRequired() called during initialization of the sync
  // engine (i.e. before SetSyncEngine()).
  EXPECT_CALL(trusted_vault_client_, FetchKeys(_, _)).Times(0);
  crypto_.OnTrustedVaultKeyRequired();

  base::OnceCallback<void(const std::vector<std::string>&)> fetch_keys_cb;
  EXPECT_CALL(trusted_vault_client_, FetchKeys(kSyncingAccount.gaia, _))
      .WillOnce(
          [&](const std::string& gaia_id,
              base::OnceCallback<void(const std::vector<std::string>&)> cb) {
            fetch_keys_cb = std::move(cb);
          });

  // Trusted vault keys should be fetched only after the engine initialization
  // is completed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  VerifyAndClearExpectations();

  // While there is an ongoing fetch, there should be no user action required.
  ASSERT_TRUE(fetch_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the fetch.
  std::move(fetch_keys_cb).Run(kFetchedKeys);
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientAfterInitialization) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceCallback<void(const std::vector<std::string>&)> fetch_keys_cb;
  EXPECT_CALL(trusted_vault_client_, FetchKeys(kSyncingAccount.gaia, _))
      .WillOnce(
          [&](const std::string& gaia_id,
              base::OnceCallback<void(const std::vector<std::string>&)> cb) {
            fetch_keys_cb = std::move(cb);
          });

  // Mimic the engine determining that trusted vault keys are required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  VerifyAndClearExpectations();

  // While there is an ongoing fetch, there should be no user action required.
  ASSERT_TRUE(fetch_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the fetch.
  std::move(fetch_keys_cb).Run(kFetchedKeys);
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldReadInvalidTrustedVaultKeysFromClient) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceCallback<void(const std::vector<std::string>&)> fetch_keys_cb;
  EXPECT_CALL(trusted_vault_client_, FetchKeys(kSyncingAccount.gaia, _))
      .WillOnce(
          [&](const std::string& gaia_id,
              base::OnceCallback<void(const std::vector<std::string>&)> cb) {
            fetch_keys_cb = std::move(cb);
          });

  // Mimic the engine determining that trusted vault keys are required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  VerifyAndClearExpectations();

  // While there is an ongoing fetch, there should be no user action required.
  ASSERT_TRUE(fetch_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the client.
  std::move(fetch_keys_cb).Run(kFetchedKeys);
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted().
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  std::move(add_keys_cb).Run();
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
}

}  // namespace

}  // namespace syncer
