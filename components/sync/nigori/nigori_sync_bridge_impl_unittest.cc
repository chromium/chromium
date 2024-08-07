// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori_state.h"
#include "components/sync/nigori/nigori_storage.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/test/nigori_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::Not;
using testing::NotNull;
using testing::Return;

NigoriMetadataBatch CreateFakeNigoriMetadataBatch(
    const std::string& progress_marker_token,
    int64_t entity_metadata_sequence_number);

MATCHER(NullTime, "") {
  return arg.is_null();
}

MATCHER_P(HasDefaultKeyDerivedFrom, key_params, "") {
  const Cryptographer& cryptographer = arg;
  std::unique_ptr<Nigori> expected_default_nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  return cryptographer.GetDefaultEncryptionKeyName() ==
         expected_default_nigori->GetKeyName();
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

MATCHER_P2(HasPublicKeyVersionAndValue, key_version, key_value, "") {
  const std::unique_ptr<EntityData>& entity_data = arg;
  if (!entity_data || !entity_data->specifics.has_nigori()) {
    return false;
  }
  const sync_pb::NigoriSpecifics& specifics = entity_data->specifics.nigori();
  return specifics.has_cross_user_sharing_public_key() &&
         specifics.cross_user_sharing_public_key().version() ==
             (int32_t)key_version &&
         specifics.cross_user_sharing_public_key().x25519_public_key() ==
             key_value;
}

MATCHER_P(HasPublicKeyVersion, key_version, "") {
  const std::unique_ptr<EntityData>& entity_data = arg;
  if (!entity_data || !entity_data->specifics.has_nigori()) {
    return false;
  }
  const sync_pb::NigoriSpecifics& specifics = entity_data->specifics.nigori();
  return specifics.has_cross_user_sharing_public_key() &&
         specifics.cross_user_sharing_public_key().version() == key_version;
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
  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori->GetKeyName());
  encrypted.set_blob(nigori->Encrypt(unencrypted));

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

MATCHER(IsEmptyMetadataBatch, "") {
  const NigoriMetadataBatch& given = arg;
  return given.entity_metadata == std::nullopt;
}

MATCHER_P2(IsFakeNigoriMetadataBatchWithTokenAndSequenceNumber,
           expected_token,
           expected_sequence_number,
           "") {
  const NigoriMetadataBatch& given = arg;
  NigoriMetadataBatch expected =
      CreateFakeNigoriMetadataBatch(expected_token, expected_sequence_number);
  if (given.data_type_state.SerializeAsString() !=
      expected.data_type_state.SerializeAsString()) {
    return false;
  }
  if (!given.entity_metadata.has_value()) {
    return !expected.entity_metadata.has_value();
  }
  return given.entity_metadata->SerializeAsString() ==
         expected.entity_metadata->SerializeAsString();
}

CrossUserSharingKeys CreateNewCrossUserSharingKeys() {
  const uint32_t kKeyVersion = 0;
  CrossUserSharingKeys cross_user_sharing_keys =
      CrossUserSharingKeys::CreateEmpty();
  cross_user_sharing_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), kKeyVersion);
  return cross_user_sharing_keys;
}

NigoriMetadataBatch CreateFakeNigoriMetadataBatch(
    const std::string& progress_marker_token,
    int64_t entity_metadata_sequence_number) {
  NigoriMetadataBatch metadata_batch;
  metadata_batch.data_type_state.mutable_progress_marker()->set_token(
      progress_marker_token);
  metadata_batch.data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  metadata_batch.entity_metadata = sync_pb::EntityMetadata::default_instance();
  metadata_batch.entity_metadata->set_sequence_number(
      entity_metadata_sequence_number);
  return metadata_batch;
}

std::unique_ptr<Nigori> MakeNigoriKey(const KeyParamsForTesting& key_params) {
  return Nigori::CreateByDerivation(key_params.derivation_params,
                                    key_params.password);
}

KeyDerivationParams MakeCustomPassphraseKeyDerivationParams() {
  return KeyDerivationParams::CreateForScrypt("salt");
}

class MockNigoriLocalChangeProcessor : public NigoriLocalChangeProcessor {
 public:
  MockNigoriLocalChangeProcessor() = default;
  ~MockNigoriLocalChangeProcessor() override = default;
  MOCK_METHOD(void,
              ModelReadyToSync,
              (NigoriSyncBridge*, NigoriMetadataBatch),
              (override));
  MOCK_METHOD(void, Put, (std::unique_ptr<EntityData>), (override));
  MOCK_METHOD(bool, IsEntityUnsynced, (), (override));
  MOCK_METHOD(NigoriMetadataBatch, GetMetadata, (), (override));
  MOCK_METHOD(void, ReportError, (const ModelError&), (override));
  MOCK_METHOD(base::WeakPtr<DataTypeControllerDelegate>,
              GetControllerDelegate,
              (),
              (override));
  MOCK_METHOD(bool, IsTrackingMetadata, (), (override));
};

// This class is a helper to keep ownership of a mocked processor by providing
// ownership to the forwarding processor.
class ForwardingNigoriLocalChangeProcessor : public NigoriLocalChangeProcessor {
 public:
  explicit ForwardingNigoriLocalChangeProcessor(
      NigoriLocalChangeProcessor* processor)
      : processor_(processor) {}
  ~ForwardingNigoriLocalChangeProcessor() override = default;

  void ModelReadyToSync(NigoriSyncBridge* bridge,
                        NigoriMetadataBatch metadata_batch) override {
    processor_->ModelReadyToSync(bridge, std::move(metadata_batch));
  }

  void Put(std::unique_ptr<EntityData> entity_data) override {
    processor_->Put(std::move(entity_data));
  }

  bool IsEntityUnsynced() override { return processor_->IsEntityUnsynced(); }

  NigoriMetadataBatch GetMetadata() override {
    return processor_->GetMetadata();
  }

  void ReportError(const ModelError& error) override {
    processor_->ReportError(error);
  }

  base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() override {
    return processor_->GetControllerDelegate();
  }

  bool IsTrackingMetadata() override {
    return processor_->IsTrackingMetadata();
  }

 private:
  raw_ptr<NigoriLocalChangeProcessor> processor_;
};

class MockObserver : public SyncEncryptionHandler::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void,
              OnPassphraseRequired,
              (const KeyDerivationParams&, const sync_pb::EncryptedData&),
              (override));
  MOCK_METHOD(void, OnPassphraseAccepted, (), (override));
  MOCK_METHOD(void, OnTrustedVaultKeyRequired, (), (override));
  MOCK_METHOD(void, OnTrustedVaultKeyAccepted, (), (override));
  MOCK_METHOD(void, OnEncryptedTypesChanged, (DataTypeSet, bool), (override));
  MOCK_METHOD(void,
              OnCryptographerStateChanged,
              (Cryptographer*, bool has_pending_keys),
              (override));
  MOCK_METHOD(void,
              OnPassphraseTypeChanged,
              (PassphraseType, base::Time),
              (override));
};

class MockNigoriStorage : public NigoriStorage {
 public:
  MockNigoriStorage() = default;
  ~MockNigoriStorage() override = default;
  MOCK_METHOD(void, StoreData, (const sync_pb::NigoriLocalData&), (override));
  MOCK_METHOD(std::optional<sync_pb::NigoriLocalData>,
              RestoreData,
              (),
              (override));
  MOCK_METHOD(void, ClearData, (), (override));
};

