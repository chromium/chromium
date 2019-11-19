// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_service.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AtLeast;
using testing::Mock;
using testing::Return;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtension2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
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

const char kInvalidTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"published\""
    "  },"
    "  \"Undeclared Name\": {"
    "    \"Value\": \"not published\""
    "  }"
    "}";

const char kTestSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"Name\": { \"type\": \"string\" },"
    "    \"Second\": { \"type\": \"string\" }"
    "  }"
    "}";

class MockComponentCloudPolicyDelegate
    : public ComponentCloudPolicyService::Delegate {
 public:
  ~MockComponentCloudPolicyDelegate() override {}

  MOCK_METHOD0(OnComponentCloudPolicyUpdated, void());
};

}  // namespace

class ComponentCloudPolicyServiceTest : public testing::Test {
 protected:
  ComponentCloudPolicyServiceTest()
      : cache_(nullptr),
        client_(nullptr),
        core_(dm_protocol::kChromeUserPolicyType,
              std::string(),
              &store_,
              base::ThreadTaskRunnerHandle::Get(),
              network::TestNetworkConnectionTracker::CreateGetter()) {
    builder_.SetDefaultSigningKey();
    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(crypto::SHA256HashString(kTestPolicy));

    public_key_ = builder_.GetPublicSigningKeyAsString();

    expected_policy_.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD,
                         std::make_unique<base::Value>("disabled"), nullptr);
    expected_policy_.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD,
                         std::make_unique<base::Value>("maybe"), nullptr);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    owned_cache_.reset(new ResourceCache(temp_dir_.GetPath(),
                                         base::ThreadTaskRunnerHandle::Get(),
                                         /* max_cache_size */ base::nullopt));
    cache_ = owned_cache_.get();
  }

  void TearDown() override {
    // The service cleans up its backend on the background thread.
    service_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void Connect() {
    client_ = new MockCloudPolicyClient(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &loader_factory_));
    service_.reset(new ComponentCloudPolicyService(
        dm_protocol::kChromeExtensionPolicyType, POLICY_SOURCE_CLOUD,
        &delegate_, &registry_, &core_, client_, std::move(owned_cache_),
        base::ThreadTaskRunnerHandle::Get()));

    client_->SetDMToken(ComponentCloudPolicyBuilder::kFakeToken);
    EXPECT_EQ(1u, client_->types_to_fetch_.size());
    core_.Connect(std::unique_ptr<CloudPolicyClient>(client_));
    EXPECT_EQ(2u, client_->types_to_fetch_.size());

    // Also initialize the refresh scheduler, so that calls to
    // core()->RefreshSoon() trigger a FetchPolicy() call on the mock |client_|.
    // The |service_| should never trigger new fetches.
    EXPECT_CALL(*client_, FetchPolicy());
    core_.StartRefreshScheduler();
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(client_);
  }

  void LoadStore() {
    em::PolicyData* data = new em::PolicyData();
    data->set_username(PolicyBuilder::kFakeUsername);
    data->set_request_token(PolicyBuilder::kFakeToken);
    data->set_device_id(PolicyBuilder::kFakeDeviceId);
    data->set_public_key_version(PolicyBuilder::kFakePublicKeyVersion);
    store_.policy_.reset(data);
    store_.policy_signature_public_key_ = public_key_;
    store_.NotifyStoreLoaded();
    RunUntilIdle();
    EXPECT_TRUE(store_.is_initialized());
  }

  void InitializeRegistry() {
    registry_.RegisterComponent(
        PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension),
        CreateTestSchema());
    registry_.SetAllDomainsReady();
  }

  void PopulateCache() {
    EXPECT_FALSE(cache_
                     ->Store("extension-policy", kTestExtension,
                             CreateSerializedResponse())
                     .empty());
    EXPECT_FALSE(
        cache_->Store("extension-policy-data", kTestExtension, kTestPolicy)
            .empty());

    builder_.policy_data().set_settings_entity_id(kTestExtension2);
    EXPECT_FALSE(cache_
                     ->Store("extension-policy", kTestExtension2,
                             CreateSerializedResponse())
                     .empty());
    EXPECT_FALSE(
        cache_->Store("extension-policy-data", kTestExtension2, kTestPolicy)
            .empty());
    builder_.policy_data().set_settings_entity_id(kTestExtension);
  }

  std::unique_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return std::make_unique<em::PolicyFetchResponse>(builder_.policy());
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  Schema CreateTestSchema() {
    std::string error;
    Schema schema = Schema::Parse(kTestSchema, &error);
    EXPECT_TRUE(schema.valid()) << error;
    return schema;
  }

  const PolicyNamespace kTestExtensionNS =
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  const PolicyNamespace kTestExtensionNS2 =
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension2);

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  network::TestURLLoaderFactory loader_factory_;
  MockComponentCloudPolicyDelegate delegate_;
  // |cache_| is owned by the |service_| and is invalid once the |service_|
  // is destroyed.
  std::unique_ptr<ResourceCache> owned_cache_;
  ResourceCache* cache_;
  MockCloudPolicyClient* client_;
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;
  SchemaRegistry registry_;
  std::unique_ptr<ComponentCloudPolicyService> service_;
  ComponentCloudPolicyBuilder builder_;
  PolicyMap expected_policy_;
  std::string public_key_;
};

