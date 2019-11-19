// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori_state.h"
#include "components/sync/nigori/nigori_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Not;
using testing::NotNull;
using testing::Return;

const char kNigoriKeyName[] = "nigori-key";

NigoriMetadataBatch CreateDummyNigoriMetadataBatch(
    const std::string& progress_marker_token,
    int64_t entity_metadata_sequence_number);

MATCHER(NullTime, "") {
  return arg.is_null();
}

MATCHER_P(HasDefaultKeyDerivedFrom, key_params, "") {
  const Cryptographer& cryptographer = arg;
  std::unique_ptr<Nigori> expected_default_nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string expected_default_key_name;
  EXPECT_TRUE(expected_default_nigori->Permute(
      Nigori::Type::Password, kNigoriKeyName, &expected_default_key_name));
  return cryptographer.GetDefaultEncryptionKeyName() ==
         expected_default_key_name;
}

MATCHER(HasKeystoreNigori, "") {
  const std::unique_ptr<EntityData>& entity_data = arg;
  if (!entity_data || !entity_data->specifics.has_nigori()) {
    return false;
  }
  const sync_pb::NigoriSpecifics& specifics = entity_data->specifics.nigori();
  if (specifics.passphrase_type() !=
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    return false;
  }
  return !specifics.encryption_keybag().blob().empty() &&
         !specifics.keystore_decryptor_token().blob().empty() &&
         specifics.keybag_is_frozen() &&
         specifics.has_keystore_migration_time();
}

MATCHER(HasCustomPassphraseNigori, "") {
  const std::unique_ptr<EntityData>& entity_data = arg;
  if (!entity_data || !entity_data->specifics.has_nigori()) {
    return false;
  }
  const sync_pb::NigoriSpecifics& specifics = entity_data->specifics.nigori();
  return specifics.passphrase_type() ==
             sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE &&
         !specifics.encryption_keybag().blob().empty() &&
         !specifics.has_keystore_decryptor_token() &&
         specifics.encrypt_everything() && specifics.keybag_is_frozen() &&
         specifics.has_custom_passphrase_time() &&
         specifics.has_custom_passphrase_key_derivation_method();
}

MATCHER_P(CanDecryptWith, key_params, "") {
  const Cryptographer& cryptographer = arg;
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string nigori_name;
  EXPECT_TRUE(
      nigori->Permute(Nigori::Type::Password, kNigoriKeyName, &nigori_name));
  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori_name);
  EXPECT_TRUE(nigori->Encrypt(unencrypted, encrypted.mutable_blob()));

  if (!cryptographer.CanDecrypt(encrypted)) {
    return false;
  }
  std::string decrypted;
  if (!cryptographer.DecryptToString(encrypted, &decrypted)) {
    return false;
  }
  return decrypted == unencrypted;
}

MATCHER_P(EncryptedDataEq, expected, "") {
  const sync_pb::EncryptedData& given = arg;
  return given.key_name() == expected.key_name() &&
         given.blob() == expected.blob();
}

MATCHER_P3(EncryptedDataEqAfterDecryption,
           expected,
           password,
           derivation_params,
           "") {
  const sync_pb::EncryptedData& given = arg;
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(password, derivation_params);
  std::string decrypted_given;
  EXPECT_TRUE(cryptographer->DecryptToString(given, &decrypted_given));
  std::string decrypted_expected;
  EXPECT_TRUE(cryptographer->DecryptToString(expected, &decrypted_expected));
  return decrypted_given == decrypted_expected;
}

MATCHER_P2(IsDummyNigoriMetadataBatchWithTokenAndSequenceNumber,
           expected_token,
           expected_sequence_number,
           "") {
  const NigoriMetadataBatch& given = arg;
  NigoriMetadataBatch expected =
      CreateDummyNigoriMetadataBatch(expected_token, expected_sequence_number);
  if (given.model_type_state.SerializeAsString() !=
      expected.model_type_state.SerializeAsString()) {
    return false;
  }
  if (!given.entity_metadata.has_value()) {
    return !expected.entity_metadata.has_value();
  }
  return given.entity_metadata->SerializeAsString() ==
         expected.entity_metadata->SerializeAsString();
}

struct KeyParams {
  KeyDerivationParams derivation_params;
  std::string password;
};

KeyParams Pbkdf2KeyParams(std::string key) {
  return {KeyDerivationParams::CreateForPbkdf2(), std::move(key)};
}

KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return Pbkdf2KeyParams(std::move(encoded_key));
}

KeyParams ScryptKeyParams(const std::string& key) {
  return {KeyDerivationParams::CreateForScrypt("some_constant_salt"), key};
}

