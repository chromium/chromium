// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_service_crypto.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/test/mock_sync_engine.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Ne;
using testing::Not;
using testing::NotNull;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;

sync_pb::EncryptedData MakeEncryptedData(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params, passphrase);

  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori->GetKeyName());
  encrypted.set_blob(nigori->Encrypt(unencrypted));
  return encrypted;
}

CoreAccountInfo MakeAccountInfoWithGaia(const std::string& gaia) {
  CoreAccountInfo result;
  result.gaia = gaia;
  return result;
}

std::string CreateBootstrapToken(const std::string& passphrase,
                                 const KeyDerivationParams& derivation_params) {
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params, passphrase);

  sync_pb::NigoriKey proto;
  nigori->ExportKeys(proto.mutable_deprecated_user_key(),
                     proto.mutable_encryption_key(), proto.mutable_mac_key());

  const std::string serialized_key = proto.SerializeAsString();
  EXPECT_FALSE(serialized_key.empty());

  std::string encrypted_key;
  EXPECT_TRUE(OSCrypt::EncryptString(serialized_key, &encrypted_key));

  return base::Base64Encode(encrypted_key);
}

MATCHER(IsScryptKeyDerivationParams, "") {
  const KeyDerivationParams& params = arg;
  return params.method() == KeyDerivationMethod::SCRYPT_8192_8_11 &&
         !params.scrypt_salt().empty();
}

MATCHER_P2(BootstrapTokenDerivedFrom,
           expected_passphrase,
           expected_derivation_params,
           "") {
  const std::string& given_bootstrap_token = arg;
  std::string decoded_key;
  if (!base::Base64Decode(given_bootstrap_token, &decoded_key)) {
    return false;
  }

  std::string decrypted_key;
  if (!OSCrypt::DecryptString(decoded_key, &decrypted_key)) {
    return false;
  }

  sync_pb::NigoriKey given_key;
  if (!given_key.ParseFromString(decrypted_key)) {
    return false;
  }

  std::unique_ptr<Nigori> expected_nigori = Nigori::CreateByDerivation(
      expected_derivation_params, expected_passphrase);
  sync_pb::NigoriKey expected_key;
  expected_nigori->ExportKeys(expected_key.mutable_deprecated_user_key(),
                              expected_key.mutable_encryption_key(),
                              expected_key.mutable_mac_key());
  return given_key.encryption_key() == expected_key.encryption_key() &&
         given_key.mac_key() == expected_key.mac_key();
}

class MockDelegate : public SyncServiceCrypto::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void, CryptoStateChanged, (), (override));
  MOCK_METHOD(void, CryptoRequiredUserActionChanged, (), (override));
  MOCK_METHOD(void, ReconfigureDataTypesDueToCrypto, (), (override));
  MOCK_METHOD(void, PassphraseTypeChanged, (PassphraseType), (override));
  MOCK_METHOD(std::optional<PassphraseType>,
              GetPassphraseType,
              (),
              (const override));
  MOCK_METHOD(void,
              SetEncryptionBootstrapToken,
              (const std::string&),
              (override));
  MOCK_METHOD(std::string, GetEncryptionBootstrapToken, (), (const override));
};

class SyncServiceCryptoTest : public testing::Test {
 protected:
  // Account used in most tests.
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");

  // Initial trusted vault keys stored on the server for |kSyncingAccount|.
  const std::vector<std::vector<uint8_t>> kInitialTrustedVaultKeys = {
      {0, 1, 2, 3, 4}};

  SyncServiceCryptoTest() : crypto_(&delegate_, &trusted_vault_client_) {
    trusted_vault_client_.server()->StoreKeysOnServer(kSyncingAccount.gaia,
                                                      kInitialTrustedVaultKeys);

    ON_CALL(delegate_, GetPassphraseType())
        .WillByDefault(ReturnPointee(&passphrase_type_));
    ON_CALL(delegate_, PassphraseTypeChanged(_))
        .WillByDefault(SaveArg<0>(&passphrase_type_));
  }

