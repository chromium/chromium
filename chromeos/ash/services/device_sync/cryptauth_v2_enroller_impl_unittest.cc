// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_key_creator.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_key_proof_computer.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_enrollment.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

using cryptauthv2::SyncKeysRequest;
using SyncSingleKeyRequest = cryptauthv2::SyncKeysRequest::SyncSingleKeyRequest;

using cryptauthv2::SyncKeysResponse;
using SyncSingleKeyResponse =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse;
using KeyAction =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse::KeyAction;
using KeyCreation =
    cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse::KeyCreation;

using cryptauthv2::EnrollKeysRequest;
using EnrollSingleKeyRequest =
    cryptauthv2::EnrollKeysRequest::EnrollSingleKeyRequest;

using cryptauthv2::EnrollKeysResponse;
using EnrollSingleKeyResponse =
    cryptauthv2::EnrollKeysResponse::EnrollSingleKeyResponse;

using cryptauthv2::ApplicationSpecificMetadata;
using cryptauthv2::BetterTogetherFeatureMetadata;
using cryptauthv2::ClientAppMetadata;
using cryptauthv2::ClientDirective;
using cryptauthv2::ClientMetadata;
using cryptauthv2::FeatureMetadata;
using cryptauthv2::InvokeNext;
using cryptauthv2::KeyDirective;
using cryptauthv2::KeyType;
using cryptauthv2::PolicyReference;
using cryptauthv2::TargetService;

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";

const char kRandomSessionId[] = "random_session_id";

const char kOldActivePublicKey[] = "old_active_public_key";
const char kOldActivePrivateKey[] = "old_active_private_key";

// User key pair active handle must be kCryptAuthFixedUserKeyPairHandle.
const CryptAuthKey kOldActiveAsymmetricKey(kOldActivePublicKey,
                                           kOldActivePrivateKey,
                                           CryptAuthKey::Status::kActive,
                                           KeyType::P256,
                                           kCryptAuthFixedUserKeyPairHandle);

const char kOldActiveSymmetricKeyMaterial[] = "old_active_symmetric_key";
const char kOldActiveSymmetricKeyHandle[] = "old_active_symmetric_key_handle";
CryptAuthKey kOldActiveSymmetricKey(kOldActiveSymmetricKeyMaterial,
                                    CryptAuthKey::Status::kActive,
                                    KeyType::RAW128,
                                    kOldActiveSymmetricKeyHandle);

const char kOldInactiveSymmetricKeyMaterial[] = "old_inactive_symmetric_key";
const char kOldInactiveSymmetricKeyHandle[] =
    "old_inactive_symmetric_key_handle";
CryptAuthKey kOldInactiveSymmetricKey(kOldInactiveSymmetricKeyMaterial,
                                      CryptAuthKey::Status::kInactive,
                                      KeyType::RAW256,
                                      kOldInactiveSymmetricKeyHandle);

const char kNewPublicKey[] = "new_public_key";
const char kNewPrivateKey[] = "new_private_key";

const char kNewSymmetricKey[] = "new_symmetric_key";
const char kNewSymmetricKeyHandle[] = "new_symmetric_key_handle";

const char kServerEphemeralDh[] = "server_ephemeral_dh";
const char kClientDhPublicKey[] = "client_ephemeral_dh_public_key";
const char kClientDhPrivateKey[] = "client_ephemeral_dh_private_key";
const CryptAuthKey kClientEphemeralDh(kClientDhPublicKey,
                                      kClientDhPrivateKey,
                                      CryptAuthKey::Status::kActive,
                                      KeyType::P256);

class FakeCryptAuthKeyProofComputerFactory
    : public CryptAuthKeyProofComputerImpl::Factory {
 public:
  FakeCryptAuthKeyProofComputerFactory() = default;

  FakeCryptAuthKeyProofComputerFactory(
      const FakeCryptAuthKeyProofComputerFactory&) = delete;
  FakeCryptAuthKeyProofComputerFactory& operator=(
      const FakeCryptAuthKeyProofComputerFactory&) = delete;

  ~FakeCryptAuthKeyProofComputerFactory() override = default;

  void set_should_return_null_key_proof(bool should_return_null_key_proof) {
    should_return_null_key_proof_ = should_return_null_key_proof;
  }

 private:
  // CryptAuthKeyProofComputerImpl::Factory:
  std::unique_ptr<CryptAuthKeyProofComputer> CreateInstance() override {
    auto instance = std::make_unique<FakeCryptAuthKeyProofComputer>();
    instance->set_should_return_null(should_return_null_key_proof_);
    return instance;
  }

  bool should_return_null_key_proof_ = false;
};