std::string PackKeyAsExplicitPassphrase(const KeyParams& key_params,
                                        const Encryptor& encryptor) {
  return NigoriSyncBridgeImpl::PackExplicitPassphraseKeyForTesting(
      encryptor, *CryptographerImpl::FromSingleKeyForTesting(
                     key_params.password, key_params.derivation_params));
}

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keybag_decryptor_params|.
// keystore_decryptor_token is always saved in encryption_keybag, even if it
// is not derived from any params in |keybag_keys_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keybag_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParams>& keybag_keys_params,
    const KeyParams& keystore_decryptor_params,
    const KeyParams& keystore_key_params) {
  sync_pb::NigoriSpecifics specifics;

  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          keystore_decryptor_params.password,
          keystore_decryptor_params.derivation_params);
  for (const KeyParams& key_params : keybag_keys_params) {
    cryptographer->EmplaceKey(key_params.password,
                              key_params.derivation_params);
  }

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<CryptographerImpl> keystore_cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          keystore_key_params.password, keystore_key_params.derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(TimeToProtoTime(base::Time::Now()));
  return specifics;
}

sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<KeyParams>& trusted_vault_key_params) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  for (const KeyParams& key_params : trusted_vault_key_params) {
    const std::string key_name = cryptographer->EmplaceKey(
        key_params.password, key_params.derivation_params);
    cryptographer->SelectDefaultEncryptionKey(key_name);
  }

  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  specifics.set_passphrase_type(
      sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE);
  return specifics;
}

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains keys derived from |passphrase_key_params|
// and |*old_key_params| (if |old_key_params| isn't nullopt). Encrypted with
// key derived from |passphrase_key_params|.
// 2. custom_passphrase_time is current time.
// 3. passphrase_type is CUSTOM_PASSPHRASE.
// 4. encrypt_everything is true.
// 5. Other fields are default.
sync_pb::NigoriSpecifics BuildCustomPassphraseNigoriSpecifics(
    const KeyParams& passphrase_key_params,
    const base::Optional<KeyParams>& old_key_params = base::nullopt) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          passphrase_key_params.password,
          passphrase_key_params.derivation_params);
  if (old_key_params) {
    cryptographer->EmplaceKey(old_key_params->password,
                              old_key_params->derivation_params);
  }

  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));

  specifics.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(
          passphrase_key_params.derivation_params.method()));
  if (passphrase_key_params.derivation_params.method() ==
      KeyDerivationMethod::SCRYPT_8192_8_11) {
    // Persist the salt used for key derivation in Nigori if we're using
    // scrypt.
    std::string encoded_salt;
    base::Base64Encode(passphrase_key_params.derivation_params.scrypt_salt(),
                       &encoded_salt);
    specifics.set_custom_passphrase_key_derivation_salt(encoded_salt);
  }
  specifics.set_custom_passphrase_time(TimeToProtoTime(base::Time::Now()));
  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  specifics.set_encrypt_everything(true);

  return specifics;
}

NigoriMetadataBatch CreateDummyNigoriMetadataBatch(
    const std::string& progress_marker_token,
    int64_t entity_metadata_sequence_number) {
  NigoriMetadataBatch metadata_batch;
  metadata_batch.model_type_state.mutable_progress_marker()->set_token(
      progress_marker_token);
  metadata_batch.entity_metadata = sync_pb::EntityMetadata::default_instance();
  metadata_batch.entity_metadata->set_sequence_number(
      entity_metadata_sequence_number);
  return metadata_batch;
}

class MockNigoriLocalChangeProcessor : public NigoriLocalChangeProcessor {
 public:
  MockNigoriLocalChangeProcessor() = default;
  ~MockNigoriLocalChangeProcessor() = default;

  MOCK_METHOD2(ModelReadyToSync, void(NigoriSyncBridge*, NigoriMetadataBatch));
  MOCK_METHOD1(Put, void(std::unique_ptr<EntityData>));
  MOCK_METHOD0(IsEntityUnsynced, bool());
  MOCK_METHOD0(GetMetadata, NigoriMetadataBatch());
  MOCK_METHOD1(ReportError, void(const ModelError&));
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<ModelTypeControllerDelegate>());
  MOCK_METHOD0(IsTrackingMetadata, bool());
};

class MockObserver : public SyncEncryptionHandler::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() = default;

  MOCK_METHOD3(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const KeyDerivationParams&,
                    const sync_pb::EncryptedData&));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD0(OnTrustedVaultKeyRequired, void());
  MOCK_METHOD0(OnTrustedVaultKeyAccepted, void());
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));
  MOCK_METHOD2(OnEncryptedTypesChanged, void(ModelTypeSet, bool));
  MOCK_METHOD0(OnEncryptionComplete, void());
  MOCK_METHOD2(OnCryptographerStateChanged,
               void(Cryptographer*, bool has_pending_keys));
  MOCK_METHOD2(OnPassphraseTypeChanged, void(PassphraseType, base::Time));
  MOCK_METHOD1(OnLocalSetPassphraseEncryption,
               void(const sync_pb::NigoriSpecifics&));
};

class MockNigoriStorage : public NigoriStorage {
 public:
  MockNigoriStorage() = default;
  ~MockNigoriStorage() override = default;