  ~SyncServiceCryptoTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  bool VerifyAndClearExpectations() {
    return testing::Mock::VerifyAndClearExpectations(&delegate_) &&
           testing::Mock::VerifyAndClearExpectations(&trusted_vault_client_) &&
           testing::Mock::VerifyAndClearExpectations(&engine_);
  }

  void MimicKeyRetrievalByUser() {
    trusted_vault_client_.server()->MimicKeyRetrievalByUser(
        kSyncingAccount.gaia, &trusted_vault_client_);
  }

  std::optional<PassphraseType> passphrase_type_;

  testing::NiceMock<MockDelegate> delegate_;
  trusted_vault::FakeTrustedVaultClient trusted_vault_client_;
  testing::NiceMock<MockSyncEngine> engine_;
  SyncServiceCrypto crypto_;
};

// Happy case where no user action is required upon startup.
TEST_F(SyncServiceCryptoTest, ShouldRequireNoUserAction) {
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  EXPECT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());
}

TEST_F(SyncServiceCryptoTest, ShouldSetUpNewCustomPassphrase) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());
  ASSERT_FALSE(crypto_.IsEncryptEverythingEnabled());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Ne(PassphraseType::kCustomPassphrase));

  EXPECT_CALL(delegate_, SetEncryptionBootstrapToken(Not(IsEmpty())));
  EXPECT_CALL(engine_, SetEncryptionPassphrase(kTestPassphrase,
                                               IsScryptKeyDerivationParams()));
  crypto_.SetEncryptionPassphrase(kTestPassphrase);

  // Mimic completion of the procedure in the sync engine.
  EXPECT_CALL(delegate_, CryptoStateChanged());
  crypto_.OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                  base::Time::Now());
  // The current implementation notifies observers again upon
  // crypto_.OnEncryptedTypesChanged(). This may change in the future.
  EXPECT_CALL(delegate_, CryptoStateChanged());
  crypto_.OnEncryptedTypesChanged(syncer::EncryptableUserTypes(),
                                  /*encrypt_everything=*/true);
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnPassphraseAccepted();

  EXPECT_FALSE(crypto_.IsPassphraseRequired());
  EXPECT_TRUE(crypto_.IsEncryptEverythingEnabled());
  EXPECT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kCustomPassphrase));
}

TEST_F(SyncServiceCryptoTest, ShouldExposePassphraseRequired) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the wrong passphrase should be rejected.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey).Times(0);
  EXPECT_FALSE(crypto_.SetDecryptionPassphrase("wrongpassphrase"));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  EXPECT_CALL(delegate_,
              SetEncryptionBootstrapToken(BootstrapTokenDerivedFrom(
                  kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

// Regression test for crbug.com/1306831.
TEST_F(SyncServiceCryptoTest,
       ShouldStoreBootstrapTokenBeforeReconfiguringDataTypes) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  ASSERT_TRUE(crypto_.IsPassphraseRequired());

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });

  // Order of SetEncryptionBootstrapToken() and
  // ReconfigureDataTypesDueToCrypto() (assuming passphrase is not required upon
  // reconfiguration) is important as clients rely on this to detect whether
  // GetExplicitPassphraseDecryptionNigoriKey() can be called.
  testing::InSequence seq;
  EXPECT_CALL(delegate_,
              SetEncryptionBootstrapToken(BootstrapTokenDerivedFrom(
                  kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  ASSERT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
}

TEST_F(SyncServiceCryptoTest, ShouldSetupDecryptionWithBootstrapToken) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic passphrase stored in bootstrap token.
  ON_CALL(delegate_, GetEncryptionBootstrapToken())
      .WillByDefault(Return(CreateBootstrapToken(
          kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));

  // Expect setting decryption key without waiting till user enters the
  // passphrase.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });

  // Mimic the engine determining that a passphrase is required.
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  // The passphrase-required state should have been automatically resolved via
  // the bootstrap token.
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldSetupDecryptionWithBootstrapTokenUponEngineInitialization) {
  const std::string kTestPassphrase = "somepassphrase";

  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic passphrase stored in bootstrap token.
  ON_CALL(delegate_, GetEncryptionBootstrapToken())
      .WillByDefault(Return(CreateBootstrapToken(
          kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));

  // Mimic the engine determining that a passphrase is required. Note that
  // |crypto_| isn't yet aware of engine initialization - this is a legitimate
  // scenario.
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Expect setting decryption key without waiting till user enters the
  // passphrase.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });

  // Mimic completion of engine initialization, now decryption key from
  // bootstrap token should be populated to the engine.
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldIgnoreNotMatchingBootstrapToken) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic wrong passphrase stored in bootstrap token.
  ON_CALL(delegate_, GetEncryptionBootstrapToken())
      .WillByDefault(Return(CreateBootstrapToken(
          "wrongpassphrase", KeyDerivationParams::CreateForPbkdf2())));

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  // There should be no attempt to populate wrong key to the |engine_|.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey).Times(0);
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldIgnoreCorruptedBootstrapToken) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic storing corrupted bootstrap token.
  ON_CALL(delegate_, GetEncryptionBootstrapToken())
      .WillByDefault(Return("corrupted_token"));

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  // There should be no attempt to populate wrong key to the |engine_|.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey).Times(0);
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldDecryptWithNigoriKey) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Passing wrong decryption key should be ignored.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey).Times(0);
  crypto_.SetExplicitPassphraseDecryptionNigoriKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "wrongpassphrase"));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Passing correct decryption key should be accepted.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  EXPECT_CALL(delegate_,
              SetEncryptionBootstrapToken(BootstrapTokenDerivedFrom(
                  kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));
  crypto_.SetExplicitPassphraseDecryptionNigoriKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldIgnoreDecryptionWithNigoriKeyWhenPassphraseNotRequired) {
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey).Times(0);
  EXPECT_CALL(delegate_, SetEncryptionBootstrapToken).Times(0);
  crypto_.SetExplicitPassphraseDecryptionNigoriKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "unexpected_passphrase"));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