TEST_F(ComponentCloudPolicyServiceTest, InitializeStoreThenRegistry) {
  Connect();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated()).Times(0);
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_FALSE(service_->is_initialized());

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  InitializeRegistry();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializeRegistryThenStore) {
  Connect();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated()).Times(0);
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  InitializeRegistry();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_FALSE(service_->is_initialized());

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(2u, client_->types_to_fetch_.size());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializeWithCachedPolicy) {
  PopulateCache();
  Connect();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  InitializeRegistry();
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(2u, client_->types_to_fetch_.size());

  // Policies for both extensions are stored in the cache.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(base::Contains(contents, kTestExtension));
  EXPECT_TRUE(base::Contains(contents, kTestExtension2));

  // Only policy for extension 1 is now being served, as the registry contains
  // only its schema.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // Register extension 2. Its policy gets loaded without any additional
  // policy fetches.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  registry_.RegisterComponent(kTestExtensionNS2, CreateTestSchema());
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Policies for both extensions are being served now.
  expected_bundle.Get(kTestExtensionNS2).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, FetchPolicy) {
  Connect();
  // Initialize the store. A refresh is not needed, because no components are
  // registered yet.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  registry_.SetAllDomainsReady();
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Register the components to fetch. The |service_| issues a new update
  // because the new schema may filter different policies from the store.
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  registry_.RegisterComponent(kTestExtensionNS, CreateTestSchema());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Send back a fake policy fetch response.
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();

  // That should have triggered the download fetch.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, FetchPolicyBeforeStoreLoaded) {
  Connect();
  // A fake policy is fetched.
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();

  // Initialize the store. A refresh is not needed, because no components are
  // registered yet.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  registry_.SetAllDomainsReady();
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Register the components to fetch. The |service_| issues a new update
  // because the new schema may filter different policies from the store.
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  registry_.RegisterComponent(kTestExtensionNS, CreateTestSchema());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  // The download started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest,
       FetchPolicyWithCachedPolicyBeforeStoreLoaded) {
  PopulateCache();
  Connect();
  // A fake policy is fetched.
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();

  // Initialize the store.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated()).Times(AtLeast(1));
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  InitializeRegistry();
  LoadStore();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Only policy for extension 1 is served. Policy for extension 2, which was in
  // the cache initially, is now dropped.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, LoadCacheAndDeleteExtensions) {
  Connect();
  // Insert data in the cache.
  PopulateCache();
  registry_.RegisterComponent(kTestExtensionNS2, CreateTestSchema());
  InitializeRegistry();

  // Load the initial cache.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  expected_bundle.Get(kTestExtensionNS2).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // Now purge one of the extensions. This generates a notification after an
  // immediate filtering.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  registry_.UnregisterComponent(kTestExtensionNS);
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy served for extension 1 becomes empty.
  expected_bundle.Get(kTestExtensionNS).Clear();
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // The cache still keeps policies for both extensions.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_TRUE(base::Contains(contents, kTestExtension));
  EXPECT_TRUE(base::Contains(contents, kTestExtension2));
}

