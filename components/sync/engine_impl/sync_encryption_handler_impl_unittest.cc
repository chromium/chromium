// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/sync_encryption_handler_impl.h"

#include <stdint.h>

#include <cstring>
#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/model_type_test_util.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;

// The raw keystore key the server sends.
static const char kRawKeystoreKey[] = "keystore_key";
// Base64 encoded version of |kRawKeystoreKey|.
static const char kKeystoreKey[] = "a2V5c3RvcmVfa2V5";
static const char kCustomPassphrase[] = "custom_passphrase";
// Denotes a value of custom_passphrase_key_derivation_method in NigoriSpecifics
// that is not a valid value from NigoriSpecifics::KeyDerivationMethod.
static const ::google::protobuf::int32 kUnsupportedKeyDerivationMethod = 12345;
static const char kScryptSalt[] = "Salt string used for scrypt";

enum class ExpectedKeyDerivationMethodStateForMetrics {
  NOT_SET = 0,
  UNSUPPORTED = 1,
  PBKDF2_HMAC_SHA1_1003 = 2,
  SCRYPT_8192_8_11 = 3
};

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD3(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const KeyDerivationParams&,
                    const sync_pb::EncryptedData&));  // NOLINT
  MOCK_METHOD0(OnPassphraseAccepted, void());         // NOLINT
  MOCK_METHOD0(OnTrustedVaultKeyRequired, void());    // NOLINT
  MOCK_METHOD0(OnTrustedVaultKeyAccepted, void());    // NOLINT
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));  // NOLINT
  MOCK_METHOD2(OnEncryptedTypesChanged, void(ModelTypeSet, bool));  // NOLINT
  MOCK_METHOD0(OnEncryptionComplete, void());                       // NOLINT
  MOCK_METHOD2(OnCryptographerStateChanged,
               void(Cryptographer*, bool));  // NOLINT
  MOCK_METHOD2(OnPassphraseTypeChanged,
               void(PassphraseType,
                    base::Time));  // NOLINT
  MOCK_METHOD1(OnLocalSetPassphraseEncryption,
               void(const sync_pb::NigoriSpecifics&));  // NOLINT
};

}  // namespace

class SyncEncryptionHandlerImplTest : public ::testing::Test {
 public:
  SyncEncryptionHandlerImplTest() {}
  ~SyncEncryptionHandlerImplTest() override {}

  void SetUp() override {
    fake_random_salt_generator_ =
        base::BindRepeating([]() { return std::string(kScryptSalt); });
    test_user_share_.SetUp();
    SetUpEncryption();
    CreateRootForType(NIGORI);
  }

  void TearDown() override {
    PumpLoop();
    test_user_share_.TearDown();
  }

 protected:
  void SetScryptFeaturesState(bool force_disabled,
                              bool use_for_new_passphrases) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (force_disabled) {
      enabled_features.push_back(
          switches::kSyncForceDisableScryptForCustomPassphrase);
    } else {
      disabled_features.push_back(
          switches::kSyncForceDisableScryptForCustomPassphrase);
    }
    if (use_for_new_passphrases) {
      enabled_features.push_back(
          switches::kSyncUseScryptForNewCustomPassphrases);
    } else {
      disabled_features.push_back(
          switches::kSyncUseScryptForNewCustomPassphrases);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpEncryption() {
    SetUpEncryptionWithKeyForBootstrapping(std::string());
  }

  void SetUpEncryptionWithKeyForBootstrapping(
      const std::string& key_for_bootstrapping) {
    encryption_handler_ = std::make_unique<SyncEncryptionHandlerImpl>(
        user_share(), &encryptor_, key_for_bootstrapping,
        std::string() /* keystore key for bootstrapping */,
        fake_random_salt_generator_);
    encryption_handler_->AddObserver(&observer_);
  }

  void CreateRootForType(ModelType model_type) {
    syncable::Directory* directory = user_share()->directory.get();

    std::string tag_name = ModelTypeToRootTag(model_type);

    syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
    syncable::MutableEntry node(&wtrans, syncable::CREATE, model_type,
                                wtrans.root_id(), tag_name);
    node.PutUniqueServerTag(tag_name);
    node.PutIsDir(true);
    node.PutServerIsDir(false);
    node.PutIsUnsynced(false);
    node.PutIsUnappliedUpdate(false);
    node.PutServerVersion(20);
    node.PutBaseVersion(20);
    node.PutIsDel(false);
    node.PutId(ids_.MakeServer(tag_name));
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(model_type, &specifics);
    node.PutSpecifics(specifics);
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  // Getters for tests.
  UserShare* user_share() { return test_user_share_.user_share(); }
  SyncEncryptionHandlerImpl* encryption_handler() {
    return encryption_handler_.get();
  }
  SyncEncryptionHandlerObserverMock* observer() { return &observer_; }
  DirectoryCryptographer* GetCryptographer() {
    return encryption_handler_->GetMutableCryptographerForTesting();
  }

  sync_pb::NigoriSpecifics ReadNigoriSpecifics() {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode nigori_node(&trans);
    BaseNode::InitByLookupResult result = nigori_node.InitTypeRoot(NIGORI);
    DCHECK_EQ(result, BaseNode::INIT_OK);
    return nigori_node.GetNigoriSpecifics();
  }

  void VerifyMigratedNigori(
      PassphraseType passphrase_type,
      const std::string& passphrase,
      const base::Optional<KeyDerivationParams>& key_derivation_params) {
    VerifyMigratedNigoriWithTimestamp(0, passphrase_type, passphrase,
                                      key_derivation_params);
  }

  void VerifyMigratedNigoriWithTimestamp(
      int64_t migration_time,
      PassphraseType passphrase_type,
      const std::string& passphrase,
      const base::Optional<KeyDerivationParams>& key_derivation_params) {
    const sync_pb::NigoriSpecifics& nigori = ReadNigoriSpecifics();
    if (migration_time > 0) {
      EXPECT_EQ(migration_time, nigori.keystore_migration_time());
    } else {
      EXPECT_TRUE(nigori.has_keystore_migration_time());
    }
    EXPECT_TRUE(nigori.keybag_is_frozen());
    if (passphrase_type == PassphraseType::kCustomPassphrase ||
        passphrase_type == PassphraseType::kFrozenImplicitPassphrase) {
      EXPECT_TRUE(nigori.encrypt_everything());
      EXPECT_TRUE(nigori.keystore_decryptor_token().blob().empty());
      if (passphrase_type == PassphraseType::kCustomPassphrase) {
        EXPECT_EQ(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE,
                  nigori.passphrase_type());
        if (key_derivation_params.has_value()) {
          EXPECT_EQ(ProtoKeyDerivationMethodToEnum(
                        nigori.custom_passphrase_key_derivation_method()),
                    key_derivation_params.value().method());
          if (key_derivation_params.value().method() ==
              KeyDerivationMethod::SCRYPT_8192_8_11) {
            std::string decoded_salt;
            base::Base64Decode(nigori.custom_passphrase_key_derivation_salt(),
                               &decoded_salt);
            EXPECT_EQ(key_derivation_params.value().scrypt_salt(),
                      decoded_salt);
          }
        }
        if (!encryption_handler()->custom_passphrase_time().is_null()) {
          EXPECT_EQ(
              nigori.custom_passphrase_time(),
              TimeToProtoTime(encryption_handler()->custom_passphrase_time()));
        }
      } else {
        DCHECK(!key_derivation_params.has_value());
        EXPECT_EQ(sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE,
                  nigori.passphrase_type());
        EXPECT_FALSE(nigori.has_custom_passphrase_key_derivation_method());
      }
    } else {
      DCHECK(!key_derivation_params.has_value());
      EXPECT_FALSE(nigori.encrypt_everything());
      EXPECT_FALSE(nigori.keystore_decryptor_token().blob().empty());
      EXPECT_EQ(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE,
                nigori.passphrase_type());
      EXPECT_FALSE(nigori.has_custom_passphrase_key_derivation_method());
      DirectoryCryptographer keystore_cryptographer;
      KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), kKeystoreKey};
      keystore_cryptographer.AddKey(params);
      EXPECT_TRUE(keystore_cryptographer.CanDecryptUsingDefaultKey(
          nigori.keystore_decryptor_token()));
    }

    DirectoryCryptographer temp_cryptographer;
    if (key_derivation_params.has_value() &&
        passphrase_type == PassphraseType::kCustomPassphrase) {
      temp_cryptographer.AddKey({key_derivation_params.value(), passphrase});
    } else {
      temp_cryptographer.AddKey(
          {KeyDerivationParams::CreateForPbkdf2(), passphrase});
    }

    EXPECT_TRUE(temp_cryptographer.CanDecryptUsingDefaultKey(
        nigori.encryption_keybag()));
  }