class NigoriSyncBridgeImplTest : public testing::Test {
 protected:
  NigoriSyncBridgeImplTest() {
    Nigori::SetUseScryptCostParameterForTesting(true);

    ON_CALL(processor_, IsTrackingMetadata).WillByDefault(Return(true));
    ON_CALL(processor_, GetMetadata()).WillByDefault([&] {
      return CreateFakeNigoriMetadataBatch(
          "fake_token", /*entity_metadata_sequence_number=*/100);
    });
    InitializeBridge();
  }

  ~NigoriSyncBridgeImplTest() override {
    bridge_->RemoveObserver(&observer_);
    Nigori::SetUseScryptCostParameterForTesting(false);
  }

  void SetUp() override { OSCryptMocker::SetUp(); }
  void TearDown() override { OSCryptMocker::TearDown(); }

  // Mimics the initial sync for Nigori. After the initial sync
  // `nigori_local_data_` persists Nigori local state.
  bool PerformInitialSyncWithNigori(const sync_pb::NigoriSpecifics& specifics) {
    EntityData entity_data;
    *entity_data.specifics.mutable_nigori() = specifics;

    if (!bridge_->SetKeystoreKeys({kRawKeystoreKey})) {
      LOG(ERROR) << "SetKeystoreKeys failed";
      return false;
    }
    std::optional<syncer::ModelError> error =
        bridge_->MergeFullSyncData(std::move(entity_data));
    if (error) {
      LOG(ERROR) << "Data type error during the initial sync: "
                 << error->ToString();
      return false;
    }
    return true;
  }

  // Similar to the above, but uses simplest keystore Nigori.
  bool PerformInitialSyncWithSimpleKeystoreNigori() {
    const KeyParamsForTesting kKeystoreKeyParams =
        KeystoreKeyParamsForTesting(kRawKeystoreKey);
    return PerformInitialSyncWithNigori(BuildKeystoreNigoriSpecifics(
        /*keybag_keys_params=*/{kKeystoreKeyParams},
        /*keystore_decryptor_params=*/kKeystoreKeyParams,
        /*keystore_key_params=*/kKeystoreKeyParams,
        /*cross_user_sharing_keys=*/CreateNewCrossUserSharingKeys()));
  }

  // Replaces Nigori local data and re-initializes the bridge. This simulates
  // browser restart with a custom NigoriLocalData.
  void MimicRestartWithLocalData(
      const sync_pb::NigoriLocalData& nigori_local_data) {
    nigori_local_data_ = nigori_local_data;
    InitializeBridge();
  }

  // Initializes `bridge_` simulating browser startup. Note that this method
  // does not perform the initial sync.
  void InitializeBridge() {
    auto storage = std::make_unique<testing::NiceMock<MockNigoriStorage>>();
    storage_ = storage.get();
    ON_CALL(*storage, StoreData)
        .WillByDefault(testing::SaveArg<0>(&nigori_local_data_));
    if (nigori_local_data_.ByteSize() != 0) {
      // Return local data only if it's populated and non-empty. Otherwise,
      // return default nullopt.
      ON_CALL(*storage, RestoreData).WillByDefault(Return(nigori_local_data_));
    }
    if (bridge_) {
      bridge_->RemoveObserver(&observer_);
    }
    bridge_ = std::make_unique<NigoriSyncBridgeImpl>(
        std::make_unique<ForwardingNigoriLocalChangeProcessor>(processor()),
        std::move(storage));
    bridge_->AddObserver(&observer_);
  }

  NigoriSyncBridgeImpl* bridge() { return bridge_.get(); }
  MockNigoriLocalChangeProcessor* processor() { return &processor_; }
  MockObserver* observer() { return &observer_; }
  MockNigoriStorage* storage() { return storage_; }
  Cryptographer* cryptographer() { return bridge_->GetCryptographer(); }
  const sync_pb::NigoriLocalData& nigori_local_data() const {
    return nigori_local_data_;
  }

  const std::vector<uint8_t> kRawKeystoreKey = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kTrustedVaultKey = {2, 3, 4, 5, 6};

 private:
  // Nigori local data used by the storage to simulate loading and storing data
  // to the disk.
  sync_pb::NigoriLocalData nigori_local_data_;
  testing::NiceMock<MockNigoriLocalChangeProcessor> processor_;
  std::unique_ptr<NigoriSyncBridgeImpl> bridge_;
  // Ownership transferred to |bridge_|.
  raw_ptr<testing::NiceMock<MockNigoriStorage>> storage_ = nullptr;
  testing::NiceMock<MockObserver> observer_;
};

class NigoriSyncBridgeImplTestWithOptionalScryptDerivation
    : public NigoriSyncBridgeImplTest,
      public testing::WithParamInterface<bool> {
 public:
  NigoriSyncBridgeImplTestWithOptionalScryptDerivation()
      : key_params_(GetParam()
                        ? ScryptPassphraseKeyParamsForTesting("passphrase")
                        : Pbkdf2PassphraseKeyParamsForTesting("passphrase")) {}

  const KeyParamsForTesting& GetCustomPassphraseKeyParams() const {
    return key_params_;
  }

 private:
  const KeyParamsForTesting key_params_;
};

// During initialization bridge should expose encrypted types via observers
// notification.
TEST_F(NigoriSyncBridgeImplTest, ShouldNotifyObserversOnInit) {
  // TODO(crbug.com/40645422): Add a variant of this test, that loads Nigori
  // from storage and exposes more complete encryption state.
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(AlwaysEncryptedUserTypes(),
                                      /*encrypt_everything=*/false));
  bridge()->NotifyInitialStateToObservers();
}

// Tests that bridge support Nigori with IMPLICIT_PASSPHRASE.
TEST_F(NigoriSyncBridgeImplTest, ShouldAcceptKeysFromImplicitPassphraseNigori) {
  const KeyParamsForTesting kKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("password");
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
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  testing::InSequence seq;
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetExplicitPassphraseDecryptionKey(MakeNigoriKey(kKeyParams));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeyParams));
}