  MOCK_METHOD1(StoreData, void(const sync_pb::NigoriLocalData&));
  MOCK_METHOD0(RestoreData, base::Optional<sync_pb::NigoriLocalData>());
  MOCK_METHOD0(ClearData, void());
};

class NigoriSyncBridgeImplTest : public testing::Test {
 protected:
  NigoriSyncBridgeImplTest() {
    override_features_.InitAndEnableFeature(
        switches::kSyncSupportTrustedVaultPassphrase);

    auto processor =
        std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
    ON_CALL(*processor, IsTrackingMetadata()).WillByDefault(Return(true));
    processor_ = processor.get();
    auto storage = std::make_unique<testing::NiceMock<MockNigoriStorage>>();
    storage_ = storage.get();
    bridge_ = std::make_unique<NigoriSyncBridgeImpl>(
        std::move(processor), std::move(storage), &encryptor_,
        base::BindRepeating(&Nigori::GenerateScryptSalt),
        /*packed_explicit_passphrase_key=*/std::string());
    bridge_->AddObserver(&observer_);
  }

  ~NigoriSyncBridgeImplTest() override { bridge_->RemoveObserver(&observer_); }

  NigoriSyncBridgeImpl* bridge() { return bridge_.get(); }
  MockNigoriLocalChangeProcessor* processor() { return processor_; }
  MockObserver* observer() { return &observer_; }
  MockNigoriStorage* storage() { return storage_; }

 private:
  base::test::ScopedFeatureList override_features_;
  const FakeEncryptor encryptor_;
  std::unique_ptr<NigoriSyncBridgeImpl> bridge_;
  // Ownership transferred to |bridge_|.
  testing::NiceMock<MockNigoriLocalChangeProcessor>* processor_;
  testing::NiceMock<MockNigoriStorage>* storage_;
  testing::NiceMock<MockObserver> observer_;
};

class NigoriSyncBridgeImplTestWithOptionalScryptDerivation
    : public NigoriSyncBridgeImplTest,
      public testing::WithParamInterface<bool> {
 public:
  NigoriSyncBridgeImplTestWithOptionalScryptDerivation()
      : key_params_(GetParam() ? ScryptKeyParams("passphrase")
                               : Pbkdf2KeyParams("passphrase")) {}

  const KeyParams& GetCustomPassphraseKeyParams() const { return key_params_; }

 private:
  const KeyParams key_params_;
};

// During initialization bridge should expose encrypted types via observers
// notification.
TEST_F(NigoriSyncBridgeImplTest, ShouldNotifyObserversOnInit) {
  // TODO(crbug.com/922900): once persistence is supported for Nigori, this
  // test should be extended to verify whole encryption state.
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(SyncEncryptionHandler::SensitiveTypes(),
                                      /*encrypt_everything=*/false));
  bridge()->Init();
}

// Tests that bridge support Nigori with IMPLICIT_PASSPHRASE.
TEST_F(NigoriSyncBridgeImplTest, ShouldAcceptKeysFromImplicitPassphraseNigori) {
  const KeyParams kKeyParams = Pbkdf2KeyParams("password");
  std::unique_ptr<CryptographerImpl> temp_cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(kKeyParams.password,
                                                 kKeyParams.derivation_params);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  ASSERT_TRUE(temp_cryptographer->Encrypt(
      temp_cryptographer->ToProto().key_bag(),
      entity_data.specifics.mutable_nigori()->mutable_encryption_keybag()));

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*reason=*/REASON_DECRYPTION,
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  bridge()->SetDecryptionPassphrase(kKeyParams.password);

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeyParams));
}

// Simplest case of keystore Nigori: we have only one keystore key and no old
// keys. This keystore key is encrypted in both encryption_keybag and
// keystore_decryptor_token. Client receives such Nigori if initialization of
// Nigori node was done after keystore was introduced and no key rotations
// happened.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromKeystoreNigoriAndNotifyObservers) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(0);
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(0);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // The current implementation issues a redundant notification.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false))
      .Times(2);
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->Init());
  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that client can properly process remote updates with rotated keystore