  sync_pb::NigoriSpecifics BuildMigratedNigori(
      PassphraseType passphrase_type,
      int64_t migration_time,
      ::google::protobuf::int32 proto_key_derivation_method,
      const std::string& default_passphrase,
      const std::string& keystore_key,
      const base::Optional<std::string>& key_derivation_salt) {
    DCHECK_NE(passphrase_type, PassphraseType::kImplicitPassphrase);
    DirectoryCryptographer other_cryptographer;

    std::string default_key = default_passphrase;
    if (default_key.empty()) {
      default_key = keystore_key;
    } else {
      KeyParams keystore_params = {KeyDerivationParams::CreateForPbkdf2(),
                                   keystore_key};
      other_cryptographer.AddKey(keystore_params);
    }

    KeyDerivationMethod key_derivation_method =
        ProtoKeyDerivationMethodToEnum(proto_key_derivation_method);
    if (key_derivation_method == KeyDerivationMethod::UNSUPPORTED) {
      // Since this is an unsupported method, we need to simulate an
      // undecryptable keybag. To do this, we will use an arbitrary passphrase
      // that SyncEncryptionHandlerImpl does not know about, therefore ensuring
      // that the keybag cannot be decrypted in tests.
      KeyParams underivable_key_params = {
          KeyDerivationParams::CreateForPbkdf2(),
          "SyncEncryptionHandlerImpl does not know this passphrase!"};
      other_cryptographer.AddKey(underivable_key_params);
    } else {
      // Since scrypt might be forcibly disabled, we want to temporarily
      // re-enable it so that we have no problems when we try to derive a key
      // using it.
      base::test::ScopedFeatureList scrypt_enabled_feature_list;
      scrypt_enabled_feature_list.InitWithFeatureState(
          switches::kSyncForceDisableScryptForCustomPassphrase, false);

      switch (key_derivation_method) {
        case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
          DCHECK(!key_derivation_salt.has_value());
          other_cryptographer.AddKey(
              {KeyDerivationParams::CreateForPbkdf2(), default_key});
          break;
        case KeyDerivationMethod::SCRYPT_8192_8_11:
          DCHECK(key_derivation_salt.has_value());
          other_cryptographer.AddKey({KeyDerivationParams::CreateForScrypt(
                                          key_derivation_salt.value()),
                                      default_key});
          break;
        case KeyDerivationMethod::UNSUPPORTED:
          NOTREACHED();
          break;
      }
    }

    EXPECT_TRUE(other_cryptographer.CanEncrypt());

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(migration_time);

    if (passphrase_type == PassphraseType::kKeystorePassphrase) {
      sync_pb::EncryptedData keystore_decryptor_token;
      EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
          other_cryptographer, keystore_key, &keystore_decryptor_token));
      nigori.mutable_keystore_decryptor_token()->CopyFrom(
          keystore_decryptor_token);
      nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    } else {
      nigori.set_encrypt_everything(true);
      nigori.set_passphrase_type(
          passphrase_type == PassphraseType::kCustomPassphrase
              ? sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE
              : sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE);
      nigori.set_custom_passphrase_key_derivation_method(
          proto_key_derivation_method);
      if (key_derivation_method == KeyDerivationMethod::SCRYPT_8192_8_11) {
        // Also persist the salt in Nigori.
        DCHECK(key_derivation_salt.has_value());
        std::string encoded_salt;
        base::Base64Encode(key_derivation_salt.value(), &encoded_salt);
        nigori.set_custom_passphrase_key_derivation_salt(encoded_salt);
      }
    }
    return nigori;
  }

  sync_pb::NigoriSpecifics BuildKeystoreMigratedNigori(
      int64_t migration_time,
      const std::string& default_passphrase,
      const std::string& keystore_key) {
    return BuildMigratedNigori(
        PassphraseType::kKeystorePassphrase, migration_time,
        sync_pb::NigoriSpecifics::UNSPECIFIED, default_passphrase, keystore_key,
        /* key_derivation_salt = */ base::nullopt);
  }

  sync_pb::NigoriSpecifics BuildCustomPassMigratedNigori(
      int64_t migration_time,
      ::google::protobuf::int32 key_derivation_method,
      const std::string& default_passphrase,
      const base::Optional<std::string>& key_derivation_salt) {
    return BuildMigratedNigori(PassphraseType::kCustomPassphrase,
                               migration_time, key_derivation_method,
                               default_passphrase, kKeystoreKey,
                               key_derivation_salt);
  }

  void VerifyPassphraseType(PassphraseType passphrase_type) {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_EQ(passphrase_type,
              encryption_handler()->GetPassphraseType(trans.GetWrappedTrans()));
  }

  // Calls SyncEncryptionHandlerImpl::SetKeystoreKeys().
  void SetupKeystoreKeys(const std::vector<std::string>& keystore_keys) {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(keystore_keys);
    PumpLoop();
    Mock::VerifyAndClearExpectations(observer());
  }

  void WriteNigori(const sync_pb::NigoriSpecifics& nigori) {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  // Build a migrated nigori node with the specified default passphrase
  // and keystore key and initialize the encryption handler with it, verifying
  // that the initialization was performed correctly.
  void InitAndVerifyKeystoreMigratedNigori(
      int64_t migration_time,
      const std::string& default_passphrase,
      const std::string& keystore_key) {
    EXPECT_CALL(*observer(), OnPassphraseAccepted());

    EXPECT_CALL(*observer(), OnPassphraseTypeChanged(
                                 PassphraseType::kKeystorePassphrase, _));
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
    EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AtLeast(1));

    InitKeystoreMigratedNigori(migration_time, default_passphrase,
                               keystore_key);

    EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
    VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
    EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

  // Build a migrated nigori node with the specified default passphrase
  // and keystore key and initialize the encryption handler with it.
  void InitKeystoreMigratedNigori(int64_t migration_time,
                                  const std::string& default_passphrase,
                                  const std::string& keystore_key) {
    CreateRootForType(NIGORI);
    sync_pb::NigoriSpecifics nigori = BuildKeystoreMigratedNigori(
        migration_time, default_passphrase, keystore_key);
    WriteNigori(nigori);
    encryption_handler()->Init();
    PumpLoop();
  }

  // Build a migrated nigori node with the specified default passphrase as a
  // custom passphrase and initialize the encryption handler with it, verifying
  // that the initialization was performed correctly.
  void InitAndVerifyCustomPassphraseMigratedNigori(
      int64_t migration_time,
      ::google::protobuf::int32 key_derivation_method,
      const std::string& default_passphrase,
      const base::Optional<std::string>& key_derivation_salt) {
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true))
        .Times(AtLeast(1));
    EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AnyNumber());
    // OnPassphraseRequired might be called if the cryptographer is not
    // populated with keys before we Init().
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(AnyNumber());

    InitCustomPassMigratedNigori(migration_time, key_derivation_method,
                                 default_passphrase, key_derivation_salt);

    EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
    VerifyPassphraseType(PassphraseType::kCustomPassphrase);
    EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

  // Build a migrated nigori node with the specified default passphrase as a
  // custom passphrase and initialize the encryption handler with it.
  void InitCustomPassMigratedNigori(
      int64_t migration_time,
      ::google::protobuf::int32 key_derivation_method,
      const std::string& default_passphrase,
      const base::Optional<std::string>& key_derivation_salt) {
    sync_pb::NigoriSpecifics nigori =
        BuildCustomPassMigratedNigori(migration_time, key_derivation_method,
                                      default_passphrase, key_derivation_salt);
    WriteNigori(nigori);
    encryption_handler()->Init();
  }

  // Returns the serialized key that should have been derived for the given key
  // derivation method and given custom passphrase. Can be compared with the
  // return value of Cryptographer::GetDefaultNigoriKeyData.
  std::string GetSerializedNigoriKeyForCustomPassphrase(
      const KeyDerivationParams& key_derivation_params,
      const std::string& passphrase) {
    sync_pb::NigoriKey key;
    std::unique_ptr<Nigori> nigori =
        Nigori::CreateByDerivation(key_derivation_params, passphrase);
    nigori->ExportKeys(key.mutable_deprecated_user_key(),
                       key.mutable_encryption_key(), key.mutable_mac_key());
    return key.SerializeAsString();
  }

  // Build an unmigrated nigori node with the specified passphrase and type and
  // initialize the encryption handler with it, verifying that the
  // initialization was performed correctly.
  void InitAndVerifyUnmigratedNigori(const std::string& default_passphrase,
                                     PassphraseType passphrase_type) {
    if (passphrase_type != PassphraseType::kImplicitPassphrase) {
      EXPECT_CALL(*observer(), OnPassphraseTypeChanged(passphrase_type, _));
    }
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));

    InitUnmigratedNigori(default_passphrase, passphrase_type);

    EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
    VerifyPassphraseType(passphrase_type);
    EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

  // Build an unmigrated nigori node with the specified passphrase and type and
  // initialize the encryption handler with it.
  void InitUnmigratedNigori(const std::string& default_passphrase,
                            PassphraseType passphrase_type) {
    DCHECK_NE(passphrase_type, PassphraseType::kFrozenImplicitPassphrase);
    DirectoryCryptographer other_cryptographer;
    KeyParams default_key = {KeyDerivationParams::CreateForPbkdf2(),
                             default_passphrase};
    other_cryptographer.AddKey(default_key);
    EXPECT_TRUE(other_cryptographer.CanEncrypt());

    {
      WriteTransaction trans(FROM_HERE, user_share());
      WriteNode nigori_node(&trans);
      ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
      sync_pb::NigoriSpecifics nigori;
      other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
      nigori.set_keybag_is_frozen(passphrase_type ==
                                  PassphraseType::kCustomPassphrase);
      nigori_node.SetNigoriSpecifics(nigori);
    }
    encryption_handler()->Init();
  }

  // Verify we can restore the SyncEncryptionHandler state using a saved
  // |bootstrap_token| and |nigori_state|.
  //
  // |migration_time| is the time migration occurred.
  //
  // |passphrase| is the explicit passphrase.
  //
  // |passphrase_type| is either CUSTOM_PASSPHRASE or
  // FROZEN_IMPLICIT_PASSPHRASE.
  void VerifyRestoreAfterExplicitPaspshrase(
      int64_t migration_time,
      const std::string& passphrase,
      const std::string& bootstrap_token,
      const sync_pb::NigoriSpecifics& nigori_specifics,
      PassphraseType passphrase_type,
      const base::Optional<KeyDerivationParams>& key_derivation_params) {
    TearDown();
    test_user_share_.SetUp();
    SetUpEncryptionWithKeyForBootstrapping(bootstrap_token);
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseTypeChanged(passphrase_type, _));
    EXPECT_CALL(*observer(), OnEncryptionComplete());
    encryption_handler()->RestoreNigoriForTesting(nigori_specifics);
    encryption_handler()->Init();
    Mock::VerifyAndClearExpectations(observer());
    VerifyMigratedNigoriWithTimestamp(migration_time, passphrase_type,
                                      passphrase, key_derivation_params);
  }

  // Sets up the observer mocks so that any calls to its methods would satisfy
  // them.
  // TODO(davidovic): Use a nice mock instead. However, that's not trivial
  // because it is not obvious which of existing EXPECT_CALLs are testing some
  // behavior and which have just been added to fulfill the strict mock.
  void IgnoreAllObserverCalls() {
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseAccepted()).Times(AnyNumber());
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, _)).Times(AnyNumber());
    EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AnyNumber());
    EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseTypeChanged(_, _)).Times(AnyNumber());
    EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _)).Times(AnyNumber());
  }

 protected:
  TestUserShare test_user_share_;
  FakeEncryptor encryptor_;
  std::unique_ptr<SyncEncryptionHandlerImpl> encryption_handler_;
  StrictMock<SyncEncryptionHandlerObserverMock> observer_;
  TestIdFactory ids_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::RepeatingCallback<std::string()> fake_random_salt_generator_;
};