// Simplest case of keystore Nigori: we have only one keystore key and no old
// keys. This keystore key is encrypted in both encryption_keybag and
// keystore_decryptor_token. Client receives such Nigori if initialization of
// Nigori node was done after keystore was introduced and no key rotations
// happened.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromKeystoreNigoriAndNotifyObservers) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_CALL(*observer(), OnPassphraseRequired).Times(0);
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(0);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // The current implementation issues a redundant notification.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false))
      .Times(2);
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Simplest case of keystore Nigori with CrossUserSharingKeys.
TEST_F(
    NigoriSyncBridgeImplTest,
    ShouldAcceptKeysFromKeystoreNigoriWithCrossUserSharingKeysAndNotifyObservers) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  CrossUserSharingKeys cross_user_sharing_keys =
      CrossUserSharingKeys::CreateEmpty();
  CrossUserSharingPublicPrivateKeyPair key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  const auto raw_private_key = key_pair.GetRawPrivateKey();
  const auto raw_public_key = key_pair.GetRawPublicKey();
  const uint32_t kKeyVersion = 0;
  cross_user_sharing_keys.SetKeyPair(std::move(key_pair), kKeyVersion);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildKeystoreNigoriSpecificsWithCrossUserSharingKeys(
          /*keybag_keys_params=*/{kKeystoreKeyParams},
          /*keystore_decryptor_params=*/kKeystoreKeyParams,
          /*keystore_key_params=*/kKeystoreKeyParams,
          /*cross_user_sharing_keys=*/cross_user_sharing_keys,
          /*cross_user_sharing_public_key=*/
          CrossUserSharingPublicKey::CreateByImport(raw_public_key).value(),
          /*cross_user_sharing_public_key_version*/ kKeyVersion);

  EXPECT_CALL(*observer(), OnPassphraseRequired).Times(0);
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(0);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // The current implementation issues a redundant notification.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false))
      .Times(2);
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();

  EXPECT_THAT(bridge()->GetDataForDebugging(),
              HasPublicKeyVersionAndValue(
                  kKeyVersion,
                  std::string(raw_public_key.begin(), raw_public_key.end())));
  const CrossUserSharingPublicPrivateKeyPair& key_pair_in_cryptographer =
      bridge()->GetCryptographerImplForTesting().GetCrossUserSharingKeyPair(
          kKeyVersion);
  EXPECT_THAT(key_pair_in_cryptographer.GetRawPrivateKey(),
              testing::ElementsAreArray(raw_private_key));
  EXPECT_THAT(key_pair_in_cryptographer.GetRawPublicKey(),
              testing::ElementsAreArray(raw_public_key));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that client can properly process remote updates with rotated keystore
// nigori. Cryptographer should be able to decrypt any data encrypted with any
// keystore key and use current keystore key as default key.
TEST_F(NigoriSyncBridgeImplTest, ShouldAcceptKeysFromRotatedKeystoreNigori) {
  const std::vector<uint8_t> kRawOldKey = {5, 6, 7, 8};
  const KeyParamsForTesting kOldKeyParams =
      KeystoreKeyParamsForTesting(kRawOldKey);
  const std::vector<uint8_t> kRawCurrentKey{kRawKeystoreKey};
  const KeyParamsForTesting kCurrentKeyParams =
      KeystoreKeyParamsForTesting(kRawCurrentKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kOldKeyParams, kCurrentKeyParams},
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kCurrentKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawOldKey, kRawCurrentKey}));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kCurrentKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// In the backward compatible mode keystore Nigori's keystore_decryptor_token
// isn't a kestore key, however keystore_decryptor_token itself should be
// encrypted with the keystore key.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromBackwardCompatibleKeystoreNigori) {
  const KeyParamsForTesting kGaiaKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("gaia_key");
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kGaiaKeyParams, kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kGaiaKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kGaiaKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kGaiaKeyParams));
}

// Tests that we can successfully use old keys from encryption_keybag in
// backward compatible mode.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptOldKeysFromBackwardCompatibleKeystoreNigori) {
  // |kOldKeyParams| is needed to ensure we was able to decrypt
  // encryption_keybag - there is no way to add key derived from
  // |kOldKeyParams| to cryptographer without decrypting encryption_keybag.
  const KeyParamsForTesting kOldKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("old_key");
  const KeyParamsForTesting kCurrentKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("current_key");
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  const std::vector<KeyParamsForTesting> kAllKeyParams = {
      kOldKeyParams, kCurrentKeyParams, kKeystoreKeyParams};
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/kAllKeyParams,
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  for (const KeyParamsForTesting& key_params : kAllKeyParams) {
    EXPECT_THAT(*cryptographer(), CanDecryptWith(key_params));
  }
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// Tests that we build keystore Nigori, put it to processor, initialize the
// cryptographer and expose a valid entity through GetDataForCommit() /
// GetDataForDebugging(), when the default Nigori is received.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldPutAndMakeCryptographerReadyOnDefaultNigori) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // We don't verify entire NigoriSpecifics here, because it requires too
  // complex matcher (NigoriSpecifics is not determenistic).
  // Calling MergeFullSyncData() triggers a commit cycle but doesn't immediately
  // expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForCommit(), HasKeystoreNigori());

  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());
  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));
  EXPECT_EQ(PassphraseType::kKeystorePassphrase, bridge()->GetPassphraseType());

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that upon receiving Nigori corrupted due to absence of
// |encryption_keybag|, bridge respect its passphrase type and doesn't attempt
// to trigger keystore initialization.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotTriggerKeystoreInitializationForCorruptedCustomPassphrase) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  entity_data.specifics.mutable_nigori()->set_passphrase_type(
      sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // There should be no commits.
  EXPECT_CALL(*processor(), Put).Times(0);
  // Model error should be reported, because there is no |encryption_keybag|.
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Ne(std::nullopt));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRotateKeystoreKey) {
  const std::vector<uint8_t> kRawKeystoreKey1{kRawKeystoreKey};
  const KeyParamsForTesting kKeystoreKeyParams1 =
      KeystoreKeyParamsForTesting(kRawKeystoreKey1);

  sync_pb::NigoriSpecifics not_rotated_specifics = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams1},
      /*keystore_decryptor_params=*/kKeystoreKeyParams1,
      /*keystore_key_params=*/kKeystoreKeyParams1);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = not_rotated_specifics;
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey1}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  const std::vector<uint8_t> kRawKeystoreKey2 = {5, 6, 7, 8};
  const KeyParamsForTesting kKeystoreKeyParams2 =
      KeystoreKeyParamsForTesting(kRawKeystoreKey2);
  // Emulate server and client behavior: server sends both keystore keys and
  // |not_rotated_specifics| with changed metadata. Client have already seen
  // this specifics, but should pass it to the bridge, because bridge also
  // issues a commit, which conflicts with |not_rotated_specifics|.

  // Ensure bridge issues a commit right after SetKeystoreKeys() call, because
  // otherwise there is no conflict and ApplyIncrementalSyncChanges() will be
  // called with empty |data|.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey1, kRawKeystoreKey2}));

  // Populate new remote specifics to bridge, which is actually still
  // |not_rotated_specifics|.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() = not_rotated_specifics;
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));

  // Mimic commit completion.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams1));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams2));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams2));
}

// This test emulates late arrival of keystore keys, so neither
// |keystore_decryptor_token| or |encryption_keybag| could be decrypted at the
// moment NigoriSpecifics arrived. They should be decrypted right after
// keystore keys arrival.
TEST_F(NigoriSyncBridgeImplTest, ShouldDecryptPendingKeysInKeystoreMode) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
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
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  EXPECT_FALSE(cryptographer()->CanEncrypt());

  testing::InSequence seq;
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// This test emulates late arrival of keystore keys in backward-compatible
// keystore mode, so neither |keystore_decryptor_token| or |encryption_keybag|
// could be decrypted at the moment NigoriSpecifics arrived. Since default key
// is derived from legacy implicit passphrase, pending keys should be decrypted
// once passphrase passed to SetExplicitPassphraseDecryptionKey().
// SetKeystoreKeys() intentionally not called in this test, to not allow
// decryption with |keystore_decryptor_token|.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldDecryptPendingKeysWithPassphraseInKeystoreMode) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams, kPassphraseKeyParams},
      /*keystore_decryptor_params=*/kPassphraseKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  testing::InSequence seq;
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kPassphraseKeyParams));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kPassphraseKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kPassphraseKeyParams));

  // Regression part of the test, SetKeystoreKeys() call in this scenario used
  // to cause the crash (see crbug.com/1042203).
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_FALSE(bridge()->NeedKeystoreKey());
}