// Regression test for crbug.com/1322687: engine initialization may happen after
// SetExplicitPassphraseDecryptionNigoriKey() call, verify it doesn't crash and
// that decryption key populated to the engine later, upon initialization.
TEST_F(SyncServiceCryptoTest,
       ShouldDeferDecryptionWithNigoriKeyUntilEngineInitialization) {
  const std::string kTestPassphrase = "somepassphrase";

  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnPassphraseRequired(
      KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  ASSERT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Pass decryption nigori key, it should be stored in the bootstrap token, but
  // shouldn't cause other changes, since engine isn't initialized.
  std::string bootstrap_token;
  ON_CALL(delegate_, SetEncryptionBootstrapToken(_))
      .WillByDefault(SaveArg<0>(&bootstrap_token));
  ON_CALL(delegate_, GetEncryptionBootstrapToken())
      .WillByDefault([&bootstrap_token]() { return bootstrap_token; });
  crypto_.SetExplicitPassphraseDecryptionNigoriKey(Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), kTestPassphrase));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Decryption key should be passed to the engine once it's initialized.
  EXPECT_CALL(engine_, SetExplicitPassphraseDecryptionKey(NotNull()))
      .WillOnce(
          [&](std::unique_ptr<Nigori>) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphrase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(2);
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldGetDecryptionKeyFromBootstrapToken) {
  const std::string kTestPassphrase = "somepassphrase";

  // Mimic passphrase being stored in bootstrap token.
  ON_CALL(delegate_, GetEncryptionBootstrapToken)
      .WillByDefault(Return(CreateBootstrapToken(
          kTestPassphrase, KeyDerivationParams::CreateForPbkdf2())));

  std::unique_ptr<Nigori> expected_nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), kTestPassphrase);
  ASSERT_THAT(expected_nigori, NotNull());
  std::string deprecated_user_key;
  std::string expected_encryption_key;
  std::string expected_mac_key;
  expected_nigori->ExportKeys(&deprecated_user_key, &expected_encryption_key,
                              &expected_mac_key);

  // Verify that GetExplicitPassphraseDecryptionNigoriKey() result equals to
  // |expected_nigori|.
  std::unique_ptr<Nigori> stored_nigori =
      crypto_.GetExplicitPassphraseDecryptionNigoriKey();
  ASSERT_THAT(stored_nigori, NotNull());
  std::string stored_encryption_key;
  std::string stored_mac_key;
  stored_nigori->ExportKeys(&deprecated_user_key, &stored_encryption_key,
                            &stored_mac_key);
  EXPECT_THAT(stored_encryption_key, Eq(expected_encryption_key));
  EXPECT_THAT(stored_mac_key, Eq(expected_mac_key));
}