// Verify that the encrypted types are being written to and read from the
// nigori node properly.
TEST_F(SyncEncryptionHandlerImplTest, NigoriEncryptionTypes) {
  sync_pb::NigoriSpecifics nigori;

  StrictMock<SyncEncryptionHandlerObserverMock> observer2;
  SyncEncryptionHandlerImpl handler2(user_share(), &encryptor_, std::string(),
                                     std::string() /* bootstrap tokens */,
                                     fake_random_salt_generator_);
  handler2.AddObserver(&observer2);

  // Just set the sensitive types (shouldn't trigger any notifications).
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->MergeEncryptedTypes(encrypted_types,
                                              trans.GetWrappedTrans());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori, trans.GetWrappedTrans());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_EQ(encrypted_types, encryption_handler()->GetEncryptedTypesUnsafe());
  EXPECT_EQ(encrypted_types, handler2.GetEncryptedTypesUnsafe());

  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);

  ModelTypeSet encrypted_user_types = EncryptableUserTypes();

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               HasModelTypes(encrypted_user_types), false));
  EXPECT_CALL(observer2, OnEncryptedTypesChanged(
                             HasModelTypes(encrypted_user_types), false));

  // Set all encrypted types
  encrypted_types = EncryptableUserTypes();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->MergeEncryptedTypes(encrypted_types,
                                              trans.GetWrappedTrans());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori, trans.GetWrappedTrans());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_EQ(encrypted_types, encryption_handler()->GetEncryptedTypesUnsafe());
  EXPECT_EQ(encrypted_types, handler2.GetEncryptedTypesUnsafe());

  // Receiving an empty nigori should not reset any encrypted types or trigger
  // an observer notification.
  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);
  nigori = sync_pb::NigoriSpecifics();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_EQ(encrypted_types, encryption_handler()->GetEncryptedTypesUnsafe());
}

// Verify the encryption handler processes the encrypt everything field
// properly.
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingExplicit) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_everything(true);

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               HasModelTypes(EncryptableUserTypes()), true));

  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_EQ(ModelTypeSet(PASSWORDS, WIFI_CONFIGURATIONS), encrypted_types);

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori, trans.GetWrappedTrans());
  }

  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.HasAll(EncryptableUserTypes()));

  // Receiving the nigori node again shouldn't trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori, trans.GetWrappedTrans());
  }
}

// Verify the encryption handler can detect an implicit encrypt everything state
// (from clients that failed to write the encrypt everything field).
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingImplicit) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_bookmarks(true);  // Non-passwords = encrypt everything

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               HasModelTypes(EncryptableUserTypes()), true));

  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_EQ(ModelTypeSet(PASSWORDS, WIFI_CONFIGURATIONS), encrypted_types);

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori, trans.GetWrappedTrans());
  }

  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.HasAll(EncryptableUserTypes()));

  // Receiving a nigori node with encrypt everything explicitly set shouldn't
  // trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  nigori.set_encrypt_everything(true);
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori, trans.GetWrappedTrans());
  }
}

// Verify the encryption handler can deal with new versions treating new types
// as Sensitive, and that it does not consider this an implicit encrypt
// everything case.
TEST_F(SyncEncryptionHandlerImplTest, UnknownSensitiveTypes) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_everything(false);
  nigori.set_encrypt_bookmarks(true);

  ModelTypeSet expected_encrypted_types =
      SyncEncryptionHandler::SensitiveTypes();
  expected_encrypted_types.Put(BOOKMARKS);

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               HasModelTypes(expected_encrypted_types), false));

  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_EQ(ModelTypeSet(PASSWORDS, WIFI_CONFIGURATIONS), encrypted_types);

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori, trans.GetWrappedTrans());
  }

  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PASSWORDS, WIFI_CONFIGURATIONS),
            encrypted_types);
}

