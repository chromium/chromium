// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_store.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disabled\""
    "  },"
    "  \"Second\": {"
    "    \"Value\": \"maybe\","
    "    \"Level\": \"Recommended\""
    "  }"
    "}";

std::string TestPolicyHash() {
  return crypto::SHA256HashString(kTestPolicy);
}

bool NotEqual(const std::string& expected,
              const PolicyDomain domain,
              const std::string& key) {
  return key != expected;
}

bool True(const PolicyDomain domain, const std::string& ignored) {
  return true;
}

class MockComponentCloudPolicyStoreDelegate
    : public ComponentCloudPolicyStore::Delegate {
 public:
  ~MockComponentCloudPolicyStoreDelegate() override {}

  MOCK_METHOD0(OnComponentCloudPolicyStoreUpdated, void());
};

}  // namespace

class ComponentCloudPolicyStoreTest : public testing::Test {
 protected:
  ComponentCloudPolicyStoreTest()
      : kTestPolicyNS(POLICY_DOMAIN_EXTENSIONS, kTestExtension) {
    builder_.SetDefaultSigningKey();
    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(TestPolicyHash());

    public_key_ = builder_.GetPublicSigningKeyAsString();

    SetupExpectBundleWithScope(POLICY_SCOPE_USER);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_.reset(new ResourceCache(
        temp_dir_.GetPath(), base::MakeRefCounted<base::TestSimpleTaskRunner>(),
        /* max_cache_size */ base::nullopt));
    store_ = CreateStore();
    store_->SetCredentials(
        PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
        PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
        PolicyBuilder::kFakePublicKeyVersion);
  }

  void SetupExpectBundleWithScope(
      const PolicyScope& scope,
      const PolicySource& source = POLICY_SOURCE_CLOUD) {
    PolicyMap& policy = expected_bundle_.Get(kTestPolicyNS);
    policy.Clear();
    policy.Set("Name", POLICY_LEVEL_MANDATORY, scope, source,
               std::make_unique<base::Value>("disabled"), nullptr);
    policy.Set("Second", POLICY_LEVEL_RECOMMENDED, scope, source,
               std::make_unique<base::Value>("maybe"), nullptr);
  }