TEST_F(SyncServiceCryptoTest,
       ShouldGetNullDecryptionKeyFromEmptyBootstrapToken) {
  // GetEncryptionBootstrapToken() returns empty string by default.
  EXPECT_THAT(crypto_.GetExplicitPassphraseDecryptionNigoriKey(), IsNull());
}

TEST_F(SyncServiceCryptoTest,
       ShouldGetNullDecryptionKeyFromCorruptedBootstrapToken) {
  // Mimic corrupted bootstrap token being stored.
  ON_CALL(delegate_, GetEncryptionBootstrapToken)
      .WillByDefault(Return("corrupted_token"));
  EXPECT_THAT(crypto_.GetExplicitPassphraseDecryptionNigoriKey(), IsNull());
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientBeforeInitialization) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization.
  MimicKeyRetrievalByUser();

  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // OnTrustedVaultKeyRequired() called during initialization of the sync
  // engine (i.e. before SetSyncEngine()).
  crypto_.OnTrustedVaultKeyRequired();

  // Trusted vault keys should be fetched only after the engine initialization
  // is completed.
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _))
      .WillOnce(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientAfterInitialization) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization.
  MimicKeyRetrievalByUser();

  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _))
      .WillOnce(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadNoTrustedVaultKeysFromClientAfterInitialization) {
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto()).Times(0);
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys).Times(0);

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  ASSERT_THAT(trusted_vault_client_.server_request_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the fetch, which should lead to a reconfiguration.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest, ShouldReadInvalidTrustedVaultKeysFromClient) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required).
  MimicKeyRetrievalByUser();

  base::OnceClosure add_keys_cb;
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  ASSERT_THAT(trusted_vault_client_.server_request_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the client.
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _));
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted().
  std::move(add_keys_cb).Run();

  // The keys should be marked as stale, and a second fetch attempt started.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // Mimic completion of the client for the second pass.
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _));
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(add_keys_cb);

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted(), for
  // the second pass.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  std::move(add_keys_cb).Run();

  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
}