// Receive an old nigori with old encryption keys and encrypted types. We should
// not revert our default key or encrypted types, and should post a task to
// overwrite the existing nigori with the correct data.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveOldNigori) {
  KeyParams old_key = {KeyDerivationParams::CreateForPbkdf2(), "old"};
  KeyParams current_key = {KeyDerivationParams::CreateForPbkdf2(), "cur"};

  // Data for testing encryption/decryption.
  DirectoryCryptographer other_cryptographer;
  other_cryptographer.AddKey(old_key);
  sync_pb::EntitySpecifics other_encrypted_specifics;
  other_encrypted_specifics.mutable_bookmark()->set_title("title");
  other_cryptographer.Encrypt(other_encrypted_specifics,
                              other_encrypted_specifics.mutable_encrypted());
  sync_pb::EntitySpecifics our_encrypted_specifics;
  our_encrypted_specifics.mutable_bookmark()->set_title("title2");

  // Set up the current encryption state (containing both keys and encrypt
  // everything).
  sync_pb::NigoriSpecifics current_nigori_specifics;
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(current_key);
  GetCryptographer()->Encrypt(our_encrypted_specifics,
                              our_encrypted_specifics.mutable_encrypted());
  GetCryptographer()->GetKeys(
      current_nigori_specifics.mutable_encryption_keybag());
  current_nigori_specifics.set_encrypt_everything(true);

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               HasModelTypes(EncryptableUserTypes()), true));
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(current_nigori_specifics,
                                            trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now set up the old nigori specifics and apply it on top.
  // Has an old set of keys, and no encrypted types.
  sync_pb::NigoriSpecifics old_nigori;
  other_cryptographer.GetKeys(old_nigori.mutable_encryption_keybag());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(old_nigori,
                                            trans.GetWrappedTrans());
  }
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  EXPECT_FALSE(GetCryptographer()->has_pending_keys());

  // Encryption handler should have posted a task to overwrite the old
  // specifics.
  PumpLoop();

  {
    // The cryptographer should be able to decrypt both sets of keys and still
    // be encrypting with the newest, and the encrypted types should be the
    // most recent.
    // In addition, the nigori node should match the current encryption state.
    const sync_pb::NigoriSpecifics nigori = ReadNigoriSpecifics();
    EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(
        our_encrypted_specifics.encrypted()));
    EXPECT_TRUE(
        GetCryptographer()->CanDecrypt(other_encrypted_specifics.encrypted()));
    EXPECT_TRUE(GetCryptographer()->CanDecrypt(nigori.encryption_keybag()));
    EXPECT_TRUE(nigori.encrypt_everything());
    EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(
        nigori.encryption_keybag()));
  }
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
}

// Ensure setting the keystore key works, updates the bootstrap token, and
// triggers a non-backwards compatible migration. Then verify that the
// bootstrap token can be correctly parsed by the encryption handler at startup
// time.
TEST_F(SyncEncryptionHandlerImplTest, SetKeystoreMigratesAndUpdatesBootstrap) {
  // Passing no keys should do nothing.
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  EXPECT_FALSE(GetCryptographer()->is_initialized());
  EXPECT_TRUE(encryption_handler()->NeedKeystoreKey());
  EXPECT_FALSE(encryption_handler()->SetKeystoreKeys({""}));
  EXPECT_TRUE(encryption_handler()->NeedKeystoreKey());
  Mock::VerifyAndClearExpectations(observer());

  // Build a set of keystore keys.
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);

  // Pass them to the encryption handler, triggering a migration and bootstrap
  // token update.
  std::string encoded_key;
  std::string keystore_bootstrap;
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN))
      .WillOnce(SaveArg<0>(&keystore_bootstrap));
  EXPECT_TRUE(encryption_handler()->SetKeystoreKeys(
      {kRawOldKeystoreKey, kRawKeystoreKey}));
  EXPECT_FALSE(encryption_handler()->NeedKeystoreKey());
  EXPECT_FALSE(GetCryptographer()->is_initialized());
  PumpLoop();
  EXPECT_TRUE(GetCryptographer()->is_initialized());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_params=*/base::nullopt);

  // Ensure the bootstrap is encoded properly (a base64 encoded encrypted blob
  // of list values containing the keystore keys).
  std::string decoded_bootstrap;
  ASSERT_TRUE(base::Base64Decode(keystore_bootstrap, &decoded_bootstrap));
  std::string decrypted_bootstrap;
  ASSERT_TRUE(
      encryptor_.DecryptString(decoded_bootstrap, &decrypted_bootstrap));
  JSONStringValueDeserializer json(decrypted_bootstrap);
  std::unique_ptr<base::Value> deserialized_keystore_keys(
      json.Deserialize(nullptr, nullptr));
  ASSERT_TRUE(deserialized_keystore_keys.get());
  base::ListValue* keystore_list = nullptr;
  deserialized_keystore_keys->GetAsList(&keystore_list);
  ASSERT_TRUE(keystore_list);
  ASSERT_EQ(2U, keystore_list->GetSize());
  std::string test_string;
  keystore_list->GetString(0, &test_string);
  ASSERT_EQ(old_keystore_key, test_string);
  keystore_list->GetString(1, &test_string);
  ASSERT_EQ(kKeystoreKey, test_string);

  // Now make sure a new encryption handler can correctly parse the bootstrap
  // token.
  SyncEncryptionHandlerImpl handler2(user_share(), &encryptor_,
                                     std::string(),  // Cryptographer bootstrap.
                                     keystore_bootstrap,
                                     fake_random_salt_generator_);

  EXPECT_FALSE(handler2.NeedKeystoreKey());
}

// Ensure GetKeystoreDecryptor only updates the keystore decryptor token if it
// wasn't already set properly. Otherwise, the decryptor should remain the
// same.
TEST_F(SyncEncryptionHandlerImplTest, GetKeystoreDecryptor) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &encrypted));
  std::string serialized = encrypted.SerializeAsString();
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &encrypted));
  EXPECT_EQ(serialized, encrypted.SerializeAsString());
}

// Test that we don't attempt to migrate while an implicit passphrase is pending
// and that once we do decrypt pending keys we migrate the nigori. Once
// migrated, we should be in keystore passphrase state.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnDecryptImplicitPass) {
  const char kOtherKey[] = "other";

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    DirectoryCryptographer other_cryptographer;
    KeyParams other_key = {KeyDerivationParams::CreateForPbkdf2(), kOtherKey};
    other_cryptographer.AddKey(other_key);

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(false);
    nigori.set_encrypt_everything(false);
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOtherKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kOtherKey,
                       /*key_derivation_params=*/base::nullopt);
}

// Test that we don't attempt to migrate while a custom passphrase is pending,
// and that once we do decrypt pending keys we migrate the nigori. Once
// migrated, we should be in custom passphrase state with encrypt everything.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnDecryptCustomPass) {
  const char kOtherKey[] = "other";

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    DirectoryCryptographer other_cryptographer;
    KeyParams other_key = {KeyDerivationParams::CreateForPbkdf2(), kOtherKey};
    other_cryptographer.AddKey(other_key);

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(2);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOtherKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  const base::Time migration_time =
      encryption_handler()->GetKeystoreMigrationTime();
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kOtherKey,
                       {KeyDerivationParams::CreateForPbkdf2()});

  VerifyRestoreAfterExplicitPaspshrase(
      TimeToProtoTime(migration_time), kOtherKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Test that we trigger a migration when we set the keystore key, had an
// implicit passphrase, and did not have encrypt everything. We should switch
// to PassphraseType::kKeystorePassphrase.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnKeystoreKeyAvailableImplicit) {
  const char kCurKey[] = "cur";
  KeyParams current_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  GetCryptographer()->AddKey(current_key);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  Mock::VerifyAndClearExpectations(observer());

  // Once we provide a keystore key, we should perform the migration.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kCurKey,
                       /*key_derivation_params=*/base::nullopt);
}

// Test that we trigger a migration when we set the keystore key, had an
// implicit passphrase, and encrypt everything enabled. We should switch to
// PassphraseType::kFrozenImplicitPassphrase.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableFrozenImplicit) {
  const char kCurKey[] = "cur";
  KeyParams current_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  GetCryptographer()->AddKey(current_key);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->EnableEncryptEverything();

  // Once we provide a keystore key, we should perform the migration.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  EXPECT_CALL(*observer(), OnPassphraseTypeChanged(
                               PassphraseType::kFrozenImplicitPassphrase, _));

  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  const base::Time migration_time =
      encryption_handler()->GetKeystoreMigrationTime();
  VerifyPassphraseType(PassphraseType::kFrozenImplicitPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kFrozenImplicitPassphrase, kCurKey,
                       /*key_derivation_params=*/base::nullopt);

  // We need the passphrase bootstrap token, but OnBootstrapTokenUpdated(_,
  // PASSPHRASE_BOOTSTRAP_TOKEN) has not been invoked (because it was invoked
  // during a previous instance) so get it from the Cryptographer.
  std::string passphrase_bootstrap_token;
  GetCryptographer()->GetBootstrapToken(encryptor_,
                                        &passphrase_bootstrap_token);
  VerifyRestoreAfterExplicitPaspshrase(
      TimeToProtoTime(migration_time), kCurKey, passphrase_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kFrozenImplicitPassphrase,
      /*key_derivation_method=*/base::nullopt);
}