// Tests that bridge is able to decrypt keystore nigori, when
// |keystore_decryptor_token| is corrupted, but |encryption_keybag| is
// decryptable using keystore keys.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldDecryptKeystoreNigoriWithCorruptedKeystoreDecryptor) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData entity_data;
  // |keystore_decryptor_token| will be undecryptable.
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/Pbkdf2PassphraseKeyParamsForTesting("wrong_key"));

  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetKeystoreKeys({kRawKeystoreKey});

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_FALSE(bridge()->NeedKeystoreKey());
}

// Tests that unsuccessful attempt of |pending_keys| decryption ends up in
// additional OnPassphraseRequired() call. This is allowed because of possible
// change of |pending_keys| in keystore mode or due to transition from keystore
// to custom passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotifyWhenDecryptionWithPassphraseFailed) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  sync_pb::EncryptedData expected_pending_keys =
      entity_data.specifics.nigori().encryption_keybag();
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(expected_pending_keys)));
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(Pbkdf2PassphraseKeyParamsForTesting("wrong_passphrase")));

  EXPECT_THAT(bridge()->GetCryptographerImplForTesting().KeyBagSizeForTesting(),
              Eq(size_t(0)));
}

// Tests that attempt to SetEncryptionPassphrase() has no effect (at least
// that bridge's Nigori is still keystore one) if it was called, while bridge
// has pending keys in keystore mode.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotSetEncryptionPassphraseWithPendingKeys) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->SetEncryptionPassphrase("passphrase",
                                    MakeCustomPassphraseKeyDerivationParams());
  bridge()->SetKeystoreKeys({kRawKeystoreKey});

  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that we can perform initial sync with custom passphrase Nigori.
// We should notify observers about encryption state changes and cryptographer
// shouldn't be ready (by having pending keys) until user provides the
// passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotifyWhenSyncedWithCustomPassphraseNigori) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          Pbkdf2PassphraseKeyParamsForTesting("passphrase"));

  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

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

  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
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

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  // Note: passing default Nigori to MergeFullSyncData() leads to instantiation
  // of keystore Nigori.
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          Pbkdf2PassphraseKeyParamsForTesting("passphrase"));

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      Not(NullTime())));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
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
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  EXPECT_CALL(*processor(), Put).Times(0);
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Ne(std::nullopt));
}

// Test emulates remote update in custom passphrase mode, which contains
// |encryption_keybag| encrypted with known key, but without this key inside
// the |encryption_keybag|. This is a protocol violation and bridge should
// return ModelError on such updates.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnCustomPassphraseUpdateWithMissingKeybagDecryptionKey) {
  const KeyParamsForTesting kOldKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("old_key");
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");

  sync_pb::NigoriSpecifics specifics =
      BuildCustomPassphraseNigoriSpecifics(kPassphraseKeyParams, kOldKeyParams);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = specifics;
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kPassphraseKeyParams));

  // Emulate |encryption_keybag| corruption: it will contain only key derived
  // from |kOldKeyParams|, but will be encrypted with key derived from
  // |kPassphraseKeyParams|.
  std::unique_ptr<CryptographerImpl> passphrase_cryptographer =
      CryptographerImpl::FromSingleKeyForTesting(
          kPassphraseKeyParams.password,
          kPassphraseKeyParams.derivation_params);
  NigoriKeyBag old_key_key_bag = NigoriKeyBag::CreateEmpty();
  old_key_key_bag.AddKey(Nigori::CreateByDerivation(
      kOldKeyParams.derivation_params, kOldKeyParams.password));
  ASSERT_TRUE(passphrase_cryptographer->Encrypt(
      old_key_key_bag.ToProto(), specifics.mutable_encryption_keybag()));
  EntityData corrupted_entity_data;
  *corrupted_entity_data.specifics.mutable_nigori() = specifics;

  EXPECT_THAT(
      bridge()->ApplyIncrementalSyncChanges(std::move(corrupted_entity_data)),
      Ne(std::nullopt));
}

// Tests that bridge reports error when receiving corrupted NigoriSpecifics
// if decryption happens in SetKeystoreKeys().
TEST_F(NigoriSyncBridgeImplTest, ShouldFailOnInvalidKeystoreDecryption) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  // Don't populate |kKeystoreKeyParams| in |keybag_keys_params|, so encryption
  // keybag isn't valid. Put fake key params in |keybag_keys_params|, because
  // they must be non-empty.
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{Pbkdf2PassphraseKeyParamsForTesting("fake_key")},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  // Call SetKeystoreKeys() after MergeFullSyncData() to trigger decryption upon
  // receiving keystore keys.
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_CALL(*processor(), ReportError);
  EXPECT_FALSE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldClearDataWhenSyncDisabled) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  ASSERT_TRUE(cryptographer()->CanEncrypt());

  EXPECT_CALL(*storage(), ClearData);
  bridge()->ApplyDisableSyncChanges();
  EXPECT_FALSE(cryptographer()->CanEncrypt());
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldClearCrossUserSharingKeysWhenSyncDisabled) {
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  CrossUserSharingKeys cross_user_sharing_keys =
      CrossUserSharingKeys::CreateEmpty();
  CrossUserSharingPublicPrivateKeyPair key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  const auto raw_public_key = key_pair.GetRawPublicKey();
  const uint32_t kKeyVersion = 0;
  cross_user_sharing_keys.SetKeyPair(std::move(key_pair), kKeyVersion);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildKeystoreNigoriSpecificsWithCrossUserSharingKeys(
          /*keybag_keys_params=*/{kKeystoreKeyParams},
          /*keystore_decryptor_params=*/kKeystoreKeyParams,
          /*keystore_key_params=*/kKeystoreKeyParams,
          /*cross_user_sharing_keys=*/cross_user_sharing_keys,
          /*cross_user_sharing_public_key=*/
          CrossUserSharingPublicKey::CreateByImport(raw_public_key).value(),
          /*cross_user_sharing_public_key_version*/ kKeyVersion);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  ASSERT_TRUE(cryptographer()->CanEncrypt());
  // Encryption should succeed since a default key exists.
  CrossUserSharingPublicPrivateKeyPair peer_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  ASSERT_TRUE(cryptographer()
                  ->AuthEncryptForCrossUserSharing(
                      base::as_bytes(base::make_span("hello world")),
                      peer_key_pair.GetRawPublicKey())
                  .has_value());

  EXPECT_CALL(*storage(), ClearData);
  bridge()->ApplyDisableSyncChanges();
  EXPECT_FALSE(cryptographer()->CanEncrypt());
  EXPECT_FALSE(cryptographer()
                   ->AuthEncryptForCrossUserSharing(
                       base::as_bytes(base::make_span("hello world")),
                       peer_key_pair.GetRawPublicKey())
                   .has_value());
}