// nigori. Cryptographer should be able to decrypt any data encrypted with any
// keystore key and use current keystore key as default key.
TEST_F(NigoriSyncBridgeImplTest, ShouldAcceptKeysFromRotatedKeystoreNigori) {
  const std::string kRawOldKey = "raw_old_keystore_key";
  const KeyParams kOldKeyParams = KeystoreKeyParams(kRawOldKey);
  const std::string kRawCurrentKey = "raw_keystore_key";
  const KeyParams kCurrentKeyParams = KeystoreKeyParams(kRawCurrentKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kOldKeyParams, kCurrentKeyParams},
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kCurrentKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawOldKey, kRawCurrentKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kCurrentKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// In the backward compatible mode keystore Nigori's keystore_decryptor_token
// isn't a kestore key, however keystore_decryptor_token itself should be
// encrypted with the keystore key.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromBackwardCompatibleKeystoreNigori) {
  const KeyParams kGaiaKeyParams = Pbkdf2KeyParams("gaia_key");
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kGaiaKeyParams, kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kGaiaKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kGaiaKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kGaiaKeyParams));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldExposeBackwardCompatibleKeystoreNigori) {
  const KeyParams kGaiaKeyParams = Pbkdf2KeyParams("gaia_key");
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kGaiaKeyParams, kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kGaiaKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);
  sync_pb::EncryptedData original_encryption_keybag =
      entity_data.specifics.nigori().encryption_keybag();
  sync_pb::EncryptedData original_keystore_decryptor_token =
      entity_data.specifics.nigori().keystore_decryptor_token();

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  std::unique_ptr<EntityData> local_entity_data = bridge()->GetData();
  ASSERT_TRUE(local_entity_data);
  ASSERT_TRUE(local_entity_data->specifics.has_nigori());
  // Note: EncryptedDataEqAfterDecryption() exercises more strict requirements
  // than bridge must support, because there is nothing wrong with reordering
  // of the keys in encryption_keybag, which will lead to failing this
  // expectation.
  EXPECT_THAT(local_entity_data->specifics.nigori().encryption_keybag(),
              EncryptedDataEqAfterDecryption(original_encryption_keybag,
                                             kGaiaKeyParams.password,
                                             kGaiaKeyParams.derivation_params));
  EXPECT_THAT(
      local_entity_data->specifics.nigori().keystore_decryptor_token(),
      EncryptedDataEqAfterDecryption(original_keystore_decryptor_token,
                                     kKeystoreKeyParams.password,
                                     kKeystoreKeyParams.derivation_params));
}

// Tests that we can successfully use old keys from encryption_keybag in
// backward compatible mode.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptOldKeysFromBackwardCompatibleKeystoreNigori) {
  // |kOldKeyParams| is needed to ensure we was able to decrypt
  // encryption_keybag - there is no way to add key derived from
  // |kOldKeyParams| to cryptographer without decrypting encryption_keybag.
  const KeyParams kOldKeyParams = Pbkdf2KeyParams("old_key");
  const KeyParams kCurrentKeyParams = Pbkdf2KeyParams("current_key");
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  const std::vector<KeyParams> kAllKeyParams = {
      kOldKeyParams, kCurrentKeyParams, kKeystoreKeyParams};
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/kAllKeyParams,
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  for (const KeyParams& key_params : kAllKeyParams) {
    EXPECT_THAT(cryptographer, CanDecryptWith(key_params));
  }
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// Tests that we build keystore Nigori, put it to processor, initialize the
// cryptographer and expose a valid entity through GetData(), when the default
// Nigori is received.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldPutAndMakeCryptographerReadyOnDefaultNigori) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // We don't verify entire NigoriSpecifics here, because it requires too
  // complex matcher (NigoriSpecifics is not determenistic).
  // Calling MergeSyncData() triggers a commit cycle but doesn't immediately
  // expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasKeystoreNigori());

  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasKeystoreNigori());
  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));
  EXPECT_EQ(bridge()->GetPassphraseTypeForTesting(),
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRotateKeystoreKey) {
  const std::string kRawKeystoreKey1 = "raw_keystore_key1";
  const KeyParams kKeystoreKeyParams1 = KeystoreKeyParams(kRawKeystoreKey1);

  sync_pb::NigoriSpecifics not_rotated_specifics = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams1},
      /*keystore_decryptor_params=*/kKeystoreKeyParams1,
      /*keystore_key_params=*/kKeystoreKeyParams1);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = not_rotated_specifics;
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey1}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const std::string kRawKeystoreKey2 = "raw_keystore_key2";
  const KeyParams kKeystoreKeyParams2 = KeystoreKeyParams(kRawKeystoreKey2);
  // Emulate server and client behavior: server sends both keystore keys and
  // |not_rotated_specifics| with changed metadata. Client have already seen
  // this specifics, but should pass it to the bridge, because bridge also
  // issues a commit, which conflicts with |not_rotated_specifics|.

  // Ensure bridge issues a commit right after SetKeystoreKeys() call, because
  // otherwise there is no conflict and ApplySyncChanges() will be called with
  // empty |data|.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey1, kRawKeystoreKey2}));

  // Populate new remote specifics to bridge, which is actually still
  // |not_rotated_specifics|.
  *entity_data.specifics.mutable_nigori() = not_rotated_specifics;
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->ApplySyncChanges(std::move(entity_data)),
              Eq(base::nullopt));

  // Mimic commit completion.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasKeystoreNigori());

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams1));
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams2));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams2));
}