// Test that we trigger a migration when we set the keystore key, had a
// custom passphrase, and encrypt everything enabled. The passphrase state
// should remain as CUSTOM_PASSPHRASE, and encrypt everything stay the same.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableCustomWithEncryption) {
  const char kCurKey[] = "cur";
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  encryption_handler()->Init();
  encryption_handler()->SetEncryptionPassphrase(kCurKey);
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->EnableEncryptEverything();
  Mock::VerifyAndClearExpectations(observer());

  sync_pb::NigoriSpecifics captured_nigori_specifics;
  // Once we provide a keystore key, we should perform the migration.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  const base::Time migration_time =
      encryption_handler()->GetKeystoreMigrationTime();
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCurKey,
                       {KeyDerivationParams::CreateForPbkdf2()});

  VerifyRestoreAfterExplicitPaspshrase(
      TimeToProtoTime(migration_time), kCurKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Test that we trigger a migration when we set the keystore key, had a
// custom passphrase, and did not have encrypt everything. The passphrase state
// should remain as PassphraseType::kCustomPassphrase, and encrypt everything
// should be enabled.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableCustomNoEncryption) {
  const char kCurKey[] = "cur";
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  encryption_handler()->Init();
  encryption_handler()->SetEncryptionPassphrase(kCurKey);
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  Mock::VerifyAndClearExpectations(observer());

  // Once we provide a keystore key, we should perform the migration.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  const base::Time migration_time =
      encryption_handler()->GetKeystoreMigrationTime();
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCurKey,
                       {KeyDerivationParams::CreateForPbkdf2()});

  VerifyRestoreAfterExplicitPaspshrase(
      TimeToProtoTime(migration_time), kCurKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Test that we can handle receiving a migrated nigori node in the
// KEYSTORE_PASS state, and use the keystore decryptor token to decrypt the
// keybag.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriKeystorePass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &keystore_decryptor_token));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  EXPECT_FALSE(GetCryptographer()->CanEncrypt());
  {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_NE(encryption_handler()->GetPassphraseType(trans.GetWrappedTrans()),
              PassphraseType::kKeystorePassphrase);
  }
  // Now build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kCurKey and kKeystoreKey.
  sync_pb::NigoriSpecifics nigori;
  nigori.mutable_keystore_decryptor_token()->CopyFrom(keystore_decryptor_token);
  other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
  nigori.set_keybag_is_frozen(true);
  nigori.set_keystore_migration_time(1);
  nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, PassphraseType::kKeystorePassphrase,
                                    kCurKey,
                                    /*key_derivation_method=*/base::nullopt);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  DirectoryCryptographer keystore_cryptographer;
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that we handle receiving migrated nigori's with
// PassphraseType::kFrozenImplicitPassphrase state. We should be in a pending
// key state until
// we supply the pending frozen implicit passphrase key.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriFrozenImplicitPass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(), OnPassphraseTypeChanged(
                                 PassphraseType::kFrozenImplicitPassphrase, _));
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.set_keybag_is_frozen(true);
    nigori.set_passphrase_type(
        sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    nigori.set_encrypt_everything(true);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kFrozenImplicitPassphrase);
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyMigratedNigoriWithTimestamp(
      1, PassphraseType::kFrozenImplicitPassphrase, kCurKey,
      /*key_derivation_method=*/base::nullopt);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  DirectoryCryptographer keystore_cryptographer;
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that we handle receiving migrated nigori's with
// PassphraseType::kCustomPassphrase state. We should be in a pending key state
// until we
// provide the custom passphrase key.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriCustomPass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.set_keybag_is_frozen(true);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    nigori.set_encrypt_everything(true);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyMigratedNigoriWithTimestamp(1, PassphraseType::kCustomPassphrase,
                                    kCurKey,
                                    {KeyDerivationParams::CreateForPbkdf2()});

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  DirectoryCryptographer keystore_cryptographer;
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that if we have a migrated nigori with a custom passphrase, then receive
// and old implicit passphrase nigori, we properly overwrite it with the current
// state.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveUnmigratedNigoriAfterMigration) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {KeyDerivationParams::CreateForPbkdf2(), kOldKey};
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  // Build a migrated nigori with full encryption.
  const int64_t migration_time = 1;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    GetCryptographer()->GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_encrypt_everything(true);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true)).Times(2);
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(migration_time,
                                    PassphraseType::kCustomPassphrase, kCurKey,
                                    {KeyDerivationParams::CreateForPbkdf2()});

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  Mock::VerifyAndClearExpectations(observer());

  // Now build an old unmigrated nigori node with old encrypted types. We should
  // properly overwrite it with the migrated + encrypt everything state.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    DirectoryCryptographer other_cryptographer;
    other_cryptographer.AddKey(old_key);
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(false);
    nigori.set_encrypt_everything(false);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, PassphraseType::kCustomPassphrase,
                                    kCurKey,
                                    {KeyDerivationParams::CreateForPbkdf2()});

  // We need the passphrase bootstrap token, but OnBootstrapTokenUpdated(_,
  // PASSPHRASE_BOOTSTRAP_TOKEN) has not been invoked (because it was invoked
  // during a previous instance) so get it from the Cryptographer.
  std::string passphrase_bootstrap_token;
  GetCryptographer()->GetBootstrapToken(encryptor_,
                                        &passphrase_bootstrap_token);
  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kCurKey, passphrase_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Test that if we have a migrated nigori with a custom passphrase, then receive
// a migrated nigori with a keystore passphrase, we properly overwrite it with
// the current state.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveOldMigratedNigori) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {KeyDerivationParams::CreateForPbkdf2(), kOldKey};
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  // Build a migrated nigori with full encryption.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    GetCryptographer()->GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_encrypt_everything(true);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true)).Times(2);
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, PassphraseType::kCustomPassphrase,
                                    kCurKey,
                                    {KeyDerivationParams::CreateForPbkdf2()});

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  Mock::VerifyAndClearExpectations(observer());

  // Now build an old keystore nigori node with old encrypted types. We should
  // properly overwrite it with the migrated + encrypt everything state.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  const int64_t migration_time = 1;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    DirectoryCryptographer other_cryptographer;
    other_cryptographer.AddKey(old_key);
    encryption_handler()->GetKeystoreDecryptor(
        other_cryptographer, kKeystoreKey,
        nigori.mutable_keystore_decryptor_token());
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori.set_keystore_migration_time(migration_time);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(migration_time,
                                    PassphraseType::kCustomPassphrase, kCurKey,
                                    {KeyDerivationParams::CreateForPbkdf2()});

  // We need the passphrase bootstrap token, but OnBootstrapTokenUpdated(_,
  // PASSPHRASE_BOOTSTRAP_TOKEN) has not been invoked (because it was invoked
  // during a previous instance) so get it from the Cryptographer.
  std::string passphrase_bootstrap_token;
  GetCryptographer()->GetBootstrapToken(encryptor_,
                                        &passphrase_bootstrap_token);
  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kCurKey, passphrase_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Test that if we receive the keystore key after receiving a migrated nigori
// node, we properly use the keystore decryptor token to decrypt the keybag.
TEST_F(SyncEncryptionHandlerImplTest, SetKeystoreAfterReceivingMigratedNigori) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &keystore_decryptor_token));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  EXPECT_FALSE(GetCryptographer()->CanEncrypt());
  {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_NE(encryption_handler()->GetPassphraseType(trans.GetWrappedTrans()),
              PassphraseType::kKeystorePassphrase);
  }
  // Now build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kCurKey and kKeystoreKey.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

    EXPECT_CALL(*observer(), OnPassphraseTypeChanged(
                                 PassphraseType::kKeystorePassphrase, _));
    EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, PassphraseType::kKeystorePassphrase,
                                    kCurKey,
                                    /*key_derivation_method=*/base::nullopt);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  DirectoryCryptographer keystore_cryptographer;
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that after receiving a migrated nigori and decrypting it using the
// keystore key, we can then switch to a custom passphrase. The nigori should
// remain migrated and encrypt everything should be enabled.
TEST_F(SyncEncryptionHandlerImplTest, SetCustomPassAfterMigration) {
  const char kOldKey[] = "old";
  sync_pb::EncryptedData keystore_decryptor_token;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kOldKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kOldKey and kKeystoreKey.
  const int64_t migration_time = 1;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(migration_time);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  const char kNewKey[] = "new_key";
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(2);
  encryption_handler()->SetEncryptionPassphrase(kNewKey);
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_FALSE(captured_bootstrap_token.empty());
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  VerifyMigratedNigoriWithTimestamp(
      migration_time, PassphraseType::kCustomPassphrase, kNewKey,
      {KeyDerivationParams::CreateForScrypt(kScryptSalt)});

  // Check that the cryptographer can decrypt the old key.
  sync_pb::EncryptedData old_encrypted;
  other_cryptographer.EncryptString("string", &old_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(old_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  DirectoryCryptographer keystore_cryptographer;
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  // Check that the cryptographer is encrypting with the new key.
  KeyParams new_key = {KeyDerivationParams::CreateForScrypt(kScryptSalt),
                       kNewKey};
  DirectoryCryptographer new_cryptographer;
  new_cryptographer.AddKey(new_key);
  sync_pb::EncryptedData new_encrypted;
  new_cryptographer.EncryptString("string", &new_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(new_encrypted));

  // Now verify that we can restore the current state using the captured
  // bootstrap token and nigori state.
  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kNewKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForScrypt(kScryptSalt)});
}