// Tests decryption logic for explicit passphrase. In order to check that we're
// able to decrypt the data encrypted with old key (i.e. keystore keys or old
// GAIA passphrase) we add one extra key to the encryption keybag.
TEST_P(NigoriSyncBridgeImplTestWithOptionalScryptDerivation,
       ShouldDecryptWithCustomPassphraseAndUpdateDefaultKey) {
  const KeyParamsForTesting kOldKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("old_key");
  const KeyParamsForTesting& passphrase_key_params =
      GetCustomPassphraseKeyParams();
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(passphrase_key_params,
                                           kOldKeyParams);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*key_derivation_params=*/passphrase_key_params.derivation_params,
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  testing::InSequence seq;
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(passphrase_key_params));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(*cryptographer(),
              HasDefaultKeyDerivedFrom(passphrase_key_params));
}

INSTANTIATE_TEST_SUITE_P(Scrypt,
                         NigoriSyncBridgeImplTestWithOptionalScryptDerivation,
                         testing::Values(false, true));

// Tests custom passphrase setup logic. Initially Nigori node will be
// initialized with keystore Nigori due to sync with default Nigori. After
// SetEncryptionPassphrase() call observers should be notified about state
// changes, custom passphrase Nigori should be put into the processor and
// exposed through GetDataForCommit(), cryptographer should encrypt data with
// custom passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldPutAndNotifyObserversWhenSetEncryptionPassphrase) {
  const std::string kCustomPassphrase = "passphrase";

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));

  // Calling SetEncryptionPassphrase() triggers a commit cycle but doesn't
  // immediately expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasCustomPassphraseNigori()));
  bridge()->SetEncryptionPassphrase(kCustomPassphrase,
                                    MakeCustomPassphraseKeyDerivationParams());
  EXPECT_THAT(bridge()->GetDataForCommit(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  testing::InSequence seq;
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasCustomPassphraseNigori());

  const KeyParamsForTesting passphrase_key_params = {
      bridge()->GetCustomPassphraseKeyDerivationParamsForTesting(),
      kCustomPassphrase};
  EXPECT_THAT(*cryptographer(), CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(*cryptographer(),
              HasDefaultKeyDerivedFrom(passphrase_key_params));
}

// Tests that pending local change with setting custom passphrase is applied,
// when there was a conflicting remote update and remote update is respected.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldSetCustomPassphraseAfterConflictingUpdates) {
  // Start with simple keystore Nigori.
  const std::vector<uint8_t> kRawKeystoreKey1{kRawKeystoreKey};
  const KeyParamsForTesting kKeystoreKeyParams1 =
      KeystoreKeyParamsForTesting(kRawKeystoreKey1);
  EntityData simple_keystore_entity_data;
  *simple_keystore_entity_data.specifics.mutable_nigori() =
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kKeystoreKeyParams1},
          /*keystore_decryptor_params=*/kKeystoreKeyParams1,
          /*keystore_key_params=*/kKeystoreKeyParams1);
  bridge()->SetKeystoreKeys({kRawKeystoreKey1});
  ASSERT_THAT(
      bridge()->MergeFullSyncData(std::move(simple_keystore_entity_data)),
      Eq(std::nullopt));

  // Set up custom passphrase locally, but don't emulate commit completion.
  const std::string kCustomPassphrase = "custom_passphrase";
  bridge()->SetEncryptionPassphrase(kCustomPassphrase,
                                    MakeCustomPassphraseKeyDerivationParams());

  // Emulate conflict with rotated keystore Nigori.
  const std::vector<uint8_t> kRawKeystoreKey2 = {5, 6, 7, 8};
  const KeyParamsForTesting kKeystoreKeyParams2 =
      KeystoreKeyParamsForTesting(kRawKeystoreKey2);
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
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(
                  std::move(rotated_keystore_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForCommit(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  testing::InSequence seq;
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasCustomPassphraseNigori());

  const KeyParamsForTesting passphrase_key_params = {
      bridge()->GetCustomPassphraseKeyDerivationParamsForTesting(),
      kCustomPassphrase};
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams1));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams2));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(passphrase_key_params));
  EXPECT_THAT(*cryptographer(),
              HasDefaultKeyDerivedFrom(passphrase_key_params));
}

// Tests that SetEncryptionPassphrase() call doesn't lead to custom passphrase
// change in case we already have one.
TEST_F(NigoriSyncBridgeImplTest, ShouldNotAllowCustomPassphraseChange) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          Pbkdf2PassphraseKeyParamsForTesting("passphrase"));
  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));

  EXPECT_CALL(*observer(), OnPassphraseAccepted()).Times(0);
  bridge()->SetEncryptionPassphrase("new_passphrase",
                                    MakeCustomPassphraseKeyDerivationParams());
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRestoreMetadata) {
  // Provide some custom metadata to verify that we store it.
  const std::string kFakeProgressMarkerToken = "progress_token";
  const int64_t kFakeSequenceNumber = 105;
  ON_CALL(*processor(), GetMetadata()).WillByDefault([&] {
    return CreateFakeNigoriMetadataBatch(kFakeProgressMarkerToken,
                                         kFakeSequenceNumber);
  });

  // Mimic initial sync, this should store metadata in nigori_local_data().
  ASSERT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());

  // Mimic browser restart and ensure that metadata was restored and passed to
  // the processor in ModelReadyToSync().
  EXPECT_CALL(
      *processor(),
      ModelReadyToSync(NotNull(),
                       IsFakeNigoriMetadataBatchWithTokenAndSequenceNumber(
                           kFakeProgressMarkerToken, kFakeSequenceNumber)));
  MimicRestartWithLocalData(nigori_local_data());
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRestoreKeystoreNigori) {
  ASSERT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  // Verify that we restored Cryptographer state.
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EXPECT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*bridge()->GetCryptographer(),
              HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Commit with keystore Nigori initialization might be not completed before
// the browser restart. This test emulates loading non-initialized Nigori
// after restart and expects that bridge will trigger initialization after
// loading.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldInitializeKeystoreNigoriWhenLoadedFromStorage) {
  // Prepare local data with keystore keys, but without initialized specifics.
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  NigoriState unitialized_state_with_keystore_keys;
  unitialized_state_with_keystore_keys.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(
          {kKeystoreKeyParams.password});

  sync_pb::NigoriLocalData nigori_local_data;
  nigori_local_data.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  *nigori_local_data.mutable_nigori_model() =
      unitialized_state_with_keystore_keys.ToLocalProto();

  // Upon startup bridge should attempt to commit keystore nigori again.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  MimicRestartWithLocalData(nigori_local_data);
  EXPECT_THAT(bridge()->GetDataForCommit(), HasKeystoreNigori());

  // Mimic commit completion.
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());
  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));
  EXPECT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kKeystorePassphrase));

  EXPECT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*bridge()->GetCryptographer(),
              HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests the initial sync with a trusted vault Nigori. Observers should be
// notified about encryption state changes and cryptographer shouldn't be ready
// (by having pending keys) until the passphrase is received by means other than
// the sync protocol.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldRequireUserActionIfInitiallyUsingTrustedVault) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  EXPECT_CALL(*observer(), OnPassphraseRequired).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // The current implementation issues redundant notifications.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(NotNull(), /*has_pending_keys=*/true))
      .Times(2);
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                      NullTime()))
      .Times(2);
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired()).Times(2);

  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  EXPECT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  EXPECT_THAT(bridge()->GetEncryptedTypes(), Eq(AlwaysEncryptedUserTypes()));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnTrustedVaultKeyAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());
}