// This test emulates late arrival of keystore keys, so neither
// |keystore_decryptor_token| or |encryption_keybag| could be decrypted at the
// moment NigoriSpecifics arrived. They should be decrypted right after
// keystore keys arrival.
TEST_F(NigoriSyncBridgeImplTest, ShouldDecryptPendingKeysInKeystoreMode) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*reason=*/REASON_DECRYPTION,
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_FALSE(cryptographer.CanEncrypt());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// This test emulates late arrival of keystore keys in backward-compatible
// keystore mode, so neither |keystore_decryptor_token| or |encryption_keybag|
// could be decrypted at the moment NigoriSpecifics arrived. Since default key
// is derived from legacy implicit passphrase, pending keys should be decrypted
// once passphrase passed to SetDecryptionPassphrase(). SetKeystoreKeys()
// intentionally not called in this test, to not allow decryption with
// |keystore_decryptor_token|.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldDecryptPendingKeysWithPassphraseInKeystoreMode) {
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams("keystore_key");
  const KeyParams kPassphraseKeyParams = Pbkdf2KeyParams("passphrase");
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams, kPassphraseKeyParams},
      /*keystore_decryptor_params=*/kPassphraseKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetDecryptionPassphrase(kPassphraseKeyParams.password);

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kPassphraseKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kPassphraseKeyParams));
}

// Tests that unsuccessful attempt of |pending_keys| decryption ends up in
// additional OnPassphraseRequired() call. This is allowed because of possible
// change of |pending_keys| in keystore mode or due to transition from keystore
// to custom passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotifyWhenDecryptionWithPassphraseFailed) {
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams("keystore_key");

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  sync_pb::EncryptedData expected_pending_keys =
      entity_data.specifics.nigori().encryption_keybag();
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*reason=*/REASON_DECRYPTION,
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(expected_pending_keys)));
  bridge()->SetDecryptionPassphrase("wrong_passphrase");

  const CryptographerImpl& cryptographer =
      bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer.KeyBagSizeForTesting(), Eq(size_t(0)));
}

// Tests that attempt to SetEncryptionPassphrase() has no effect (at least
// that bridge's Nigori is still keystore one) if it was called, while bridge
// has pending keys in keystore mode.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotSetEncryptionPassphraseWithPendingKeys) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  bridge()->SetEncryptionPassphrase("passphrase");
  bridge()->SetKeystoreKeys({kRawKeystoreKey});

  // TODO(crbug.com/922900): revisit expectations once conflict resolution is
  // implemented. They might be not properly working with deferred state
  // change.
  EXPECT_THAT(bridge()->GetData(), HasKeystoreNigori());
  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that we can perform initial sync with custom passphrase Nigori.
// We should notify observers about encryption state changes and cryptographer
// shouldn't be ready (by having pending keys) until user provides the
// passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotifyWhenSyncedWithCustomPassphraseNigori) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(Pbkdf2KeyParams("passphrase"));

  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  // The current implementation issues redundant notifications.
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true))
      .Times(2);
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(NotNull(), /*has_pending_keys=*/true))
      .Times(2);
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      Not(NullTime())))
      .Times(2);

  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->Init());
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());
}

// Tests that we can process remote update with custom passphrase Nigori, while
// we already have keystore Nigori locally.
// We should notify observers about encryption state changes and cryptographer
// shouldn't be ready (by having pending keys) until user provides the
// passphrase.
TEST_F(NigoriSyncBridgeImplTest, ShouldTransitToCustomPassphrase) {
  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  // Note: passing default Nigori to MergeSyncData() leads to instantiation of
  // keystore Nigori.
  ASSERT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(Pbkdf2KeyParams("passphrase"));

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      Not(NullTime())));
  EXPECT_THAT(bridge()->ApplySyncChanges(std::move(new_entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());
}

// Tests that bridge doesn't try to overwrite unknown passphrase type and
// report ModelError unless it received default Nigori node (which is
// determined by the size of encryption_keybag). It's a requirement because
// receiving unknown passphrase type might mean that some newer client switched
// to the new passphrase type.
TEST_F(NigoriSyncBridgeImplTest, ShouldFailOnUnknownPassprase) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  entity_data.specifics.mutable_nigori()->mutable_encryption_keybag()->set_blob(
      "data");
  entity_data.specifics.mutable_nigori()->set_passphrase_type(
      sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE + 1);
  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(*processor(), Put(_)).Times(0);
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Ne(base::nullopt));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldClearDataWhenSyncDisabled) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  ASSERT_TRUE(bridge()->GetCryptographerForTesting().CanEncrypt());

  EXPECT_CALL(*storage(), ClearData);
  bridge()->ApplyDisableSyncChanges();
  EXPECT_FALSE(bridge()->GetCryptographerForTesting().CanEncrypt());
}

// Tests decryption logic for explicit passphrase. In order to check that we're
// able to decrypt the data encrypted with old key (i.e. keystore keys or old
// GAIA passphrase) we add one extra key to the encryption keybag.
TEST_P(NigoriSyncBridgeImplTestWithOptionalScryptDerivation,
       ShouldDecryptWithCustomPassphraseAndUpdateDefaultKey) {
  const KeyParams kOldKeyParams = Pbkdf2KeyParams("old_key");
  const KeyParams& passphrase_key_params = GetCustomPassphraseKeyParams();
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(passphrase_key_params,
                                           kOldKeyParams);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*reason=*/REASON_DECRYPTION,
          /*key_derivation_params=*/passphrase_key_params.derivation_params,
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(Ne(std::string()),
                                                   PASSPHRASE_BOOTSTRAP_TOKEN));
  bridge()->SetDecryptionPassphrase(passphrase_key_params.password);

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(passphrase_key_params));
}