// Test that if a client without a keystore key (e.g. one without keystore
// encryption enabled) receives a migrated nigori and then attempts to set a
// custom passphrase, it also enables encrypt everything. The nigori node
// should remain migrated.
TEST_F(SyncEncryptionHandlerImplTest,
       SetCustomPassAfterMigrationNoKeystoreKey) {
  const char kOldKey[] = "old";
  sync_pb::EncryptedData keystore_decryptor_token;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kOldKey};
  other_cryptographer.AddKey(cur_key);
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  other_cryptographer.AddNonDefaultKey(keystore_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer will have
  // pending keys until we provide the decryption passphrase.
  const int64_t migration_time = 1;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(migration_time);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->SetDecryptionPassphrase(kOldKey);
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  const char kNewKey[] = "new_key";
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase, _));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(2);
  encryption_handler()->SetEncryptionPassphrase(kNewKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  VerifyMigratedNigoriWithTimestamp(
      migration_time, PassphraseType::kCustomPassphrase, kNewKey,
      {KeyDerivationParams::CreateForScrypt(kScryptSalt)});

  // Check that the cryptographer can decrypt the old key.
  sync_pb::EncryptedData old_encrypted;
  other_cryptographer.EncryptString("string", &old_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(old_encrypted));

  // Check that the cryptographer can still decrypt keystore key based
  // encryption (should have been extracted from the encryption keybag).
  DirectoryCryptographer keystore_cryptographer;
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  // Check that the cryptographer is encrypting with the new key.
  KeyParams new_key = {KeyDerivationParams::CreateForScrypt(kScryptSalt),
                       kNewKey};
  DirectoryCryptographer new_cryptographer;
  new_cryptographer.AddKey(new_key);
  sync_pb::EncryptedData new_encrypted;
  new_cryptographer.EncryptString("string", &new_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(new_encrypted));

  // Now verify that we can restore the current state using the captured
  // bootstrap token and nigori state.
  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kNewKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForScrypt(kScryptSalt)});
}

// Test that if a client without a keystore key (e.g. one without keystore
// encryption enabled) receives a migrated nigori in keystore passphrase state
// and then attempts to enable encrypt everything, we switch to a custom
// passphrase. The nigori should remain migrated.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnEncryptEverythingKeystorePassphrase) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  DirectoryCryptographer other_cryptographer;
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  other_cryptographer.AddKey(cur_key);
  KeyParams keystore_key = {KeyDerivationParams::CreateForPbkdf2(),
                            kKeystoreKey};
  other_cryptographer.AddNonDefaultKey(keystore_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer, kKeystoreKey, &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer will have
  // pending keys until we provide the decryption passphrase.
  const int64_t migration_time = 1;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(migration_time);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnPassphraseTypeChanged(
                               PassphraseType::kFrozenImplicitPassphrase, _));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  encryption_handler()->EnableEncryptEverything();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kFrozenImplicitPassphrase);
  EXPECT_TRUE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(
      1, PassphraseType::kFrozenImplicitPassphrase, kCurKey,
      /*key_derivation_method=*/base::nullopt);

  // Check that the cryptographer is encrypting using the frozen current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can still decrypt keystore key based
  // encryption (due to extracting the keystore key from the encryption keybag).
  DirectoryCryptographer keystore_cryptographer;
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kCurKey, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kFrozenImplicitPassphrase,
      /*key_derivation_method=*/base::nullopt);
}

// If we receive a nigori migrated and with a  KEYSTORE_PASSPHRASE type, but
// using an old default key (i.e. old GAIA password), we should overwrite the
// nigori, updating the keybag and keystore decryptor.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriWithOldPassphrase) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {KeyDerivationParams::CreateForPbkdf2(), kOldKey};
  KeyParams cur_key = {KeyDerivationParams::CreateForPbkdf2(), kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  DirectoryCryptographer other_cryptographer;
  other_cryptographer.AddKey(old_key);
  EXPECT_TRUE(other_cryptographer.CanEncrypt());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kCurKey,
                       /*key_derivation_params=*/base::nullopt);

  // Now build an old keystore passphrase nigori node.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitTypeRoot(NIGORI), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    DirectoryCryptographer other_cryptographer;
    other_cryptographer.AddKey(old_key);
    encryption_handler()->GetKeystoreDecryptor(
        other_cryptographer, kKeystoreKey,
        nigori.mutable_keystore_decryptor_token());
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kCurKey,
                       /*key_derivation_params=*/base::nullopt);
}

// Trigger a key rotation upon receiving new keys if we already had a keystore
// migrated nigori with the gaia key as the default (still in backwards
// compatible mode).
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysGaiaDefault) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  SetupKeystoreKeys({kRawOldKeystoreKey});

  // Then init the nigori node with a backwards compatible set of keys.
  InitAndVerifyKeystoreMigratedNigori(1, kOldGaiaKey, old_keystore_key);

  // Now set some new keystore keys.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  SetupKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_method=*/base::nullopt);
}

// Trigger a key rotation upon receiving new keys if we already had a keystore
// migrated nigori with the keystore key as the default.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysKeystoreDefault) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  SetupKeystoreKeys({kRawOldKeystoreKey});

  // Then init the nigori node with a non-backwards compatible set of keys.
  InitAndVerifyKeystoreMigratedNigori(1, old_keystore_key, old_keystore_key);

  // Now set some new keystore keys.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  SetupKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});
  // Pump for any posted tasks.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_method=*/base::nullopt);
}

// Trigger a key rotation upon when a pending gaia passphrase is resolved.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysAfterPendingGaiaResolved) {
  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  InitAndVerifyUnmigratedNigori(kOldGaiaKey,
                                PassphraseType::kImplicitPassphrase);

  // Pass multiple keystore keys, signaling a rotation has happened.
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Resolve the pending keys. This should trigger the key rotation.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AtLeast(1));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOldGaiaKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_method=*/base::nullopt);
}

// When signing in for the first time, make sure we can rotate keys if we
// already have a keystore migrated nigori.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysGaiaDefaultOnInit) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  // Set two keys, signaling that a rotation has been performed. No Nigori
  // node is present yet, so we can't rotate.
  SetupKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  InitAndVerifyKeystoreMigratedNigori(1, kOldGaiaKey, old_keystore_key);

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_FALSE(encryption_handler()->IsEncryptEverythingEnabled());
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_method=*/base::nullopt);
}

// Trigger a key rotation when a migrated nigori (with an old keystore key) is
// applied.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysWhenMigratedNigoriArrives) {
  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  InitAndVerifyUnmigratedNigori(kOldGaiaKey,
                                PassphraseType::kImplicitPassphrase);

  // Pass multiple keystore keys, signaling a rotation has happened.
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Now simulate downloading a nigori node that was migrated before the
  // keys were rotated, and hence still encrypt with the old gaia key.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AtLeast(1));
  {
    sync_pb::NigoriSpecifics nigori = BuildMigratedNigori(
        PassphraseType::kKeystorePassphrase, 1,
        sync_pb::NigoriSpecifics::UNSPECIFIED, kOldGaiaKey, old_keystore_key,
        /* key_derivation_salt = */ base::nullopt);
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  PumpLoop();

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  VerifyPassphraseType(PassphraseType::kKeystorePassphrase);
  VerifyMigratedNigori(PassphraseType::kKeystorePassphrase, kKeystoreKey,
                       /*key_derivation_method=*/base::nullopt);
}