class SyncSingleKeyResponseData {
 public:
  SyncSingleKeyResponseData(
      const CryptAuthKeyBundle::Name& bundle_name,
      const CryptAuthKeyRegistry* key_registry,
      const base::flat_map<std::string, KeyAction>& handle_to_action_map,
      const KeyCreation& new_key_creation,
      const std::optional<KeyType>& new_key_type,
      const std::optional<KeyDirective>& new_key_directive)
      : bundle_name(bundle_name),
        single_response(GenerateResponse(key_registry,
                                         handle_to_action_map,
                                         new_key_creation,
                                         new_key_type,
                                         new_key_directive)) {}

  CryptAuthKeyBundle::Name bundle_name;
  SyncSingleKeyResponse single_response;

 private:
  SyncSingleKeyResponse GenerateResponse(
      const CryptAuthKeyRegistry* key_registry,
      const base::flat_map<std::string, KeyAction>& handle_to_action_map,
      const KeyCreation& new_key_creation,
      const std::optional<KeyType>& new_key_type,
      const std::optional<KeyDirective>& new_key_directive) {
    SyncSingleKeyResponse generated_response;
    generated_response.set_key_creation(new_key_creation);
    if (new_key_type)
      generated_response.set_key_type(*new_key_type);
    if (new_key_directive)
      generated_response.mutable_key_directive()->CopyFrom(*new_key_directive);

    // If there are no keys, we don't need to add key actions.
    const CryptAuthKeyBundle* bundle = key_registry->GetKeyBundle(bundle_name);
    if (!bundle || handle_to_action_map.empty())
      return generated_response;

    // We assume the enroller populated SyncSingleKeyRequest::key_handles in
    // the same order as the key bundle's handle-to-key map. Populate
    // SyncSingleKeyResponse::key_actions with the same ordering. If a key
    // action for a handle is not specified in |handle_to_action| map, use
    // KEY_ACTION_UNSPECIFIED.
    for (const std::pair<std::string, CryptAuthKey>& handle_key_pair :
         bundle->handle_to_key_map()) {
      auto it = handle_to_action_map.find(handle_key_pair.first);
      KeyAction key_action = it == handle_to_action_map.end()
                                 ? SyncSingleKeyResponse::KEY_ACTION_UNSPECIFIED
                                 : it->second;
      generated_response.add_key_actions(key_action);
    }

    return generated_response;
  }
};

ClientMetadata GetClientMetadataForTest() {
  return cryptauthv2::BuildClientMetadata(
      2 /* retry_count */, ClientMetadata::PERIODIC /* invocation_reason */);
}

PolicyReference GetPreviousClientDirectivePolicyReferenceForTest() {
  return cryptauthv2::BuildPolicyReference(
      "Previous Client Directive Policy Reference", 1 /* version */);
}

ClientDirective GetNewClientDirectiveForTest() {
  return cryptauthv2::GetClientDirectiveForTest();
}

KeyDirective GetOldKeyDirectiveForTest() {
  return cryptauthv2::BuildKeyDirective(
      cryptauthv2::BuildPolicyReference("Old Key Policy Name",
                                        10 /* version */),
      100 /* enroll_time_millis */);
}

KeyDirective GetNewKeyDirectiveForTest() {
  return cryptauthv2::BuildKeyDirective(
      cryptauthv2::BuildPolicyReference("New Key Policy Name",
                                        20 /* version */),
      100 /* enroll_time_millis */);
}

// Note: Copied from the implementation file.
const std::vector<CryptAuthKeyBundle::Name>& GetKeyBundleOrder() {
  static const base::NoDestructor<std::vector<CryptAuthKeyBundle::Name>> order(
      [] {
        std::vector<CryptAuthKeyBundle::Name> order;
        for (const CryptAuthKeyBundle::Name& bundle_name :
             CryptAuthKeyBundle::AllEnrollableNames())
          order.push_back(bundle_name);
        return order;
      }());

  return *order;
}

// Returns the index of SyncKeysRequest.sync_single_key_requests or
// SyncKeysResponse.sync_single_key_responses that contains information about
// the key bundle |bundle_name|.
size_t GetKeyBundleIndex(const CryptAuthKeyBundle::Name& bundle_name) {
  for (size_t i = 0; i < GetKeyBundleOrder().size(); ++i) {
    if (GetKeyBundleOrder()[i] == bundle_name)
      return i;
  }

  return GetKeyBundleOrder().size();
}