// Tests the processing of a remote incremental update that transitions from
// keystore to trusted vault passphrase, which requires receiving the new
// passphrase by means other than the sync protocol.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteTransitionFromKeystoreToTrustedVault) {
  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();

  EXPECT_CALL(*observer(), OnPassphraseRequired).Times(0);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  // Note: passing default Nigori to MergeFullSyncData() leads to instantiation
  // of keystore Nigori.
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_FALSE(bridge()->GetTrustedVaultDebugInfo().has_migration_time());
  ASSERT_FALSE(bridge()->GetTrustedVaultDebugInfo().has_key_version());

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged).Times(0);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kTrustedVaultPassphrase,
                                      NullTime()));
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired());
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  EXPECT_THAT(bridge()->GetEncryptedTypes(), Eq(AlwaysEncryptedUserTypes()));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnTrustedVaultKeyAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_migration_time());
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_key_version());
}

// Tests the processing of a remote incremental update that rotates the trusted
// vault passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteKeyRotationForTrustedVault) {
  const std::vector<uint8_t> kRotatedTrustedVaultKey = {7, 8, 9, 10};

  EXPECT_CALL(*observer(), OnPassphraseRequired).Times(0);

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  // Mimic remote key rotation.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics(
          {kTrustedVaultKey, kRotatedTrustedVaultKey});
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged).Times(0);
  EXPECT_CALL(*observer(), OnPassphraseTypeChanged).Times(0);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(), OnTrustedVaultKeyRequired());

  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
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
  const std::string kCustomPassphrase = "custom_passphrase";

  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  // Calling SetEncryptionPassphrase() triggers a commit cycle but doesn't
  // immediately expose the new state, until the commit completes.
  EXPECT_CALL(*processor(), Put(HasCustomPassphraseNigori()));
  bridge()->SetEncryptionPassphrase(kCustomPassphrase,
                                    MakeCustomPassphraseKeyDerivationParams());
  EXPECT_THAT(bridge()->GetDataForCommit(), HasCustomPassphraseNigori());

  // Mimic commit completion.
  testing::InSequence seq;
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      /*passphrase_time=*/Not(NullTime())));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasCustomPassphraseNigori());
}

// Tests processing of remote incremental update that transits from trusted
// vault to keystore passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteTransitionFromTrustedVaultToKeystore) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  const KeyParamsForTesting kTrustedVaultKeyParams =
      TrustedVaultKeyParamsForTesting(kTrustedVaultKey);
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kTrustedVaultKeyParams, kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged).Times(0);
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(
      *observer(),
      OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase, NullTime()));

  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kKeystorePassphrase));
  EXPECT_THAT(bridge()->GetEncryptedTypes(), Eq(AlwaysEncryptedUserTypes()));
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kTrustedVaultKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*cryptographer(), HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests processing of remote incremental update that transits from trusted
// vault to custom passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldProcessRemoteTransitionFromTrustedVaultToCustomPassphrase) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  const KeyParamsForTesting kTrustedVaultKeyParams =
      TrustedVaultKeyParamsForTesting(kTrustedVaultKey);
  const KeyParamsForTesting kCustomPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("custom_passphrase");
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          kCustomPassphraseKeyParams,
          /*old_key_params=*/kTrustedVaultKeyParams);

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      Not(NullTime())));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kCustomPassphraseKeyParams));

  EXPECT_THAT(*cryptographer(), CanDecryptWith(kTrustedVaultKeyParams));
  EXPECT_THAT(*cryptographer(), CanDecryptWith(kCustomPassphraseKeyParams));
  EXPECT_THAT(*cryptographer(),
              HasDefaultKeyDerivedFrom(kCustomPassphraseKeyParams));
}

// Tests processing of remote incremental update that transits from trusted
// vault to keystore passphrase, which doesn't contain trusted vault key. The
// bridge should report model error.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnInvalidRemoteTransitionFromTrustedVaultToKeystore) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  // Don't populate kTrustedVaultKey into |new_entity_data|.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Ne(std::nullopt));
}

// Tests processing of remote incremental update that transits from trusted
// vault to custom passphrase, which doesn't contain trusted vault key. The
// bridge should report model error.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnInvalidRemoteTransitionFromTrustedVaultToCustomPassphrase) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  ASSERT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  const KeyParamsForTesting kCustomPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("custom_passphrase");
  // Don't populate kTrustedVaultKey into |new_entity_data|.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams);

  // The bridge doesn't know whether update is valid until decryption, expect
  // processing as a normal update.
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(
                               NotNull(), /*has_pending_keys=*/true));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      Not(NullTime())));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_TRUE(bridge()->HasPendingKeysForTesting());

  // Once decryption passphrase is provided, bridge should ReportError().
  EXPECT_CALL(*processor(), ReportError);
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kCustomPassphraseKeyParams));
}

// Tests processing of remote incremental update that transits from trusted
// vault to custom passphrase, which doesn't contain trusted vault key. Mimics
// browser restart in between of receiving the remote update and providing
// custom passphrase. The bridge should report model error.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnInvalidRemoteTransitionFromTrustedVaultAfterRestart) {
  // Perform initial sync with trusted vault passphrase.
  const std::vector<uint8_t> kTrustedVaultKey = {2, 3, 4, 5, 6};
  ASSERT_TRUE(PerformInitialSyncWithNigori(
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey})));

  bridge()->NotifyInitialStateToObservers();
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_THAT(bridge()->GetDataForDebugging(),
              Not(HasCustomPassphraseNigori()));

  // Mimic invalid remote update with custom passphrase.
  const KeyParamsForTesting kCustomPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("custom_passphrase");
  // Don't populate kTrustedVaultKeyParams into |new_entity_data|.
  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams);

  // The bridge doesn't know whether update is valid until decryption, expect
  // processing as a normal update.
  ASSERT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  // Once decryption passphrase is provided, bridge should ReportError().
  EXPECT_CALL(*processor(), ReportError);
  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kCustomPassphraseKeyParams));
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotAddDecryptionKeysToTrustedVaultCryptographer) {
  const std::vector<uint8_t> kTrustedVaultKey1{kTrustedVaultKey};
  const std::vector<uint8_t> kTrustedVaultKey2 = {3, 4, 5, 6};
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey1});

  ASSERT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(entity_data)),
              Eq(std::nullopt));
  bridge()->NotifyInitialStateToObservers();
  ASSERT_THAT(bridge()->GetPassphraseType(),
              Eq(PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(bridge()->HasPendingKeysForTesting());

  // Note that |kTrustedVaultKey2| was not part of Nigori specifics.
  bridge()->AddTrustedVaultDecryptionKeys(
      {kTrustedVaultKey1, kTrustedVaultKey2});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());

  const CryptographerImpl& cryptographer =
      bridge()->GetCryptographerImplForTesting();
  ASSERT_THAT(
      cryptographer,
      CanDecryptWith(TrustedVaultKeyParamsForTesting(kTrustedVaultKey1)));
  EXPECT_THAT(
      cryptographer,
      Not(CanDecryptWith(TrustedVaultKeyParamsForTesting(kTrustedVaultKey2))));
  EXPECT_THAT(cryptographer.KeyBagSizeForTesting(), Eq(size_t(1)));
}