  std::unique_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return std::make_unique<em::PolicyFetchResponse>(builder_.policy());
  }

  std::unique_ptr<em::PolicyData> CreatePolicyData() {
    builder_.Build();
    return std::make_unique<em::PolicyData>(builder_.policy_data());
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  std::unique_ptr<ComponentCloudPolicyStore> CreateStore(
      const std::string& policy_type = dm_protocol::kChromeExtensionPolicyType,
      const PolicySource& source = POLICY_SOURCE_CLOUD) {
    return std::make_unique<ComponentCloudPolicyStore>(
        &store_delegate_, cache_.get(), policy_type, source);
  }

  // Returns true if the policy exposed by the |store| is empty.
  bool IsStoreEmpty(const ComponentCloudPolicyStore& store) {
    return store.policy().Equals(PolicyBundle());
  }

  void StoreTestPolicy(ComponentCloudPolicyStore* store) {
    StoreTestPolicyWithNamespace(store, kTestPolicyNS);
  }
  void StoreTestPolicyWithNamespace(ComponentCloudPolicyStore* store,
                                    const PolicyNamespace& ns) {
    EXPECT_TRUE(store->ValidatePolicy(ns, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
    EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
    EXPECT_TRUE(store->Store(ns, CreateSerializedResponse(),
                             CreatePolicyData().get(), TestPolicyHash(),
                             kTestPolicy));
    Mock::VerifyAndClearExpectations(&store_delegate_);
    EXPECT_TRUE(store->policy().Equals(expected_bundle_));
    EXPECT_FALSE(LoadCacheExtensionsSubkeys().empty());
  }

  std::map<std::string, std::string> LoadCacheExtensionsSubkeys() {
    std::map<std::string, std::string> contents;
    cache_->LoadAllSubkeys("extension-policy", &contents);
    return contents;
  }

  const PolicyNamespace kTestPolicyNS;
  std::unique_ptr<ResourceCache> cache_;

  std::unique_ptr<ComponentCloudPolicyStore> store_;
  std::unique_ptr<ComponentCloudPolicyStore> another_store_;
  std::unique_ptr<ComponentCloudPolicyStore> yet_another_store_;

  MockComponentCloudPolicyStoreDelegate store_delegate_;
  ComponentCloudPolicyBuilder builder_;
  PolicyBundle expected_bundle_;
  std::string public_key_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicy) {
  em::PolicyData policy_data;
  em::ExternalPolicyData payload;
  EXPECT_TRUE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                     &policy_data, &payload));
  EXPECT_EQ(dm_protocol::kChromeExtensionPolicyType, policy_data.policy_type());
  EXPECT_EQ(kTestExtension, policy_data.settings_entity_id());
  EXPECT_EQ(kTestDownload, payload.download_url());
  EXPECT_EQ(TestPolicyHash(), payload.secure_hash());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongTimestamp) {
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));

  const int64_t kPastTimestamp =
      (base::Time() + base::TimeDelta::FromDays(1)).ToJavaTime();
  CHECK_GT(PolicyBuilder::kFakeTimestamp, kPastTimestamp);
  builder_.policy_data().set_timestamp(kPastTimestamp);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongUser) {
  builder_.policy_data().set_username("anotheruser@example.com");
  builder_.policy_data().set_gaia_id("another-gaia-id");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongDMToken) {
  builder_.policy_data().set_request_token("notmytoken");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongDeviceId) {
  builder_.policy_data().set_device_id("invalid");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadType) {
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongNamespace) {
  EXPECT_FALSE(store_->ValidatePolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "nosuchid"), CreateResponse(),
      nullptr /* policy_data */, nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyNoSignature) {
  builder_.UnsetSigningKey();
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadSignature) {
  std::unique_ptr<em::PolicyFetchResponse> response = CreateResponse();
  response->set_policy_data_signature("invalid");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, std::move(response),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyEmptyComponentId) {
  builder_.policy_data().set_settings_entity_id(std::string());
  EXPECT_FALSE(store_->ValidatePolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()),
      CreateResponse(), nullptr /* policy_data */, nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongPublicKey) {
  // Test against a policy signed with a wrong key.
  builder_.SetSigningKey(*PolicyBuilder::CreateTestOtherSigningKey());
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongPublicKeyVersion) {
  // Test against a policy containing wrong public key version.
  builder_.policy_data().set_public_key_version(
      PolicyBuilder::kFakePublicKeyVersion + 1);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyDifferentPublicKey) {
  // Test against a policy signed with a different key and containing a new
  // public key version.
  builder_.SetSigningKey(*PolicyBuilder::CreateTestOtherSigningKey());
  builder_.policy_data().set_public_key_version(
      PolicyBuilder::kFakePublicKeyVersion + 1);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadDownloadUrl) {
  builder_.payload().set_download_url("invalidurl");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyEmptyDownloadUrl) {
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  // This is valid; it's how "no policy" is signalled to the client.
  EXPECT_TRUE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                     nullptr /* policy_data */,
                                     nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadPayload) {
  builder_.clear_payload();
  builder_.policy_data().set_policy_value("broken");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentials) {
  store_ = CreateStore();
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsUser) {
  store_ = CreateStore();
  store_->SetCredentials(/*username=*/std::string(), /*gaia_id=*/std::string(),
                         PolicyBuilder::kFakeToken,
                         PolicyBuilder::kFakeDeviceId, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsDMToken) {
  store_ = CreateStore();
  store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      std::string() /* dm_token */, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsDeviceId) {
  store_ = CreateStore();
  store_->SetCredentials(PolicyBuilder::kFakeUsername,
                         PolicyBuilder::kFakeGaiaId, PolicyBuilder::kFakeToken,
                         std::string() /* device_id */, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsPublicKey) {
  store_ = CreateStore();
  store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId,
      std::string() /* public_key */, PolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsPublicKeyVersion) {
  StoreTestPolicy(store_.get());
  another_store_ = CreateStore();
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      -1 /* public_key_version */);
  another_store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*another_store_));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsDMToken) {
  StoreTestPolicy(store_.get());
  another_store_ = CreateStore();
  another_store_->SetCredentials(PolicyBuilder::kFakeUsername,
                                 PolicyBuilder::kFakeGaiaId, "wrongtoken",
                                 PolicyBuilder::kFakeDeviceId, public_key_,
                                 PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*another_store_));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsDeviceId) {
  StoreTestPolicy(store_.get());
  another_store_ = CreateStore();
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, "wrongdeviceid", public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*another_store_));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsPublicKey) {
  StoreTestPolicy(store_.get());
  another_store_ = CreateStore();
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, "wrongkey",
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*another_store_));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest,
       ValidateWrongCredentialsPublicKeyVersion) {
  StoreTestPolicy(store_.get());
  another_store_ = CreateStore();
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion + 1);
  another_store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*another_store_));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest,
       ValidatePolicyWithInvalidCombinationOfDomainAndPolicyType) {
  PolicyNamespace ns_chrome(POLICY_DOMAIN_CHROME, std::string());
  PolicyNamespace ns_extension(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  PolicyNamespace ns_signin_extension(POLICY_DOMAIN_SIGNIN_EXTENSIONS,
                                      kTestExtension);

  store_ =
      CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  EXPECT_FALSE(store_->ValidatePolicy(ns_chrome, CreateResponse(),
                                      nullptr /*policy_data*/,
                                      nullptr /*payload*/));
  EXPECT_FALSE(store_->ValidatePolicy(ns_signin_extension, CreateResponse(),
                                      nullptr, nullptr));

  store_ = CreateStore(dm_protocol::kChromeSigninExtensionPolicyType);
  EXPECT_FALSE(
      store_->ValidatePolicy(ns_chrome, CreateResponse(), nullptr, nullptr));
  EXPECT_FALSE(
      store_->ValidatePolicy(ns_extension, CreateResponse(), nullptr, nullptr));

  store_ = CreateStore(dm_protocol::kChromeExtensionPolicyType);
  EXPECT_FALSE(
      store_->ValidatePolicy(ns_chrome, CreateResponse(), nullptr, nullptr));
  EXPECT_FALSE(store_->ValidatePolicy(ns_signin_extension, CreateResponse(),
                                      nullptr, nullptr));
}

TEST_F(ComponentCloudPolicyStoreTest, StoreAndLoad) {
  // Initially empty.
  EXPECT_TRUE(IsStoreEmpty(*store_));
  store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*store_));

  // Store policy for an unsupported domain.
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  EXPECT_FALSE(
      store_->Store(PolicyNamespace(POLICY_DOMAIN_CHROME, kTestExtension),
                    CreateSerializedResponse(), CreatePolicyData().get(),
                    TestPolicyHash(), kTestPolicy));

  // Store policy for an unowned domain.
  EXPECT_FALSE(store_->Store(
      PolicyNamespace(POLICY_DOMAIN_SIGNIN_EXTENSIONS, kTestExtension),
      CreateSerializedResponse(), CreatePolicyData().get(), TestPolicyHash(),
      kTestPolicy));

  // Store policy with the wrong hash.
  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeExtensionPolicyType);
  builder_.payload().set_secure_hash("badash");
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), "badash", kTestPolicy));

  // Store policy without a hash.
  builder_.payload().clear_secure_hash();
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), std::string(),
                             kTestPolicy));

  // Store policy with invalid JSON data.
  static const char kInvalidData[] = "{ not json }";
  const std::string invalid_data_hash = crypto::SHA256HashString(kInvalidData);
  builder_.payload().set_secure_hash(invalid_data_hash);
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), invalid_data_hash,
                             kInvalidData));

  // All of those failed.
  EXPECT_TRUE(IsStoreEmpty(*store_));
  EXPECT_EQ(std::string(), store_->GetCachedHash(kTestPolicyNS));

  // Now store a valid policy.
  builder_.payload().set_secure_hash(TestPolicyHash());
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), store_->GetCachedHash(kTestPolicyNS));

  // Loading from the cache validates the policy data again.
  another_store_ = CreateStore();
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(another_store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store_->GetCachedHash(kTestPolicyNS));
}