INSTANTIATE_TEST_SUITE_P(Scrypt,
                         NigoriSyncBridgeImplTestWithOptionalScryptDerivation,
                         testing::Values(false, true));

// Tests custom passphrase setup logic. Initially Nigori node will be
// initialized with keystore Nigori due to sync with default Nigori. After
// SetEncryptionPassphrase() call observers should be notified about state
// changes, custom passphrase Nigori should be put into the processor and
// exposed through GetData(), cryptographer should encrypt data with custom
// passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldPutAndNotifyObserversWhenSetEncryptionPassphrase) {
  const std::string kCustomPassphrase = "passphrase";

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));
  ASSERT_THAT(bridge()->GetData(), Not(HasCustomPassphraseNigori()));
  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));

  // Calling SetEncryptionPassphrase() triggers a commit cycle but doesn't
  // immediately expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasCustomPassphraseNigori()));
  bridge()->SetEncryptionPassphrase(kCustomPassphrase);
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(Ne(std::string()),
                                                   PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());

  const KeyParams passphrase_key_params = {
      bridge()->GetCustomPassphraseKeyDerivationParamsForTesting(),
      kCustomPassphrase};
  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(passphrase_key_params));
}

// Tests that pending local change with setting custom passphrase is applied,
// when there was a conflicting remote update and remote update is respected.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldSetCustomPassphraseAfterConflictingUpdates) {
  // Start with simple keystore Nigori.
  const std::string kRawKeystoreKey1 = "keystore_key1";
  const KeyParams kKeystoreKeyParams1 = KeystoreKeyParams(kRawKeystoreKey1);
  EntityData simple_keystore_entity_data;
  *simple_keystore_entity_data.specifics.mutable_nigori() =
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kKeystoreKeyParams1},
          /*keystore_decryptor_params=*/kKeystoreKeyParams1,
          /*keystore_key_params=*/kKeystoreKeyParams1);
  bridge()->SetKeystoreKeys({kRawKeystoreKey1});
  ASSERT_THAT(bridge()->MergeSyncData(std::move(simple_keystore_entity_data)),
              Eq(base::nullopt));

  // Set up custom passphrase locally, but don't emulate commit completion.
  const std::string kCustomPassphrase = "custom_passphrase";
  bridge()->SetEncryptionPassphrase(kCustomPassphrase);

  // Emulate conflict with rotated keystore Nigori.
  const std::string kRawKeystoreKey2 = "keystore_key2";
  const KeyParams kKeystoreKeyParams2 = KeystoreKeyParams(kRawKeystoreKey2);
  EntityData rotated_keystore_entity_data;
  *rotated_keystore_entity_data.specifics.mutable_nigori() =
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kKeystoreKeyParams1, kKeystoreKeyParams2},
          /*keystore_decryptor_params=*/kKeystoreKeyParams2,
          /*keystore_key_params=*/kKeystoreKeyParams2);
  bridge()->SetKeystoreKeys({kRawKeystoreKey1, kRawKeystoreKey2});

  // Verify that custom passphrase is set on top of
  // |rotated_keystore_entity_data|.
  EXPECT_CALL(*processor(), Put(HasCustomPassphraseNigori()));
  EXPECT_THAT(
      bridge()->ApplySyncChanges(std::move(rotated_keystore_entity_data)),
      Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(Ne(std::string()),
                                                   PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());

  const KeyParams passphrase_key_params = {
      bridge()->GetCustomPassphraseKeyDerivationParamsForTesting(),
      kCustomPassphrase};
  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams1));
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams2));
  EXPECT_THAT(cryptographer, CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(passphrase_key_params));
}

// Tests that SetEncryptionPassphrase() call doesn't lead to custom passphrase
// change in case we already have one.
TEST_F(NigoriSyncBridgeImplTest, ShouldNotAllowCustomPassphraseChange) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(Pbkdf2KeyParams("passphrase"));
  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(*observer(), OnPassphraseAccepted()).Times(0);
  bridge()->SetEncryptionPassphrase("new_passphrase");
}

// Tests that we can use packed explicit passphrase key passed to bridge to
// decrypt custom passphrase NigoriSpecifics.
TEST(NigoriSyncBridgeImplTestWithPackedExplicitPassphrase,
     ShouldDecryptWithExplicitPassphraseFromPrefs) {
  const KeyParams kKeyParams = Pbkdf2KeyParams("passphrase");

  const FakeEncryptor encryptor;
  auto processor =
      std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
  auto bridge = std::make_unique<NigoriSyncBridgeImpl>(
      std::move(processor),
      std::make_unique<testing::NiceMock<MockNigoriStorage>>(), &encryptor,
      base::BindRepeating(&Nigori::GenerateScryptSalt),
      PackKeyAsExplicitPassphrase(kKeyParams, encryptor));
  testing::NiceMock<MockObserver> observer;
  bridge->AddObserver(&observer);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(kKeyParams);
  ASSERT_TRUE(bridge->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(observer,
              OnCryptographerStateChanged(NotNull(),
                                          /*has_pending_keys=*/false));
  EXPECT_CALL(observer, OnPassphraseRequired(_, _, _)).Times(0);
  ASSERT_THAT(bridge->MergeSyncData(std::move(entity_data)), Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeyParams));
  bridge->RemoveObserver(&observer);
}