// Verify that performing a migration while having more than one keystore key
// preserves a custom passphrase.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysUnmigratedCustomPassphrase) {
  const char kCustomPass[] = "custom_passphrase";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  InitAndVerifyUnmigratedNigori(kCustomPass, PassphraseType::kCustomPassphrase);

  // Pass multiple keystore keys, signaling a rotation has happened.
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Pass the decryption passphrase. This will also trigger the migration,
  // but should not overwrite the default key.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, true));
  sync_pb::NigoriSpecifics captured_nigori_specifics;
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  EXPECT_CALL(*observer(), OnEncryptionComplete()).Times(AnyNumber());
  std::string captured_bootstrap_token;
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(testing::SaveArg<0>(&captured_bootstrap_token));
  encryption_handler()->SetDecryptionPassphrase(kCustomPass);
  Mock::VerifyAndClearExpectations(observer());

  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCustomPass,
                       {KeyDerivationParams::CreateForPbkdf2()});

  const base::Time migration_time =
      encryption_handler()->GetKeystoreMigrationTime();
  VerifyRestoreAfterExplicitPaspshrase(
      TimeToProtoTime(migration_time), kCustomPass, captured_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Verify that a key rotation done after we've migrated a custom passphrase
// nigori node preserves the custom passphrase.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysMigratedCustomPassphrase) {
  const char kCustomPass[] = "custom_passphrase";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  KeyParams custom_key = {KeyDerivationParams::CreateForPbkdf2(), kCustomPass};
  GetCryptographer()->AddKey(custom_key);

  const int64_t migration_time = 1;
  InitAndVerifyCustomPassphraseMigratedNigori(
      migration_time, sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
      kCustomPass, /* key_derivation_salt = */ base::nullopt);
  VerifyMigratedNigoriWithTimestamp(
      migration_time, PassphraseType::kCustomPassphrase, kCustomPass,
      {KeyDerivationParams::CreateForPbkdf2()});

  sync_pb::NigoriSpecifics captured_nigori_specifics;
  // Pass multiple keystore keys, signaling a rotation has happened.
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnLocalSetPassphraseEncryption(_))
      .WillOnce(testing::SaveArg<0>(&captured_nigori_specifics));
  encryption_handler()->SetKeystoreKeys({kRawOldKeystoreKey, kRawKeystoreKey});

  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  VerifyMigratedNigoriWithTimestamp(
      migration_time, PassphraseType::kCustomPassphrase, kCustomPass,
      {KeyDerivationParams::CreateForPbkdf2()});

  // We need the passphrase bootstrap token, but OnBootstrapTokenUpdated(_,
  // PASSPHRASE_BOOTSTRAP_TOKEN) has not been invoked (because it was invoked
  // during a previous instance) so get it from the Cryptographer.
  std::string passphrase_bootstrap_token;
  GetCryptographer()->GetBootstrapToken(encryptor_,
                                        &passphrase_bootstrap_token);
  VerifyRestoreAfterExplicitPaspshrase(
      migration_time, kCustomPass, passphrase_bootstrap_token,
      captured_nigori_specifics, PassphraseType::kCustomPassphrase,
      {KeyDerivationParams::CreateForPbkdf2()});
}

// Verify that the client can gracefully handle a nigori node that is missing
// the keystore migration time field.
TEST_F(SyncEncryptionHandlerImplTest, MissingKeystoreMigrationTime) {
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  Mock::VerifyAndClearExpectations(observer());

  // Now simulate downloading a nigori node that that is missing the keystore
  // migration time. It should be interpreted properly, and the passphrase type
  // should switch to keystore passphrase.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, _));
  {
    sync_pb::NigoriSpecifics nigori = BuildMigratedNigori(
        PassphraseType::kKeystorePassphrase, 1,
        sync_pb::NigoriSpecifics::UNSPECIFIED, kKeystoreKey, kKeystoreKey,
        /* key_derivation_salt = */ base::nullopt);
    nigori.clear_keystore_migration_time();
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now provide the keystore key to fully initialize the cryptographer.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetKeystoreKeys({kRawKeystoreKey});
}

// When we receive a remote Nigori with UNSPECIFIED as the key derivation
// method, it implies data encrypted using an old version (<M70) which does not
// know about the key derivation method field but always uses PBKDF2.
// Initializing the encryption handler should set it to PBKDF2 explicitly.
TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldSetPbkdf2WithCustomPassphraseWhenUnspecified) {
  KeyParams custom_key = {KeyDerivationParams::CreateForPbkdf2(),
                          kCustomPassphrase};
  GetCryptographer()->AddKey(custom_key);

  IgnoreAllObserverCalls();
  InitCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::UNSPECIFIED,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);

  sync_pb::NigoriSpecifics nigori = ReadNigoriSpecifics();
  ASSERT_TRUE(nigori.has_custom_passphrase_key_derivation_method());
  EXPECT_EQ(sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
            nigori.custom_passphrase_key_derivation_method());
}

// This tests behavior that happens when Init calls ApplyNigoriUpdateImpl.
// Generally, in order not to repeat the tests, we are testing only this case.
// As long as this test passes (implying Init calls ApplyNigoriUpdateImpl) and
// ApplyNigoriUpdate tests pass, Init behaves correctly.
TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldPassPbkdf2ToObserverWhenUnspecified) {
  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(), OnPassphraseRequired(
                               _, KeyDerivationParams::CreateForPbkdf2(), _));
  InitCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::UNSPECIFIED,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);

  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldReportPbkdf2InHistogramWhenPbkdf2Persisted) {
  KeyParams custom_key = {KeyDerivationParams::CreateForPbkdf2(),
                          kCustomPassphrase};
  GetCryptographer()->AddKey(custom_key);

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  InitCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodStateOnStartup",
      /*sample=*/
      ExpectedKeyDerivationMethodStateForMetrics::PBKDF2_HMAC_SHA1_1003,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldReportPbkdf2InHistogramWhenUnspecifiedKeyMethodPersisted) {
  KeyParams custom_key = {KeyDerivationParams::CreateForPbkdf2(),
                          kCustomPassphrase};
  GetCryptographer()->AddKey(custom_key);

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  InitCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::UNSPECIFIED,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodStateOnStartup",
      /*sample=*/
      ExpectedKeyDerivationMethodStateForMetrics::PBKDF2_HMAC_SHA1_1003,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldReportScryptInHistogramWhenScryptPersisted) {
  KeyParams custom_key = {KeyDerivationParams::CreateForScrypt(kScryptSalt),
                          kCustomPassphrase};
  GetCryptographer()->AddKey(custom_key);

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  InitCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, /*key_derivation_salt=*/kScryptSalt);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodStateOnStartup",
      /*sample=*/ExpectedKeyDerivationMethodStateForMetrics::SCRYPT_8192_8_11,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       InitShouldNotReportKeyMethodInHistogramIfNotCustomPassphrase) {
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});

  base::HistogramTester histogram_tester;
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  histogram_tester.ExpectTotalCount(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodStateOnStartup",
      /*count=*/0);
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetEncryptionPassphraseShouldUsePbkdf2IfScryptForNewDisabled) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/false);
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  IgnoreAllObserverCalls();
  encryption_handler()->SetEncryptionPassphrase(kCustomPassphrase);

  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCustomPassphrase,
                       {KeyDerivationParams::CreateForPbkdf2()});
  EXPECT_EQ(GetSerializedNigoriKeyForCustomPassphrase(
                KeyDerivationParams::CreateForPbkdf2(), kCustomPassphrase),
            GetCryptographer()->GetDefaultNigoriKeyData());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetEncryptionPassphraseShouldReportPbkdf2InHistogramWhenPbkdf2Used) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/false);
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  encryption_handler()->SetEncryptionPassphrase(kCustomPassphrase);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodOnNewPassphrase",
      /*sample=*/
      ExpectedKeyDerivationMethodStateForMetrics::PBKDF2_HMAC_SHA1_1003,
      /*count=*/1);
}