// Builds a SyncKeysResponse, ensuring that the SyncSingleKeyResponses ordering
// aligns with GetKeyBundleOrder().
SyncKeysResponse BuildSyncKeysResponse(
    std::vector<SyncSingleKeyResponseData> sync_single_key_responses_data = {},
    const std::string& session_id = kRandomSessionId,
    const std::string& server_ephemeral_dh = kServerEphemeralDh,
    const ClientDirective& client_directive = GetNewClientDirectiveForTest()) {
  SyncKeysResponse sync_keys_response;
  sync_keys_response.set_random_session_id(session_id);
  sync_keys_response.set_server_ephemeral_dh(server_ephemeral_dh);
  sync_keys_response.mutable_client_directive()->CopyFrom(client_directive);

  // Make sure there are at least as many SyncSingleKeyResponses as key bundles.
  while (
      static_cast<size_t>(sync_keys_response.sync_single_key_responses_size()) <
      GetKeyBundleOrder().size()) {
    sync_keys_response.add_sync_single_key_responses();
  }

  // Populate the relevant SyncSingleKeyResponse for each key bundle with data
  // from the input |sync_single_key_responses_map|.
  for (const SyncSingleKeyResponseData& data : sync_single_key_responses_data) {
    sync_keys_response
        .mutable_sync_single_key_responses(GetKeyBundleIndex(data.bundle_name))
        ->CopyFrom(data.single_response);
  }

  return sync_keys_response;
}

}  // namespace