TEST(NigoriSyncBridgeImplPersistenceTest, ShouldRestoreKeystoreNigori) {
  // Emulate storing on disc.
  auto storage1 = std::make_unique<testing::NiceMock<MockNigoriStorage>>();
  sync_pb::NigoriLocalData nigori_local_data;
  ON_CALL(*storage1, StoreData(_))
      .WillByDefault([&](const sync_pb::NigoriLocalData& data) {
        nigori_local_data = data;
      });

  // Provide some metadata to verify that we store it.
  auto processor1 =
      std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
  const std::string kDummyProgressMarkerToken = "dummy_token";
  const int64_t kDummySequenceNumber = 100;
  ON_CALL(*processor1, GetMetadata()).WillByDefault([&] {
    return CreateDummyNigoriMetadataBatch(kDummyProgressMarkerToken,
                                          kDummySequenceNumber);
  });

  const FakeEncryptor kEncryptor;
  auto bridge1 = std::make_unique<NigoriSyncBridgeImpl>(
      std::move(processor1), std::move(storage1), &kEncryptor,
      base::BindRepeating(&Nigori::GenerateScryptSalt),
      /*packed_explicit_passphrase_key=*/std::string());

  // Perform initial sync with simple keystore Nigori.
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  ASSERT_TRUE(bridge1->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge1->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  // At this point |nigori_local_data| must be initialized with metadata
  // provided by CreateDummyNigoriMetadataBatch() and data should represent
  // the simple keystore Nigori.

  // Create secondary storage which will return |nigori_local_data| on
  // RestoreData() call.
  auto storage2 = std::make_unique<testing::NiceMock<MockNigoriStorage>>();
  ON_CALL(*storage2, RestoreData()).WillByDefault(Return(nigori_local_data));

  // Create secondary processor, which should expect ModelReadyToSync() call
  // with previously stored metadata.
  auto processor2 =
      std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
  EXPECT_CALL(
      *processor2,
      ModelReadyToSync(NotNull(),
                       IsDummyNigoriMetadataBatchWithTokenAndSequenceNumber(
                           kDummyProgressMarkerToken, kDummySequenceNumber)));

  auto bridge2 = std::make_unique<NigoriSyncBridgeImpl>(
      std::move(processor2), std::move(storage2), &kEncryptor,
      base::BindRepeating(&Nigori::GenerateScryptSalt),
      /*packed_explicit_passphrase_key=*/std::string());

  // Verify that we restored Cryptographer state.
  const Cryptographer& cryptographer = bridge2->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Commit with keystore Nigori initialization might be not completed before
// the browser restart. This test emulates loading non-initialized Nigori
// after restart and expects that bridge will trigger initialization after
// loading.
TEST(NigoriSyncBridgeImplPersistenceTest,
     ShouldInitializeKeystoreNigoriWhenLoadedFromStorage) {
  const KeyParams kKeystoreKeyParams = Pbkdf2KeyParams("keystore_key");
  NigoriState unitialized_state_with_keystore_keys;
  unitialized_state_with_keystore_keys.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(
          {kKeystoreKeyParams.password});

  sync_pb::NigoriLocalData nigori_local_data;
  *nigori_local_data.mutable_nigori_model() =
      unitialized_state_with_keystore_keys.ToLocalProto();

  auto storage = std::make_unique<testing::NiceMock<MockNigoriStorage>>();
  ON_CALL(*storage, RestoreData()).WillByDefault(Return(nigori_local_data));

  auto processor =
      std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
  ON_CALL(*processor, IsTrackingMetadata()).WillByDefault(Return(true));
  MockNigoriLocalChangeProcessor* not_owned_processor = processor.get();

  const FakeEncryptor kEncryptor;
  // Calling bridge constructor triggers a commit cycle but doesn't immediately
  // expose the new state, until the commit completes.
  EXPECT_CALL(*not_owned_processor, Put(HasKeystoreNigori()));
  auto bridge = std::make_unique<NigoriSyncBridgeImpl>(
      std::move(processor), std::move(storage), &kEncryptor,
      base::BindRepeating(&Nigori::GenerateScryptSalt),
      /*packed_explicit_passphrase_key=*/std::string());
  EXPECT_THAT(bridge->GetData(), HasKeystoreNigori());

  // Emulate commit completeness.
  EXPECT_THAT(bridge->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge->GetData(), HasKeystoreNigori());
  EXPECT_THAT(bridge->GetKeystoreMigrationTime(), Not(NullTime()));
  EXPECT_EQ(bridge->GetPassphraseTypeForTesting(),
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

  const Cryptographer& cryptographer = bridge->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests the initial sync with a trusted vault Nigori. Observers should be
// notified about encryption state changes and cryptographer shouldn't be ready
// (by having pending keys) until the passphrase is received by means other than
// the sync protocol.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldRequireUserActionIfInitiallyUsingTrustedVault) {
  const std::string kTrustedVaultKey = "trusted_vault_key";
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({Pbkdf2KeyParams(kTrustedVaultKey)});

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  // The current implementation issues redundant notifications.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(NotNull(), /*has_pending_keys=*/true))
      .Times(2);
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                      NullTime()))
      .Times(2);
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(2);

  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->Init());
  EXPECT_THAT(bridge()->GetPassphraseTypeForTesting(),
              Eq(sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE));
  EXPECT_THAT(bridge()->GetEncryptedTypesForTesting(),
              Eq(SyncEncryptionHandler::SensitiveTypes()));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnTrustedVaultKeyAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());
}