// Tests that upon startup bridge migrates the Nigori from backward compatible
// keystore mode to full keystore mode.
TEST_F(NigoriSyncBridgeImplTest, ShouldCompleteKeystoreMigration) {
  // Perform initial sync with backward compatible keystore Nigori.
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");

  ASSERT_TRUE(PerformInitialSyncWithNigori(BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams, kPassphraseKeyParams},
      /*keystore_decryptor_params=*/kPassphraseKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams,
      CreateNewCrossUserSharingKeys())));

  // Upon startup bridge should issue a commit with full keystore Nigori.
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  // Mimic commit completion.
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());

  // Ensure the cryptographer corresponds to full keystore Nigori.
  EXPECT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(kPassphraseKeyParams));
  EXPECT_THAT(*bridge()->GetCryptographer(),
              HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that upon startup bridge adds keystore keys into cryptographer, so it
// can later decrypt the data using them.
TEST_F(NigoriSyncBridgeImplTest, ShouldDecryptWithKeystoreKeysAfterRestart) {
  // Perform initial sync with custom passphrase Nigori without keystore keys.
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  ASSERT_TRUE(PerformInitialSyncWithNigori(
      BuildCustomPassphraseNigoriSpecifics(kPassphraseKeyParams)));

  bridge()->SetExplicitPassphraseDecryptionKey(
      MakeNigoriKey(kPassphraseKeyParams));

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  ASSERT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(kPassphraseKeyParams));
  ASSERT_THAT(*bridge()->GetCryptographer(),
              HasDefaultKeyDerivedFrom(kPassphraseKeyParams));
  EXPECT_THAT(*bridge()->GetCryptographer(),
              CanDecryptWith(KeystoreKeyParamsForTesting(kRawKeystoreKey)));
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRestoreTrustedVaultNigori) {
  // Perform initial sync with TrustedVault Nigori.
  ASSERT_TRUE(PerformInitialSyncWithNigori(
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey})));

  // Ensure data is decryptable, this should be reflected in persisted state
  // (e.g. key shouldn't be required again after restart).
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  ASSERT_FALSE(bridge()->HasPendingKeysForTesting());

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  testing::NiceMock<MockObserver> observer;
  bridge()->AddObserver(&observer);
  // Verify that bridge notifies observer about restored state.
  EXPECT_CALL(observer, OnEncryptedTypesChanged(AlwaysEncryptedUserTypes(),
                                                /*encrypt_everything=*/false));
  EXPECT_CALL(observer,
              OnCryptographerStateChanged(
                  /*cryptographer=*/NotNull(), /*has_pending_keys=*/false));
  EXPECT_CALL(observer, OnPassphraseTypeChanged(
                            PassphraseType::kTrustedVaultPassphrase, _));
  EXPECT_CALL(observer, OnTrustedVaultKeyRequired).Times(0);
  bridge()->NotifyInitialStateToObservers();
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());

  // Verify that debug info was restored.
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_migration_time());
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_key_version());

  bridge()->RemoveObserver(&observer);
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldRestoreTrustedVaultNigoriWithPendingKeys) {
  // Perform initial sync with TrustedVault Nigori.
  ASSERT_TRUE(PerformInitialSyncWithNigori(
      BuildTrustedVaultNigoriSpecifics({kTrustedVaultKey})));

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  testing::NiceMock<MockObserver> observer;
  bridge()->AddObserver(&observer);
  // Verify that bridge notifies observer about restored state.
  EXPECT_CALL(observer, OnEncryptedTypesChanged(AlwaysEncryptedUserTypes(),
                                                /*encrypt_everything=*/false));
  EXPECT_CALL(observer, OnCryptographerStateChanged(/*cryptographer=*/NotNull(),
                                                    /*has_pending_keys=*/true));
  EXPECT_CALL(observer, OnPassphraseTypeChanged(
                            PassphraseType::kTrustedVaultPassphrase, _));
  EXPECT_CALL(observer, OnTrustedVaultKeyRequired);
  bridge()->NotifyInitialStateToObservers();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify that bridge accepts kTrustedVaultKey.
  EXPECT_CALL(observer, OnTrustedVaultKeyAccepted);
  EXPECT_CALL(observer,
              OnCryptographerStateChanged(
                  /*cryptographer=*/NotNull(), /*has_pending_keys=*/false));
  bridge()->AddTrustedVaultDecryptionKeys({kTrustedVaultKey});
  EXPECT_FALSE(bridge()->HasPendingKeysForTesting());

  // Verify that debug info was restored.
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_migration_time());
  EXPECT_TRUE(bridge()->GetTrustedVaultDebugInfo().has_key_version());

  bridge()->RemoveObserver(&observer);
}

// Tests that the initial built keystore Nigori, includes initialized
// Public-private key-pairs.
TEST_F(NigoriSyncBridgeImplTest, ShouldInitKeystoreNigoriWithKeyPair) {
  base::HistogramTester histogram_tester;

  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  std::string key_value;
  EXPECT_CALL(*processor(), Put(HasPublicKeyVersion(0)))
      .WillOnce([&key_value](auto committed_entity_data) {
        key_value = committed_entity_data->specifics.nigori()
                        .cross_user_sharing_public_key()
                        .x25519_public_key();
      });

  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForCommit(), HasKeystoreNigori());
  // Key version and material should be consistent across the processor and the
  // bridge.
  EXPECT_THAT(bridge()->GetDataForCommit(),
              HasPublicKeyVersionAndValue(0, key_value));

  // Mimic commit completion,
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());
  EXPECT_THAT(bridge()->GetDataForDebugging(),
              HasPublicKeyVersionAndValue(0, key_value));

  EXPECT_THAT(bridge()->GetKeystoreMigrationTime(), Not(NullTime()));
  histogram_tester.ExpectUniqueSample(
      "Sync.CrossUserSharingPublicPrivateKeyInitSuccess", true, 1);
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnDifferentKeyInitializingKeystoreNigoriWithKeyPair) {
  base::HistogramTester histogram_tester;

  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  std::string key_value;
  EXPECT_CALL(*processor(), Put(HasPublicKeyVersion(0)))
      .WillOnce([&key_value](auto committed_entity_data) {
        key_value = committed_entity_data->specifics.nigori()
                        .cross_user_sharing_public_key()
                        .x25519_public_key();
      });
  EXPECT_THAT(bridge()->MergeFullSyncData(std::move(default_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForCommit(), HasKeystoreNigori());
  // Key version and material should be consistent across the processor and the
  // bridge.
  EXPECT_THAT(bridge()->GetDataForCommit(),
              HasPublicKeyVersionAndValue(0, key_value));

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          Pbkdf2PassphraseKeyParamsForTesting("passphrase"));
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), Not(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->GetDataForDebugging(), Not(HasPublicKeyVersion(0)));
  histogram_tester.ExpectUniqueSample(
      "Sync.CrossUserSharingPublicPrivateKeyInitSuccess", false, 1);
}