TEST_F(ComponentCloudPolicyServiceTest, SignInAfterStartup) {
  registry_.SetAllDomainsReady();

  // Initialize the store without credentials.
  EXPECT_FALSE(store_.is_initialized());
  store_.NotifyStoreLoaded();
  RunUntilIdle();

  // Register an extension.
  registry_.RegisterComponent(kTestExtensionNS, CreateTestSchema());
  RunUntilIdle();

  // Now signin. The service will finish loading its backend (which is empty
  // for now, because there are no credentials) and issue a notification.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  Connect();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Send the response to the service. The response data will be ignored,
  // because the store doesn't have the updated credentials yet.
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();

  // The policy was ignored and no download is started because the store
  // doesn't have credentials.
  EXPECT_EQ(0, loader_factory_.NumPending());

  // Now update the |store_| with the updated policy, which includes
  // credentials. The responses in the |client_| will be reloaded.
  LoadStore();

  // The extension policy was validated this time, and the download is started.
  ASSERT_EQ(1, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, SignOut) {
  // Initialize everything and serve policy for a component.
  PopulateCache();
  LoadStore();
  InitializeRegistry();

  // The initial, cached policy will be served once the backend is initialized.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  Connect();
  Mock::VerifyAndClearExpectations(&delegate_);
  // Policy for extension 1 is now being served.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
  // The cache still contains policies for both extensions.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(2u, contents.size());

  // Signing out removes all of the component policies from the service and
  // from the cache. It does not trigger a refresh.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  core_.Disconnect();
  store_.policy_.reset();
  Mock::VerifyAndClearExpectations(&store_);
  store_.NotifyStoreLoaded();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(0u, contents.size());
}

TEST_F(ComponentCloudPolicyServiceTest, LoadInvalidPolicyFromCache) {
  // Put the invalid test policy in the cache. One of its policies will be
  // loaded, the other should be filtered out by the schema.
  builder_.payload().set_secure_hash(
      crypto::SHA256HashString(kInvalidTestPolicy));
  EXPECT_FALSE(cache_
                   ->Store("extension-policy", kTestExtension,
                           CreateSerializedResponse())
                   .empty());
  EXPECT_FALSE(
      cache_->Store("extension-policy-data", kTestExtension, kInvalidTestPolicy)
          .empty());

  LoadStore();
  InitializeRegistry();

  // The initial, cached policy will be served once the backend is initialized.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  Connect();
  Mock::VerifyAndClearExpectations(&delegate_);

  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS)
      .Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("published"),
           nullptr);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, PurgeWhenServerRemovesPolicy) {
  // Initialize with cached policy.
  PopulateCache();
  Connect();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(*client_, FetchPolicy()).Times(0);
  registry_.RegisterComponent(kTestExtensionNS2, CreateTestSchema());
  InitializeRegistry();
  LoadStore();
  Mock::VerifyAndClearExpectations(client_);
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(2u, client_->types_to_fetch_.size());

  // Verify that policy for 2 extensions has been loaded from the cache.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(base::Contains(contents, kTestExtension));
  EXPECT_TRUE(base::Contains(contents, kTestExtension2));

  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  expected_bundle.Get(kTestExtensionNS2).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // Receive an updated fetch response from the server. There is no response for
  // extension 2, so it will be dropped from the cache. This triggers an
  // immediate notification to the delegate.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The cache should have dropped the entries for the second extension.
  contents.clear();
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(base::Contains(contents, kTestExtension));
  EXPECT_FALSE(base::Contains(contents, kTestExtension2));

  // And the service isn't publishing policy for the second extension anymore.
  expected_bundle.Clear();
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, KeyRotation) {
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  Connect();
  LoadStore();
  InitializeRegistry();
  RunUntilIdle();
  EXPECT_TRUE(service_->is_initialized());

  // Send back a fake policy fetch response with the new signing key.
  const int kNewPublicKeyVersion = PolicyBuilder::kFakePublicKeyVersion + 1;
  std::unique_ptr<crypto::RSAPrivateKey> new_signing_key =
      PolicyBuilder::CreateTestOtherSigningKey();
  builder_.SetSigningKey(*new_signing_key);
  builder_.policy_data().set_public_key_version(kNewPublicKeyVersion);
  client_->SetPolicy(dm_protocol::kChromeExtensionPolicyType, kTestExtension,
                     *CreateResponse());
  service_->OnPolicyFetched(client_);
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_);

  // The download fetch shouldn't have been triggered, as the component policy
  // validation should fail due to the old key returned by the store.
  ASSERT_EQ(0, loader_factory_.NumPending());

  // Update the signing key in the store.
  store_.policy_signature_public_key_ = builder_.GetPublicSigningKeyAsString();
  store_.policy_->set_public_key_version(kNewPublicKeyVersion);
  store_.NotifyStoreLoaded();
  RunUntilIdle();

  // The validation of the component policy should have finished successfully
  // and trigger the download fetch.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

}  // namespace policy