class DeviceSyncCryptAuthV2EnrollerImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceSyncCryptAuthV2EnrollerImplTest(
      const DeviceSyncCryptAuthV2EnrollerImplTest&) = delete;
  DeviceSyncCryptAuthV2EnrollerImplTest& operator=(
      const DeviceSyncCryptAuthV2EnrollerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthV2EnrollerImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)),
        fake_cryptauth_key_creator_factory_(
            std::make_unique<FakeCryptAuthKeyCreatorFactory>()),
        fake_cryptauth_key_proof_computer_factory_(
            std::make_unique<FakeCryptAuthKeyProofComputerFactory>()) {
    CryptAuthKeyRegistryImpl::RegisterPrefs(pref_service_.registry());
    key_registry_ = CryptAuthKeyRegistryImpl::Factory::Create(&pref_service_);

    client_factory_->AddObserver(this);
  }

  ~DeviceSyncCryptAuthV2EnrollerImplTest() override {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_key_creator_factory_.get());
    CryptAuthKeyProofComputerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_key_proof_computer_factory_.get());

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();

    enroller_ = CryptAuthV2EnrollerImpl::Factory::Create(
        key_registry(), client_factory(), std::move(mock_timer));
  }

  // testing::Test:
  void TearDown() override {
    CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthKeyProofComputerImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, SyncKeys(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthV2EnrollerImplTest::OnSyncKeys));

    ON_CALL(*client, EnrollKeys(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthV2EnrollerImplTest::OnEnrollKeys));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void CallEnroll(const cryptauthv2::ClientMetadata& client_metadata,
                  const cryptauthv2::ClientAppMetadata& client_app_metadata,
                  const std::optional<cryptauthv2::PolicyReference>&
                      client_directive_policy_reference) {
    enroller()->Enroll(
        client_metadata, client_app_metadata, client_directive_policy_reference,
        base::BindOnce(
            &DeviceSyncCryptAuthV2EnrollerImplTest::OnEnrollmentComplete,
            base::Unretained(this)));
  }

  void OnSyncKeys(const SyncKeysRequest& request,
                  CryptAuthClient::SyncKeysCallback callback,
                  CryptAuthClient::ErrorCallback error_callback) {
    // Check that SyncKeys is called before EnrollKeys.
    EXPECT_FALSE(sync_keys_request_);
    EXPECT_FALSE(enroll_keys_request_);
    EXPECT_TRUE(sync_keys_success_callback_.is_null());
    EXPECT_TRUE(enroll_keys_success_callback_.is_null());
    EXPECT_TRUE(sync_keys_failure_callback_.is_null());
    EXPECT_TRUE(enroll_keys_failure_callback_.is_null());

    sync_keys_request_ = request;
    sync_keys_success_callback_ = std::move(callback);
    sync_keys_failure_callback_ = std::move(error_callback);
  }

  void SendSyncKeysResponse(const SyncKeysResponse& sync_keys_response) {
    std::move(sync_keys_success_callback_).Run(sync_keys_response);
  }

  void FailSyncKeysRequest(const NetworkRequestError& network_request_error) {
    std::move(sync_keys_failure_callback_).Run(network_request_error);
  }

  void RunKeyCreator(
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& new_keys_output,
      const CryptAuthKey& client_ephemeral_dh_output) {
    std::move(key_creator()->create_keys_callback())
        .Run(new_keys_output, client_ephemeral_dh_output);
  }

  void SendEnrollKeysResponse(const EnrollKeysResponse& enroll_keys_response) {
    std::move(enroll_keys_success_callback_).Run(enroll_keys_response);
  }

  void FailEnrollKeysRequest(const NetworkRequestError& network_request_error) {
    std::move(enroll_keys_failure_callback_).Run(network_request_error);
  }

  void OnEnrollKeys(const EnrollKeysRequest& request,
                    CryptAuthClient::EnrollKeysCallback callback,
                    CryptAuthClient::ErrorCallback error_callback) {
    // Check that EnrollKeys is called after a successful SyncKeys call.
    EXPECT_TRUE(sync_keys_request_);
    EXPECT_FALSE(enroll_keys_request_);
    EXPECT_TRUE(sync_keys_success_callback_.is_null());
    EXPECT_TRUE(enroll_keys_success_callback_.is_null());
    EXPECT_FALSE(sync_keys_failure_callback_.is_null());
    EXPECT_TRUE(enroll_keys_failure_callback_.is_null());

    enroll_keys_request_ = request;
    enroll_keys_success_callback_ = std::move(callback);
    enroll_keys_failure_callback_ = std::move(error_callback);
  }

  void VerifyKeyCreatorInputs(
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& expected_new_keys,
      const std::string& expected_server_ephemeral_dh_public_key) {
    ASSERT_EQ(expected_new_keys.size(), key_creator()->keys_to_create().size());
    for (const std::pair<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>&
             name_key_pair : expected_new_keys) {
      const CryptAuthKeyBundle::Name& bundle_name = name_key_pair.first;
      const CryptAuthKey& key = *name_key_pair.second;
      ASSERT_TRUE(base::Contains(key_creator()->keys_to_create(), bundle_name));
      const CryptAuthKeyCreator::CreateKeyData& create_key_data =
          key_creator()->keys_to_create().find(bundle_name)->second;

      EXPECT_EQ(key.status(), create_key_data.status);
      EXPECT_EQ(key.type(), create_key_data.type);

      // Special handling for user key pair.
      if (bundle_name == CryptAuthKeyBundle::Name::kUserKeyPair) {
        EXPECT_EQ(key.handle(), create_key_data.handle);
        const CryptAuthKey* current_active_user_key_pair =
            key_registry()->GetActiveKey(
                CryptAuthKeyBundle::Name::kUserKeyPair);
        if (current_active_user_key_pair) {
          EXPECT_EQ(key.public_key(), create_key_data.public_key);
          EXPECT_EQ(current_active_user_key_pair->public_key(),
                    create_key_data.public_key);

          EXPECT_EQ(key.private_key(), create_key_data.private_key);
          EXPECT_EQ(current_active_user_key_pair->private_key(),
                    create_key_data.private_key);

          EXPECT_EQ(KeyType::P256, create_key_data.type);
          EXPECT_EQ(CryptAuthKey::Status::kActive, create_key_data.status);
        }
      }
    }

    ASSERT_TRUE(key_creator()->server_ephemeral_dh()->IsAsymmetricKey());
    EXPECT_EQ(expected_server_ephemeral_dh_public_key,
              key_creator()->server_ephemeral_dh()->public_key());
    EXPECT_EQ(KeyType::P256, key_creator()->server_ephemeral_dh()->type());
  }

  void VerifyEnrollSingleKeyRequest(const CryptAuthKeyBundle::Name& bundle_name,
                                    const CryptAuthKey& new_key) {
    const EnrollSingleKeyRequest& single_request_user_key_pair =
        enroll_keys_request()->enroll_single_key_requests(
            GetKeyBundleIndex(bundle_name));

    std::string bundle_name_str =
        CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name);

    EXPECT_EQ(bundle_name_str, single_request_user_key_pair.key_name());

    EXPECT_EQ(new_key.handle(), single_request_user_key_pair.new_key_handle());

    // No private or symmetric keys should be sent to CryptAuth, so key_material
    // should only ever be populated with a public key.
    EXPECT_EQ(new_key.IsAsymmetricKey() ? new_key.public_key() : std::string(),
              single_request_user_key_pair.key_material());

    EXPECT_EQ(
        CryptAuthKeyProofComputerImpl::Factory::Create()->ComputeKeyProof(
            new_key, kRandomSessionId, kCryptAuthKeyProofSalt, bundle_name_str),
        single_request_user_key_pair.key_proof());
  }

  CryptAuthV2Enroller* enroller() { return enroller_.get(); }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

  CryptAuthClientFactory* client_factory() { return client_factory_.get(); }

  FakeCryptAuthKeyProofComputerFactory* key_proof_computer_factory() {
    return fake_cryptauth_key_proof_computer_factory_.get();
  }

  base::MockOneShotTimer* timer() { return timer_; }

  const std::optional<SyncKeysRequest>& sync_keys_request() {
    return sync_keys_request_;
  }

  const std::optional<EnrollKeysRequest>& enroll_keys_request() {
    return enroll_keys_request_;
  }

  const std::optional<CryptAuthEnrollmentResult>& enrollment_result() {
    return enrollment_result_;
  }

 private:
  void OnEnrollmentComplete(
      const CryptAuthEnrollmentResult& enrollment_result) {
    enrollment_result_ = enrollment_result;
  }

  FakeCryptAuthKeyCreator* key_creator() {
    return fake_cryptauth_key_creator_factory_->instance();
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  std::unique_ptr<FakeCryptAuthKeyCreatorFactory>
      fake_cryptauth_key_creator_factory_;
  std::unique_ptr<FakeCryptAuthKeyProofComputerFactory>
      fake_cryptauth_key_proof_computer_factory_;

  // Parameters passed to the CryptAuthClient functions {Sync,Enroll}Keys().
  std::optional<SyncKeysRequest> sync_keys_request_;
  std::optional<EnrollKeysRequest> enroll_keys_request_;
  CryptAuthClient::SyncKeysCallback sync_keys_success_callback_;
  CryptAuthClient::EnrollKeysCallback enroll_keys_success_callback_;
  CryptAuthClient::ErrorCallback sync_keys_failure_callback_;
  CryptAuthClient::ErrorCallback enroll_keys_failure_callback_;

  std::optional<CryptAuthEnrollmentResult> enrollment_result_;

  std::unique_ptr<CryptAuthV2Enroller> enroller_;
};

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, SuccessfulEnrollment) {
  // Seed key registry.
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                         kOldActiveAsymmetricKey);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kUserKeyPair,
                                  GetOldKeyDirectiveForTest());
  CryptAuthKeyBundle expected_key_bundle_user_key_pair(
      *key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kUserKeyPair));

  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                         kOldActiveSymmetricKey);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                         kOldInactiveSymmetricKey);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                                  GetOldKeyDirectiveForTest());
  CryptAuthKeyBundle expected_key_bundle_legacy_authzen_key(
      *key_registry()->GetKeyBundle(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey));

  // Start the enrollment flow.
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  ClientDirective expected_new_client_directive =
      GetNewClientDirectiveForTest();
  KeyDirective expected_new_key_directive = GetNewKeyDirectiveForTest();

  // For kUserKeyPair (special case):
  //   - active --> temporarily active during key creation
  //   - new --> same handle so overwrites active key with same material
  // For kLegacyAuthzenKey:
  //   - active --> deleted
  //   - inactive --> temporarily active during key creation
  //   - new --> active after created
  std::vector<SyncSingleKeyResponseData> sync_single_key_responses_data = {
      SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {{kCryptAuthFixedUserKeyPairHandle,
            SyncSingleKeyResponse::ACTIVATE}} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          expected_new_key_directive /* new_key_directive */),
      SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey, key_registry(),
          {{kOldActiveSymmetricKeyHandle, SyncSingleKeyResponse::DELETE},
           {kOldInactiveSymmetricKeyHandle,
            SyncSingleKeyResponse::ACTIVATE}} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::RAW256 /* new_key_type */,
          expected_new_key_directive /* new_key_directive */)};

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse(sync_single_key_responses_data, kRandomSessionId,
                            kServerEphemeralDh, expected_new_client_directive);

  // Assume a successful SyncKeys() call.
  SendSyncKeysResponse(sync_keys_response);

  // Verify that the key actions were applied. (Note: New keys not created yet.)
  // No key actions expected for kUserKeyPair.
  EXPECT_EQ(
      expected_key_bundle_user_key_pair,
      *key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kUserKeyPair));

  // In kLegacyAuthzenKey bundle, former active key should have been deleted and
  // former inactive key should now be active.
  expected_key_bundle_legacy_authzen_key.DeleteKey(
      kOldActiveSymmetricKeyHandle);
  expected_key_bundle_legacy_authzen_key.SetActiveKey(
      kOldInactiveSymmetricKeyHandle);
  EXPECT_EQ(expected_key_bundle_legacy_authzen_key,
            *key_registry()->GetKeyBundle(
                CryptAuthKeyBundle::Name::kLegacyAuthzenKey));

  // Verify the key creation data, and assume successful key creation.
  // Note: Since an active user key pair already exists, the same key material
  // should re-used.
  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {{CryptAuthKeyBundle::Name::kUserKeyPair,
                            std::make_optional(CryptAuthKey(
                                kOldActivePublicKey, kOldActivePrivateKey,
                                CryptAuthKey::Status::kActive, KeyType::P256,
                                kCryptAuthFixedUserKeyPairHandle))},
                           {CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                            std::make_optional(CryptAuthKey(
                                kNewSymmetricKey, CryptAuthKey::Status::kActive,
                                KeyType::RAW256, kNewSymmetricKeyHandle))}};

  VerifyKeyCreatorInputs(
      expected_new_keys,
      kServerEphemeralDh /* expected_server_ephemeral_dh_public_key */);

  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  // Verify EnrollKeysRequest.
  EXPECT_EQ(kRandomSessionId, enroll_keys_request()->random_session_id());
  EXPECT_EQ(kClientDhPublicKey, enroll_keys_request()->client_ephemeral_dh());
  EXPECT_EQ(2, enroll_keys_request()->enroll_single_key_requests_size());
  VerifyEnrollSingleKeyRequest(
      CryptAuthKeyBundle::Name::kUserKeyPair,
      *expected_new_keys.find(CryptAuthKeyBundle::Name::kUserKeyPair)->second);
  VerifyEnrollSingleKeyRequest(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
      *expected_new_keys.find(CryptAuthKeyBundle::Name::kLegacyAuthzenKey)
           ->second);

  // Assume a successful EnrollKeys() call.
  // Note: No parameters in EnrollKeysResponse are processed by the enroller
  // (yet), so send a trivial response.
  SendEnrollKeysResponse(EnrollKeysResponse());

  // Verify enrollment result.
  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
                expected_new_client_directive),
            enrollment_result());

  // Verify that the key registry is updated with the newly enrolled keys
  // and new key directives.
  CryptAuthKeyBundle::Name bundle_name = CryptAuthKeyBundle::Name::kUserKeyPair;
  expected_key_bundle_user_key_pair.AddKey(
      *expected_new_keys.find(bundle_name)->second);
  expected_key_bundle_user_key_pair.set_key_directive(
      expected_new_key_directive);
  EXPECT_EQ(expected_key_bundle_user_key_pair,
            *key_registry()->GetKeyBundle(bundle_name));

  bundle_name = CryptAuthKeyBundle::Name::kLegacyAuthzenKey;
  expected_key_bundle_legacy_authzen_key.AddKey(
      *expected_new_keys.find(bundle_name)->second);
  expected_key_bundle_legacy_authzen_key.set_key_directive(
      expected_new_key_directive);
  EXPECT_EQ(expected_key_bundle_legacy_authzen_key,
            *key_registry()->GetKeyBundle(bundle_name));
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       SuccessfulEnrollment_CreateUserKeyPair_NoKeyInRegistry) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  ClientDirective expected_new_client_directive =
      GetNewClientDirectiveForTest();
  KeyDirective expected_new_key_directive = GetNewKeyDirectiveForTest();

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse(
      {SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          expected_new_key_directive /* new_key_directive */)},
      kRandomSessionId, kServerEphemeralDh, expected_new_client_directive);

  SendSyncKeysResponse(sync_keys_response);

  // Note: Because there is not an existing kUserKeyPair key in registry, a new
  // key should be generated. (If there was an existing key, its key material
  // would be reused because the kUserKeyPair key should not be rotated.)
  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           std::make_optional(CryptAuthKey(
               kNewPublicKey, kNewPrivateKey, CryptAuthKey::Status::kActive,
               KeyType::P256, kCryptAuthFixedUserKeyPairHandle))}};

  VerifyKeyCreatorInputs(
      expected_new_keys,
      kServerEphemeralDh /* expected_server_ephemeral_dh_public_key */);

  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  EXPECT_EQ(1, enroll_keys_request()->enroll_single_key_requests_size());
  VerifyEnrollSingleKeyRequest(
      CryptAuthKeyBundle::Name::kUserKeyPair,
      *expected_new_keys.find(CryptAuthKeyBundle::Name::kUserKeyPair)->second);

  SendEnrollKeysResponse(EnrollKeysResponse());

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
                expected_new_client_directive),
            enrollment_result());

  CryptAuthKeyBundle expected_key_bundle(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_key_bundle.AddKey(
      *expected_new_keys.find(CryptAuthKeyBundle::Name::kUserKeyPair)->second);
  expected_key_bundle.set_key_directive(expected_new_key_directive);
  EXPECT_EQ(expected_key_bundle, *key_registry()->GetKeyBundle(
                                     CryptAuthKeyBundle::Name::kUserKeyPair));
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       SuccessfulEnrollment_NoKeysCreated) {
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                         kOldActiveSymmetricKey);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                         kOldInactiveSymmetricKey);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                                  GetOldKeyDirectiveForTest());
  CryptAuthKeyBundle expected_key_bundle(*key_registry()->GetKeyBundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey));

  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Simulate CryptAuth instructing us to swap active and inactive key states
  // but not create any new keys.
  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey, key_registry(),
          {{kOldActiveSymmetricKeyHandle, SyncSingleKeyResponse::DEACTIVATE},
           {kOldInactiveSymmetricKeyHandle,
            SyncSingleKeyResponse::ACTIVATE}} /* handle_to_action_map */,
          SyncSingleKeyResponse::NONE /* new_key_creation */,
          std::nullopt /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  expected_key_bundle.SetActiveKey(kOldInactiveSymmetricKeyHandle);
  EXPECT_EQ(expected_key_bundle,
            *key_registry()->GetKeyBundle(
                CryptAuthKeyBundle::Name::kLegacyAuthzenKey));

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
                sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_ServerOverloaded) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse();
  sync_keys_response.set_server_status(SyncKeysResponse::SERVER_OVERLOADED);

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorCryptAuthServerOverloaded,
                                      std::nullopt /* client_directive */),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_MissingSessionId) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse();
  sync_keys_response.clear_random_session_id();

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorSyncKeysResponseMissingRandomSessionId,
                std::nullopt /* client_directive */),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_MissingClientDirective) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse();
  sync_keys_response.clear_client_directive();

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorSyncKeysResponseInvalidClientDirective,
                std::nullopt /* client_directive */),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidSyncSingleKeyResponsesSize) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse();
  sync_keys_response.clear_sync_single_key_responses();

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorWrongNumberOfSyncSingleKeyResponses,
                                std::nullopt /* client_directive */),
      enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_InvalidKeyActions_Size) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response = BuildSyncKeysResponse();

  // Add a key action for a bundle that has no keys.
  sync_keys_response.mutable_sync_single_key_responses(0)->add_key_actions(
      SyncSingleKeyResponse::ACTIVATE);

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(
      CryptAuthEnrollmentResult(
          CryptAuthEnrollmentResult::ResultCode::kErrorWrongNumberOfKeyActions,
          sync_keys_response.client_directive()),
      enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidKeyActions_NoActiveKey) {
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                         kOldActiveAsymmetricKey);

  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Try to deactivate the only active key.
  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey, key_registry(),
          {{kOldActiveSymmetricKeyHandle,
            SyncSingleKeyResponse::DEACTIVATE}} /* handle_to_action_map */,
          SyncSingleKeyResponse::NONE /* new_key_creation */,
          std::nullopt /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorKeyActionsDoNotSpecifyAnActiveKey,
                                sync_keys_response.client_directive()),
      enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidKeyCreationInstructions_UnsupportedKeyType) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Instruct client to create an unsupported key type, CURVE25519.
  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::CURVE25519 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorKeyCreationKeyTypeNotSupported,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidKeyCreationInstructions_UnsupportedUserKeyPairKeyType) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Instruct client to create a symmetric user key pair. The user key pair is
  // heavily protected against anything other than P256.
  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::RAW256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorUserKeyPairCreationInstructionsInvalid,
                sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidKeyCreationInstructions_NewUserKeyPairMustBeActive) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Instruct client to create a new, inactive user key pair. Since there can
  // only be one user key pair in the bundle, a new one must be active.
  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::INACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorUserKeyPairCreationInstructionsInvalid,
                sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_InvalidKeyCreationInstructions_NoServerDiffieHellman) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::RAW256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  sync_keys_response.clear_server_ephemeral_dh();

  SendSyncKeysResponse(sync_keys_response);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorSymmetricKeyCreationMissingServerDiffieHellman,
                sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_KeyCreation_UserKeyPair) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kUserKeyPair, std::nullopt}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorUserKeyPairCreationFailed,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_KeyCreation_LegacyAuthzenKey) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kLegacyAuthzenKey, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::RAW256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kLegacyAuthzenKey, std::nullopt}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorLegacyAuthzenKeyCreationFailed,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_KeyCreation_DeviceSyncBetterTogether) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether, std::nullopt}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  EXPECT_EQ(CryptAuthEnrollmentResult(
                CryptAuthEnrollmentResult::ResultCode::
                    kErrorDeviceSyncBetterTogetherKeyCreationFailed,
                sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_KeyProofComputationFailed) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  key_proof_computer_factory()->set_should_return_null_key_proof(true);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           std::make_optional(CryptAuthKey(
               kNewPublicKey, kNewPrivateKey, CryptAuthKey::Status::kActive,
               KeyType::P256, kCryptAuthFixedUserKeyPairHandle))}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorKeyProofComputationFailed,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_SyncKeysApiCall) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  FailSyncKeysRequest(NetworkRequestError::kAuthenticationError);

  EXPECT_EQ(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorSyncKeysApiCallAuthenticationError,
                                std::nullopt /* client_directive */),
      enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest, Failure_EnrollKeysApiCall) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           std::make_optional(CryptAuthKey(
               kNewPublicKey, kNewPrivateKey, CryptAuthKey::Status::kActive,
               KeyType::P256, kCryptAuthFixedUserKeyPairHandle))}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  FailEnrollKeysRequest(NetworkRequestError::kBadRequest);

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorEnrollKeysApiCallBadRequest,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_Timeout_WaitingForSyncKeysResponse) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  // Timeout waiting for SyncKeysResponse.
  EXPECT_TRUE(timer()->IsRunning());
  timer()->Fire();

  EXPECT_EQ(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorTimeoutWaitingForSyncKeysResponse,
                                std::nullopt /* client_directive */),
      enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_Timeout_WaitingForKeyCreation) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  // Timeout waiting for key creation.
  EXPECT_TRUE(timer()->IsRunning());
  timer()->Fire();

  EXPECT_EQ(CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                          kErrorTimeoutWaitingForKeyCreation,
                                      sync_keys_response.client_directive()),
            enrollment_result());
}