// Regardless of the state of the "scrypt for new passphrases" feature, turning
// on the "force-disable scrypt" should lead to using PBKDF2.
TEST_F(SyncEncryptionHandlerImplTest,
       SetEncryptionPassphraseShouldUsePbkdf2IfScryptForceDisabled) {
  SetScryptFeaturesState(/*force_disabled=*/true,
                         /*use_for_new_passphrases=*/true);
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  IgnoreAllObserverCalls();
  encryption_handler()->SetEncryptionPassphrase(kCustomPassphrase);

  // Nigori should contain PBKDF2 as the key derivation method.
  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCustomPassphrase,
                       {KeyDerivationParams::CreateForPbkdf2()});
  // The key added to the cryptographer should have been derived using PBKDF2.
  EXPECT_EQ(GetSerializedNigoriKeyForCustomPassphrase(
                KeyDerivationParams::CreateForPbkdf2(), kCustomPassphrase),
            GetCryptographer()->GetDefaultNigoriKeyData());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetEncryptionPassphraseShouldUseScryptIfScryptForNewEnabled) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/true);
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  IgnoreAllObserverCalls();
  encryption_handler()->SetEncryptionPassphrase(kCustomPassphrase);

  std::string salt = fake_random_salt_generator_.Run();
  // Nigori should contain scrypt as the key derivation method.
  VerifyMigratedNigori(PassphraseType::kCustomPassphrase, kCustomPassphrase,
                       {KeyDerivationParams::CreateForScrypt(salt)});
  // The key added to the cryptographer should have been derived using scrypt
  // with the proper salt.
  EXPECT_EQ(GetSerializedNigoriKeyForCustomPassphrase(
                KeyDerivationParams::CreateForScrypt(salt), kCustomPassphrase),
            GetCryptographer()->GetDefaultNigoriKeyData());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetEncryptionPassphraseShouldReportScryptInHistogramWhenScryptUsed) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/true);
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(/*migration_time=*/1, kRawKeystoreKey,
                                      kKeystoreKey);

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  encryption_handler()->SetEncryptionPassphrase(kCustomPassphrase);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodOnNewPassphrase",
      /*sample=*/ExpectedKeyDerivationMethodStateForMetrics::SCRYPT_8192_8_11,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldUsePbkdf2WhenUnspecifiedInNigori) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::UNSPECIFIED,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  EXPECT_EQ(GetSerializedNigoriKeyForCustomPassphrase(
                KeyDerivationParams::CreateForPbkdf2(), kCustomPassphrase),
            GetCryptographer()->GetDefaultNigoriKeyData());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldUsePbkdf2FromNigori) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  EXPECT_EQ(GetSerializedNigoriKeyForCustomPassphrase(
                KeyDerivationParams::CreateForPbkdf2(), kCustomPassphrase),
            GetCryptographer()->GetDefaultNigoriKeyData());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
}

// If we receive data encrypted using a key derivation method that we don't know
// about from a future version, we should reject the passphrase and signal this
// to the observer when SetDecryptionPassphrase is called.
TEST_F(
    SyncEncryptionHandlerImplTest,
    SetDecryptionPassphraseShouldRejectPassphraseOnUnsupportedNigoriKeyMethod) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, kUnsupportedKeyDerivationMethod, kCustomPassphrase,
      /*key_derivation_salt=*/base::nullopt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(
                  _, KeyDerivationParams::CreateWithUnsupportedMethod(), _));
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  // Verify that the cryptographer has not been initialized, i.e. no keys have
  // been added.
  EXPECT_FALSE(GetCryptographer()->is_initialized());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldUseScryptFromNigoriWhenScryptEnabled) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/false);

  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, {kScryptSalt});
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  EXPECT_EQ(
      GetSerializedNigoriKeyForCustomPassphrase(
          KeyDerivationParams::CreateForScrypt(kScryptSalt), kCustomPassphrase),
      GetCryptographer()->GetDefaultNigoriKeyData());
  EXPECT_TRUE(GetCryptographer()->CanEncrypt());
}

// If scrypt support is explicitly disabled, we should treat it exactly as an
// unsupported method.
TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldNotUseScryptFromNigoriWhenScryptDisabled) {
  SetScryptFeaturesState(/*force_disabled=*/true,
                         /*use_for_new_passphrases=*/false);

  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, {kScryptSalt});
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(
                  _, KeyDerivationParams::CreateWithUnsupportedMethod(), _));
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  EXPECT_FALSE(GetCryptographer()->CanEncrypt());
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldReportPersistedPbkdf2InHistogramOnSuccess) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
      kCustomPassphrase, /*key_derivation_salt=*/base::nullopt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodOnSuccessfulDecryption",
      /*sample=*/
      ExpectedKeyDerivationMethodStateForMetrics::PBKDF2_HMAC_SHA1_1003,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldReportPersistedScryptInHistogramOnSuccess) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, /*key_derivation_salt=*/kScryptSalt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  encryption_handler()->SetDecryptionPassphrase(kCustomPassphrase);

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodOnSuccessfulDecryption",
      /*sample=*/ExpectedKeyDerivationMethodStateForMetrics::SCRYPT_8192_8_11,
      /*count=*/1);
}

TEST_F(SyncEncryptionHandlerImplTest,
       SetDecryptionPassphraseShouldNotReportKeyMethodInHistogramOnFailure) {
  InitAndVerifyCustomPassphraseMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, /*key_derivation_salt=*/kScryptSalt);
  GetCryptographer()->SetPendingKeys(ReadNigoriSpecifics().encryption_keybag());

  IgnoreAllObserverCalls();
  base::HistogramTester histogram_tester;
  encryption_handler()->SetDecryptionPassphrase(
      "Invalid passphrase, not the same as kCustomPassphrase");

  histogram_tester.ExpectTotalCount(
      "Sync.Crypto.CustomPassphraseKeyDerivationMethodOnSuccessfulDecryption",
      /*count=*/0);
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldPassPbkdf2ToObserverWhenUnspecified) {
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      0, sync_pb::NigoriSpecifics::UNSPECIFIED, kCustomPassphrase,
      /*key_derivation_salt=*/base::nullopt);
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(), OnPassphraseRequired(
                               _, KeyDerivationParams::CreateForPbkdf2(), _));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldPassPbkdf2ToObserver) {
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      0, sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003, kCustomPassphrase,
      /*key_derivation_salt=*/base::nullopt);
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(), OnPassphraseRequired(
                               _, KeyDerivationParams::CreateForPbkdf2(), _));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldPassScryptToObserverWhenEnabled) {
  SetScryptFeaturesState(/*force_disabled=*/false,
                         /*use_for_new_passphrases=*/false);

  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, {kScryptSalt});
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(
                  _, KeyDerivationParams::CreateForScrypt(kScryptSalt), _));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldPassScryptAsUnsupportedToObserverWhenDisabled) {
  SetScryptFeaturesState(/*force_disabled=*/true,
                         /*use_for_new_passphrases=*/false);

  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      /*migration_time=*/1, sync_pb::NigoriSpecifics::SCRYPT_8192_8_11,
      kCustomPassphrase, {kScryptSalt});
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(
                  _, KeyDerivationParams::CreateWithUnsupportedMethod(), _));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldPropagateUnsupportedKeyMethodToObserver) {
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      0, kUnsupportedKeyDerivationMethod, kCustomPassphrase,
      /*key_derivation_salt=*/base::nullopt);
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(
                  _, KeyDerivationParams::CreateWithUnsupportedMethod(), _));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldSetPbkdf2WithCustomPassphraseWhenUnspecified) {
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  SetupKeystoreKeys({kRawKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori = BuildCustomPassMigratedNigori(
      0, sync_pb::NigoriSpecifics::UNSPECIFIED, kCustomPassphrase,
      /*key_derivation_salt=*/base::nullopt);
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();

  sync_pb::NigoriSpecifics nigori = ReadNigoriSpecifics();
  ASSERT_TRUE(nigori.has_custom_passphrase_key_derivation_method());
  EXPECT_EQ(sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003,
            nigori.custom_passphrase_key_derivation_method());
}

TEST_F(SyncEncryptionHandlerImplTest,
       ApplyNigoriUpdateShouldNotSetKeyMethodWithKeystorePassphrase) {
  const char kRawNewKeystoreKey[] = "new_keystore_key";
  std::string new_keystore_key;
  base::Base64Encode(kRawNewKeystoreKey, &new_keystore_key);

  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();
  // Set up both keys right away so that the new keystore can be decrypted.
  SetupKeystoreKeys({kRawKeystoreKey, kRawNewKeystoreKey});
  InitAndVerifyKeystoreMigratedNigori(1, kRawKeystoreKey, kKeystoreKey);
  sync_pb::NigoriSpecifics new_nigori =
      BuildKeystoreMigratedNigori(0, kRawNewKeystoreKey, new_keystore_key);
  WriteNigori(new_nigori);

  IgnoreAllObserverCalls();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(new_nigori,
                                            trans.GetWrappedTrans());
  }
  PumpLoop();

  sync_pb::NigoriSpecifics nigori = ReadNigoriSpecifics();
  EXPECT_FALSE(nigori.has_custom_passphrase_key_derivation_method());
}

}  // namespace syncer