// Tests the processing of a remote incremental update that transitions from
// keystore to trusted vault passphrase, which requires receiving the new
// passphrase by means other than the sync protocol.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteTransitionFromKeystoreToTrustedVault) {
  const std::string kTrustedVaultKey = "trusted_vault_key";

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  // Note: passing default Nigori to MergeSyncData() leads to instantiation of
  // keystore Nigori.
  ASSERT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));
  ASSERT_TRUE(bridge()->Init());

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({Pbkdf2KeyParams(kTrustedVaultKey)});

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                      NullTime()));
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired());
  EXPECT_THAT(bridge()->ApplySyncChanges(std::move(new_entity_data)),
              Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetPassphraseTypeForTesting(),
              Eq(sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE));
  EXPECT_THAT(bridge()->GetEncryptedTypesForTesting(),
              Eq(SyncEncryptionHandler::SensitiveTypes()));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnTrustedVaultKeyAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());
}

// Tests the processing of a remote incremental update that rotates the trusted
// vault passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteKeyRotationForTrustedVault) {
  const std::string kTrustedVaultKey = "trusted_vault_key";
  const std::string kRotatedTrustedVaultKey = "rotated_vault_key";

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(0);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({Pbkdf2KeyParams(kTrustedVaultKey)});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  ASSERT_TRUE(bridge()->Init());
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseTypeForTesting(),
              Eq(sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE));
  // Mimic remote key rotation.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics(
          {Pbkdf2KeyParams(kTrustedVaultKey),
           Pbkdf2KeyParams(kRotatedTrustedVaultKey)});
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnPassphraseTypeChanged(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired());

  EXPECT_THAT(bridge()->ApplySyncChanges(std::move(new_entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnTrustedVaultKeyAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  bridge()->AddTrustedVaultDecryptionKeys({kRotatedTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());
}

// Tests transitioning locally from trusted vault passphrase to custom
// passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldTransitionLocallyFromTrustedVaultToCustomPassphrase) {
  const std::string kTrustedVaultKey = "trusted_vault_key";
  const std::string kCustomPassphrase = "custom_passphrase";

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({Pbkdf2KeyParams(kTrustedVaultKey)});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  ASSERT_TRUE(bridge()->Init());
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseTypeForTesting(),
              Eq(sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE));
  ASSERT_THAT(bridge()->GetData(), Not(HasCustomPassphraseNigori()));

  // Calling SetEncryptionPassphrase() triggers a commit cycle but doesn't
  // immediately expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasCustomPassphraseNigori()));
  bridge()->SetEncryptionPassphrase(kCustomPassphrase);
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(Ne(std::string()),
                                                   PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_THAT(bridge()->ApplySyncChanges(base::nullopt), Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasCustomPassphraseNigori());
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotAddDecryptionKeysToTrustedVaultCryptographer) {
  const std::string kTrustedVaultKey1 = "trusted_vault_key_1";
  const std::string kTrustedVaultKey2 = "trusted_vault_key_2";
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({Pbkdf2KeyParams(kTrustedVaultKey1)});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->Init());
  ASSERT_THAT(bridge()->GetPassphraseTypeForTesting(),
              Eq(sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE));
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());

  // Note that |kTrustedVaultKey2| was not part of Nigori specifics.
  bridge()->AddTrustedVaultDecryptionKeys(
      {kTrustedVaultKey1, kTrustedVaultKey2});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());

  const CryptographerImpl& cryptographer =
      bridge()->GetCryptographerForTesting();
  ASSERT_THAT(cryptographer,
              CanDecryptWith(Pbkdf2KeyParams(kTrustedVaultKey1)));
  EXPECT_THAT(cryptographer,
              Not(CanDecryptWith(Pbkdf2KeyParams(kTrustedVaultKey2))));
  EXPECT_THAT(cryptographer.KeyBagSizeForTesting(), Eq(size_t(1)));
}

}  // namespace

}  // namespace syncer