// Similar to ShouldReadInvalidTrustedVaultKeysFromClient but in this case the
// client is able to follow a key rotation as part of the second fetch attempt.
TEST_F(SyncServiceCryptoTest, ShouldFollowKeyRotationDueToSecondFetch) {
  const std::vector<std::vector<uint8_t>> kRotatedKeys = {
      kInitialTrustedVaultKeys[0], {2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kRotatedKeys| are provided).
  MimicKeyRetrievalByUser();

  // Mimic server-side key rotation which the keys, in a way that the rotated
  // keys are a continuation of kInitialTrustedVaultKeys, such that
  // FakeTrustedVaultClient::server() will allow the client to silently follow
  // key rotation.
  trusted_vault_client_.server()->StoreKeysOnServer(kSyncingAccount.gaia,
                                                    kRotatedKeys);

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kRotatedKeys|
  // are provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kRotatedKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are fetched as part of the first fetch.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch (first attempt), there should be no user
  // action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // The keys fetched in the first attempt (|kInitialTrustedVaultKeys|) are
  // insufficient and should be marked as stale. In addition, a second fetch
  // should be triggered.
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // While there is an ongoing fetch (second attempt), there should be no user
  // action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Because of |kRotatedKeys| is a continuation of |kInitialTrustedVaultKeys|,
  // TrustedVaultServer should successfully deliver the new keys |kRotatedKeys|
  // to the client.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
}

// Similar to ShouldReadInvalidTrustedVaultKeysFromClient: the vault
// initially has no valid keys, leading to IsTrustedVaultKeyRequired().
// Later, the vault gets populated with the keys, which should trigger
// a fetch and eventually resolve the encryption issue.
TEST_F(SyncServiceCryptoTest, ShouldRefetchTrustedVaultKeysWhenChangeObserved) {
  const std::vector<std::vector<uint8_t>> kNewKeys = {{2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kNewKeys| are provided).
  MimicKeyRetrievalByUser();

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kNewKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are fetched, which are insufficient, and hence
  // IsTrustedVaultKeyRequired() is exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  // Note that this initial attempt involves two fetches, where both return
  // |kInitialTrustedVaultKeys|.
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic server-side key reset and a new retrieval.
  trusted_vault_client_.server()->StoreKeysOnServer(kSyncingAccount.gaia,
                                                    kNewKeys);
  MimicKeyRetrievalByUser();

  // Key retrieval should have initiated a third fetch.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
}

// Same as above but the new keys become available during an ongoing FetchKeys()
// request.
TEST_F(SyncServiceCryptoTest,
       ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingFetch) {
  const std::vector<std::vector<uint8_t>> kNewKeys = {{2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kNewKeys| are provided).
  MimicKeyRetrievalByUser();

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kNewKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are in the process of being fetched.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // While there is an ongoing fetch, mimic server-side key reset and a new
  // retrieval.
  trusted_vault_client_.server()->StoreKeysOnServer(kSyncingAccount.gaia,
                                                    kNewKeys);
  MimicKeyRetrievalByUser();

  // Because there's already an ongoing fetch, a second one should not have been
  // triggered yet and should be deferred instead.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // As soon as the first fetch completes, the second one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // The completion of the second fetch should resolve the encryption issue.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

// The engine gets initialized and the vault initially has insufficient keys,
// leading to IsTrustedVaultKeyRequired(). Later, keys are added to the vault
// *twice*, where the later event should be handled as a deferred fetch.
TEST_F(
    SyncServiceCryptoTest,
    ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingRefetch) {
  const std::vector<std::vector<uint8_t>> kLatestKeys = {{2, 2, 2, 2, 2}};

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kLatestKeys|
  // are provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kLatestKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // no keys are fetched from the client, hence IsTrustedVaultKeyRequired() is
  // exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic retrieval of keys, leading to a second fetch that returns
  // |kInitialTrustedVaultKeys|, which are insufficient and should be marked as
  // stale as soon as the fetch completes (later below).
  MimicKeyRetrievalByUser();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // While the second fetch is ongoing, mimic additional keys being retrieved.
  // Because there's already an ongoing fetch, a third one should not have been
  // triggered yet and should be deferred instead.
  trusted_vault_client_.server()->StoreKeysOnServer(kSyncingAccount.gaia,
                                                    kLatestKeys);
  MimicKeyRetrievalByUser();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // As soon as the second fetch completes, the keys should be marked as stale
  // and a third fetch attempt triggered.
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));

  // As soon as the third fetch completes, the fourth one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
}

TEST_F(SyncServiceCryptoTest,
       ShouldNotGetRecoverabilityIfKeystorePassphraseUsed) {
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);
  crypto_.OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kKeystorePassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  EXPECT_THAT(trusted_vault_client_.get_is_recoverablity_degraded_call_count(),
              Eq(0));
  EXPECT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());
}