TEST_F(DeviceSyncCryptAuthV2EnrollerImplTest,
       Failure_Timeout_WaitingForEnrollKeysResponse) {
  CallEnroll(GetClientMetadataForTest(),
             cryptauthv2::GetClientAppMetadataForTest(),
             GetPreviousClientDirectivePolicyReferenceForTest());

  SyncKeysResponse sync_keys_response =
      BuildSyncKeysResponse({SyncSingleKeyResponseData(
          CryptAuthKeyBundle::Name::kUserKeyPair, key_registry(),
          {} /* handle_to_action_map */,
          SyncSingleKeyResponse::ACTIVE /* new_key_creation */,
          KeyType::P256 /* new_key_type */,
          std::nullopt /* new_key_directive */)});
  SendSyncKeysResponse(sync_keys_response);

  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      expected_new_keys = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           std::make_optional(CryptAuthKey(
               kNewPublicKey, kNewPrivateKey, CryptAuthKey::Status::kActive,
               KeyType::P256, kCryptAuthFixedUserKeyPairHandle))}};
  RunKeyCreator(expected_new_keys, kClientEphemeralDh);

  // Timeout waiting for EnrollKeysResponse.
  EXPECT_TRUE(timer()->IsRunning());
  timer()->Fire();

  EXPECT_EQ(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorTimeoutWaitingForEnrollKeysResponse,
                                sync_keys_response.client_directive()),
      enrollment_result());
}

}  // namespace device_sync

}  // namespace ash