TEST_F(ComponentCloudPolicyStoreTest, StoreAndLoadMachineLevelUserPolicy) {
  store_ =
      CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  store_->SetCredentials(PolicyBuilder::kFakeUsername,
                         PolicyBuilder::kFakeGaiaId, PolicyBuilder::kFakeToken,
                         PolicyBuilder::kFakeDeviceId, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);

  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  builder_.payload().set_secure_hash(TestPolicyHash());
  SetupExpectBundleWithScope(POLICY_SCOPE_MACHINE);

  StoreTestPolicyWithNamespace(store_.get(), kTestPolicyNS);

  another_store_ =
      CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(another_store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store_->GetCachedHash(kTestPolicyNS));
}

TEST_F(ComponentCloudPolicyStoreTest, StoreAndLoadPolicyWithCloudPriority) {
  store_ = CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
                       POLICY_SOURCE_PRIORITY_CLOUD);
  store_->SetCredentials(PolicyBuilder::kFakeUsername,
                         PolicyBuilder::kFakeGaiaId, PolicyBuilder::kFakeToken,
                         PolicyBuilder::kFakeDeviceId, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);

  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  builder_.payload().set_secure_hash(TestPolicyHash());
  SetupExpectBundleWithScope(POLICY_SCOPE_MACHINE,
                             POLICY_SOURCE_PRIORITY_CLOUD);

  StoreTestPolicyWithNamespace(store_.get(), kTestPolicyNS);

  another_store_ =
      CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
                  POLICY_SOURCE_PRIORITY_CLOUD);
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(another_store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store_->GetCachedHash(kTestPolicyNS));
}