TEST_F(SyncServiceCryptoTest,
       ShouldNotReportDegradedRecoverabilityUponInitialization) {
  const SyncStatus kEmptySyncStatus;
  ON_CALL(engine_, GetDetailedStatus())
      .WillByDefault(ReturnRef(kEmptySyncStatus));

  base::HistogramTester histogram_tester;
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  EXPECT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceCryptoTest,
       ShouldReportDegradedRecoverabilityUponInitialization) {
  // Use a very recent migration time to verify all histograms.
  SyncStatus sync_status;
  sync_status.trusted_vault_debug_info.set_migration_time(
      TimeToProtoTime(base::Time::Now() - base::Hours(1)));
  ON_CALL(engine_, GetDetailedStatus()).WillByDefault(ReturnRef(sync_status));

  base::HistogramTester histogram_tester;
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  EXPECT_TRUE(crypto_.IsTrustedVaultRecoverabilityDegraded());
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast28Days",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast7Days",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast3Days",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLastDay",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceCryptoTest, ShouldReportDegradedRecoverabilityUponChange) {
  const SyncStatus kEmptySyncStatus;
  ON_CALL(engine_, GetDetailedStatus())
      .WillByDefault(ReturnRef(kEmptySyncStatus));

  base::HistogramTester histogram_tester;
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // Changing the state notifies observers and should lead to a change in
  // IsTrustedVaultRecoverabilityDegraded().
  EXPECT_CALL(delegate_, CryptoStateChanged());
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_TRUE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // For UMA purposes, only the initial value counts (false).
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceCryptoTest,
       ShouldStopReportingDegradedRecoverabilityUponChange) {
  const SyncStatus kEmptySyncStatus;
  ON_CALL(engine_, GetDetailedStatus())
      .WillByDefault(ReturnRef(kEmptySyncStatus));

  base::HistogramTester histogram_tester;
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_TRUE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // Changing the state notifies observers and should lead to a change in
  // IsTrustedVaultRecoverabilityDegraded().
  EXPECT_CALL(delegate_, CryptoStateChanged());
  trusted_vault_client_.SetIsRecoveryMethodRequired(false);
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // For UMA purposes, only the initial value counts (true).
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceCryptoTest, ShouldReportDegradedRecoverabilityUponRetrieval) {
  const SyncStatus kEmptySyncStatus;
  ON_CALL(engine_, GetDetailedStatus())
      .WillByDefault(ReturnRef(kEmptySyncStatus));

  base::HistogramTester histogram_tester;
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);

  // Mimic startup with trusted vault keys being required.
  crypto_.OnTrustedVaultKeyRequired();
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // Complete the fetching of initial keys (no keys) from the client.
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // Mimic a successful key retrieval.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys)
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        crypto_.OnTrustedVaultKeyAccepted();
        std::move(done_cb).Run();
      });
  MimicKeyRetrievalByUser();
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Complete degraded recoverability refresh, that should be triggered upon
  // successful key retrieval.
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());

  // The recoverability state should be exposed.
  EXPECT_TRUE(crypto_.IsTrustedVaultRecoverabilityDegraded());
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceCryptoTest,
       ShouldClearDegradedRecoverabilityIfCustomPassphraseIsSet) {
  const SyncStatus kEmptySyncStatus;
  ON_CALL(engine_, GetDetailedStatus())
      .WillByDefault(ReturnRef(kEmptySyncStatus));

  const std::string kTestPassphrase = "somepassphrase";

  // Mimic a browser startup in |kTrustedVaultPassphrase| with no additional
  // keys required and degraded recoverability state.
  trusted_vault_client_.SetIsRecoveryMethodRequired(true);
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_FALSE(crypto_.IsPassphraseRequired());
  ASSERT_TRUE(crypto_.IsTrustedVaultRecoverabilityDegraded());

  // Mimic the user setting up a new custom passphrase.
  crypto_.SetEncryptionPassphrase(kTestPassphrase);

  // Mimic completion of the procedure in the sync engine.
  EXPECT_CALL(delegate_, ReconfigureDataTypesDueToCrypto());
  EXPECT_CALL(delegate_, CryptoStateChanged());
  crypto_.OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                  base::Time::Now());
  crypto_.OnPassphraseAccepted();

  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kCustomPassphrase));

  // Recoverability should no longer be considered degraded.
  EXPECT_FALSE(crypto_.IsTrustedVaultRecoverabilityDegraded());
}

// Regression test for crbug.com/1475589.
TEST_F(SyncServiceCryptoTest,
       ShouldIgnoreDegradedRecoverabilityRequestCompletionAfterReset) {
  crypto_.OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                  base::Time::Now());
  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequiredStateKnown());
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_FALSE(crypto_.IsPassphraseRequired());

  // Reset all in-memory |crypto_| state, including engine pointer. Passphrase
  // type will remain kTrustedVaultPassphrase, because it is cached by delegate.
  crypto_.Reset();
  ASSERT_THAT(crypto_.GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));

  // There is an ongoing GetIsRecoverabilityRequest(), mimic its completion.
  // Main expectation: no crashes.
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
}

}  // namespace

}  // namespace syncer