// Tests that an existing Nigori will be initialized with Public-private
// key-pairs.
TEST_F(NigoriSyncBridgeImplTest, ShouldInitKeyPairForExistingNigori) {
  base::HistogramTester histogram_tester;

  // Perform initial sync with simple keystore Nigori without key pair.
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  ASSERT_TRUE(PerformInitialSyncWithNigori(BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams)));
  ASSERT_THAT(bridge()->GetDataForDebugging(), Not(HasPublicKeyVersion(0)));

  // Mimic the browser restart.
  std::string key_value;
  EXPECT_CALL(*processor(), Put(HasPublicKeyVersion(0)))
      .WillOnce([&key_value](auto committed_entity_data) {
        key_value = committed_entity_data->specifics.nigori()
                        .cross_user_sharing_public_key()
                        .x25519_public_key();
      });
  MimicRestartWithLocalData(nigori_local_data());

  // Mimic commit completion.
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());
  // Key version and material should be consistent across the processor and the
  // bridge.
  EXPECT_THAT(bridge()->GetDataForDebugging(),
              HasPublicKeyVersionAndValue(0, key_value));
  histogram_tester.ExpectUniqueSample(
      "Sync.CrossUserSharingPublicPrivateKeyInitSuccess", true, 1);
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldFailOnDifferentNigoriKeyInitializingKeyPairForExistingNigori) {
  base::HistogramTester histogram_tester;

  // Perform initial sync with simple keystore Nigori without key pair.
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(kRawKeystoreKey);
  ASSERT_TRUE(PerformInitialSyncWithNigori(BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams)));
  ASSERT_THAT(bridge()->GetDataForDebugging(), Not(HasPublicKeyVersion(0)));

  // Mimic the browser restart.
  MimicRestartWithLocalData(nigori_local_data());

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(
          Pbkdf2PassphraseKeyParamsForTesting("passphrase"));
  // Mimic unsuccessful commit due to conflict.
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::move(new_entity_data)),
              Eq(std::nullopt));
  EXPECT_THAT(bridge()->GetDataForDebugging(), Not(HasKeystoreNigori()));

  // Commit has failed due to conflict and bridge just received custom
  // passphrase Nigori. Bridge should not attempt to commit cross user sharing
  // key anymore, because it can't decrypt the custom passphrase Nigori yet.
  EXPECT_THAT(bridge()->GetDataForCommit(), Not(HasPublicKeyVersion(0)));
  histogram_tester.ExpectUniqueSample(
      "Sync.CrossUserSharingPublicPrivateKeyInitSuccess", false, 1);
}

TEST_F(NigoriSyncBridgeImplTest, ShouldRegenerateKeyPairIfCorrupted) {
  ASSERT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());

  sync_pb::NigoriLocalData local_data = nigori_local_data();

  // Mimic corrupted key pair, replace the public key with a new value.
  CrossUserSharingPublicPrivateKeyPair new_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  auto raw_public_key = new_key_pair.GetRawPublicKey();
  local_data.mutable_nigori_model()
      ->mutable_cross_user_sharing_public_key()
      ->set_x25519_public_key(
          std::string(raw_public_key.begin(), raw_public_key.end()));

  std::string new_public_key;
  EXPECT_CALL(*processor(), Put(HasPublicKeyVersion(0)))
      .WillOnce([&new_public_key](auto committed_entity_data) {
        new_public_key = committed_entity_data->specifics.nigori()
                             .cross_user_sharing_public_key()
                             .x25519_public_key();
      });
  base::HistogramTester histogram_tester;
  MimicRestartWithLocalData(local_data);

  // Verify that local state wasn't dropped.
  ASSERT_THAT(bridge()->GetDataForDebugging(), HasKeystoreNigori());

  // Verify that the key pair is corrupted.
  histogram_tester.ExpectUniqueSample("Sync.CrossUserSharingKeyPairState",
                                      /*kCorruptedKeyPair*/ 3,
                                      /*expected_bucket_count=*/1);

  // Mimic commit completion.
  EXPECT_THAT(bridge()->ApplyIncrementalSyncChanges(std::nullopt),
              Eq(std::nullopt));

  // Key version and material should be consistent across the processor and the
  // bridge.
  EXPECT_THAT(bridge()->GetDataForDebugging(),
              HasPublicKeyVersionAndValue(0, new_public_key));
  EXPECT_NE(new_public_key,
            std::string(raw_public_key.begin(), raw_public_key.end()));
}

// Regression test for crbug.com/329164040: stored local data suggests that
// initial sync was not done (due to data corruption or missing migration),
// the bridge should drop local data and perform initial sync once again.
// Main expectation is absence of crash.
TEST_F(NigoriSyncBridgeImplTest, ShouldIgnoreLocalDataWithoutInitialSyncDone) {
  ASSERT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());

  sync_pb::NigoriLocalData local_data = nigori_local_data();
  // Mimic corrupted (empty) |initial_sync_state| field.
  local_data.mutable_data_type_state()->clear_initial_sync_state();

  // Ensure that bridge ignores local state.
  EXPECT_CALL(*processor(),
              ModelReadyToSync(NotNull(), IsEmptyMetadataBatch()));
  MimicRestartWithLocalData(local_data);
  EXPECT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());
}

// The only legit scenario when UNKNOWN passphrase type gets persisted is when
// keystore keys are present, but commit wasn't completed before browser
// restart. Otherwise it indicates data corruption and it is safer to ignore
// such state.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldIgnoreLocalDataWithUnknownPassphraseWithoutKeystoreKeys) {
  sync_pb::NigoriLocalData local_data;
  // Set INITIAL_SYNC_DONE, because otherwise the data will be dropped anyway.
  local_data.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  // Don't set passphrase type (e.g. UNKNOWN will be used) and keystore keys
  // (this makes state invalid).

  // Ensure that bridge ignores local state.
  EXPECT_CALL(*processor(),
              ModelReadyToSync(NotNull(), IsEmptyMetadataBatch()));
  MimicRestartWithLocalData(local_data);
  EXPECT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldIgnoreLocalDataWithCustomPassphraseWithoutKeyDerivationParams) {
  ASSERT_TRUE(PerformInitialSyncWithNigori(BuildCustomPassphraseNigoriSpecifics(
      Pbkdf2PassphraseKeyParamsForTesting("passphrase"))));
  sync_pb::NigoriLocalData local_data = nigori_local_data();
  // Mimic corrupted custom passphrase key derivation params.
  local_data.mutable_nigori_model()
      ->clear_custom_passphrase_key_derivation_params();

  // Ensure that bridge ignores local state.
  EXPECT_CALL(*processor(),
              ModelReadyToSync(NotNull(), IsEmptyMetadataBatch()));
  MimicRestartWithLocalData(local_data);
  EXPECT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());
}

TEST_F(NigoriSyncBridgeImplTest,
       ShouldIgnoreLocalDataWithRealPassphraseTypeWithoutEncryptionKeys) {
  ASSERT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());
  sync_pb::NigoriLocalData local_data = nigori_local_data();
  // Mimic corrupted cryptographer data state.
  local_data.mutable_nigori_model()->clear_cryptographer_data();

  // Ensure that bridge ignores local state.
  EXPECT_CALL(*processor(),
              ModelReadyToSync(NotNull(), IsEmptyMetadataBatch()));
  MimicRestartWithLocalData(local_data);
  EXPECT_TRUE(PerformInitialSyncWithSimpleKeystoreNigori());
}

}  // namespace

}  // namespace syncer