TEST_F(ComponentCloudPolicyStoreTest,
       StoreAndLoadPolicyWithDifferentCloudPriority) {
  store_ = CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
                       POLICY_SOURCE_CLOUD);
  store_->SetCredentials(PolicyBuilder::kFakeUsername,
                         PolicyBuilder::kFakeGaiaId, PolicyBuilder::kFakeToken,
                         PolicyBuilder::kFakeDeviceId, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);

  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  builder_.payload().set_secure_hash(TestPolicyHash());
  SetupExpectBundleWithScope(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD);

  StoreTestPolicyWithNamespace(store_.get(), kTestPolicyNS);

  SetupExpectBundleWithScope(POLICY_SCOPE_MACHINE,
                             POLICY_SOURCE_PRIORITY_CLOUD);
  another_store_ =
      CreateStore(dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
                  POLICY_SOURCE_PRIORITY_CLOUD);
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(another_store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store_->GetCachedHash(kTestPolicyNS));
}

TEST_F(ComponentCloudPolicyStoreTest, Updates) {
  // Store some policies.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Deleting a non-existant namespace doesn't trigger updates.
  PolicyNamespace ns_fake(POLICY_DOMAIN_EXTENSIONS, "nosuchid");
  store_->Delete(ns_fake);
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Deleting a unowned domain doesn't trigger updates.
  PolicyNamespace ns_fake_2(POLICY_DOMAIN_SIGNIN_EXTENSIONS, kTestExtension);
  store_->Delete(ns_fake_2);
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Deleting a namespace that has policies triggers an update.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  store_->Delete(kTestPolicyNS);
  Mock::VerifyAndClearExpectations(&store_delegate_);
}

TEST_F(ComponentCloudPolicyStoreTest, Purge) {
  // Store a valid policy.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Purge other components.
  store_->Purge(base::BindRepeating(&NotEqual, kTestExtension));

  // The policy for |kTestPolicyNS| is still served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Loading the store again will still see |kTestPolicyNS|.
  another_store_ = CreateStore();
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(another_store_->policy().Equals(empty_bundle));
  another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  another_store_->Load();
  EXPECT_TRUE(another_store_->policy().Equals(expected_bundle_));

  // Now purge everything.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  store_->Purge(base::BindRepeating(&True));
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // No policies are served anymore.
  EXPECT_TRUE(store_->policy().Equals(empty_bundle));

  // And they aren't loaded anymore either.
  yet_another_store_ = CreateStore();
  yet_another_store_->SetCredentials(
      PolicyBuilder::kFakeUsername, PolicyBuilder::kFakeGaiaId,
      PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId, public_key_,
      PolicyBuilder::kFakePublicKeyVersion);
  yet_another_store_->Load();
  EXPECT_TRUE(yet_another_store_->policy().Equals(empty_bundle));
}

}  // namespace policy
