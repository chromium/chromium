// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_updater.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtension2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtension3[] = "cccccccccccccccccccccccccccccccc";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestDownload2[] = "http://example.com/getpolicy?id=456";
const char kTestDownload3[] = "http://example.com/getpolicy?id=789";
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

class MockComponentCloudPolicyStoreDelegate
    : public ComponentCloudPolicyStore::Delegate {
 public:
  ~MockComponentCloudPolicyStoreDelegate() override = default;

  MOCK_METHOD0(OnComponentCloudPolicyStoreUpdated, void());
};

}  // namespace

class ComponentCloudPolicyUpdaterTest : public testing::Test {
 protected:
  ComponentCloudPolicyUpdaterTest();
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<em::PolicyFetchResponse> CreateResponse();

  const PolicyNamespace kTestPolicyNS{POLICY_DOMAIN_EXTENSIONS, kTestExtension};
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<ResourceCache> cache_;
  std::unique_ptr<ComponentCloudPolicyStore> store_;
  MockComponentCloudPolicyStoreDelegate store_delegate_;
  network::TestURLLoaderFactory loader_factory_;
  std::unique_ptr<ComponentCloudPolicyUpdater> updater_;
  ComponentCloudPolicyBuilder builder_;
  PolicyBundle expected_bundle_;

 private:
  base::ScopedTempDir temp_dir_;
  std::string public_key_;
};

ComponentCloudPolicyUpdaterTest::ComponentCloudPolicyUpdaterTest()
    : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  builder_.SetDefaultSigningKey();
  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeExtensionPolicyType);
  builder_.policy_data().set_settings_entity_id(kTestExtension);
  builder_.payload().set_download_url(kTestDownload);
  builder_.payload().set_secure_hash(crypto::SHA256HashString(kTestPolicy));

  public_key_ = builder_.GetPublicSigningKeyAsString();

  PolicyMap& policy = expected_bundle_.Get(kTestPolicyNS);
  policy.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("disabled"), nullptr);
  policy.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("maybe"), nullptr);
}

void ComponentCloudPolicyUpdaterTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  cache_ = std::make_unique<ResourceCache>(temp_dir_.GetPath(),
                                           task_env_.GetMainThreadTaskRunner(),
                                           /* max_cache_size */ std::nullopt);
  store_ = std::make_unique<ComponentCloudPolicyStore>(
      &store_delegate_, cache_.get(), dm_protocol::kChromeExtensionPolicyType);
  store_->SetCredentials(PolicyBuilder::kFakeUsername,
                         PolicyBuilder::kFakeGaiaId, PolicyBuilder::kFakeToken,
                         PolicyBuilder::kFakeDeviceId, public_key_,
                         PolicyBuilder::kFakePublicKeyVersion);
  auto url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory_);
  updater_ = std::make_unique<ComponentCloudPolicyUpdater>(
      task_env_.GetMainThreadTaskRunner(),
      std::make_unique<ExternalPolicyDataFetcher>(
          std::move(url_loader_factory), task_env_.GetMainThreadTaskRunner()),
      store_.get());
  ASSERT_EQ(store_->policy().end(), store_->policy().begin());
}

void ComponentCloudPolicyUpdaterTest::TearDown() {
  updater_.reset();
  task_env_.RunUntilIdle();
}

std::unique_ptr<em::PolicyFetchResponse>
ComponentCloudPolicyUpdaterTest::CreateResponse() {
  builder_.Build();
  return std::make_unique<em::PolicyFetchResponse>(builder_.policy());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchAndCache) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that a download has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);
  task_env_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseTooLarge) {
  // Submit a policy fetch response that exceeds the allowed maximum size.
  std::string long_download("http://example.com/get?id=");
  long_download.append(20 * 1024, '1');
  builder_.payload().set_download_url(long_download);
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  // Submit two valid policy fetch responses.
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension2),
      CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension3),
      CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first policy fetch response has been ignored and downloads
  // have been started for the next two fetch responses instead.
  EXPECT_EQ(2, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload2));
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload3));
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseInvalid) {
  // Submit an invalid policy fetch response.
  builder_.policy_data().set_username("wronguser@example.com");
  builder_.policy_data().set_gaia_id("wrong-gaia-id");
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  // Submit two valid policy fetch responses.
  builder_.policy_data().set_username(PolicyBuilder::kFakeUsername);
  builder_.policy_data().set_gaia_id(PolicyBuilder::kFakeGaiaId);
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension2),
      CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension3),
      CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first policy fetch response has been ignored and downloads
  // have been started for the next two fetch responses instead.
  EXPECT_EQ(2, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload2));
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload3));
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseNoSignature) {
  // Submit an invalid policy fetch response.
  builder_.UnsetSigningKey();
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseBadSignature) {
  // Submit an invalid policy fetch response.
  std::unique_ptr<em::PolicyFetchResponse> response = CreateResponse();
  response->set_policy_data_signature("invalid");
  updater_->UpdateExternalPolicy(kTestPolicyNS, std::move(response));

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseWrongPublicKey) {
  // Submit a policy fetch response signed with a wrong signing key.
  builder_.SetSigningKey(*PolicyBuilder::CreateTestOtherSigningKey());
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest,
       PolicyFetchResponseWrongPublicKeyVersion) {
  // Submit a policy fetch response containing different public key version.
  builder_.policy_data().set_public_key_version(
      PolicyBuilder::kFakePublicKeyVersion + 1);
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseDifferentPublicKey) {
  // Submit a policy fetch response signed with a different key and containing a
  // new public key version.
  builder_.SetSigningKey(*PolicyBuilder::CreateTestOtherSigningKey());
  builder_.policy_data().set_public_key_version(
      PolicyBuilder::kFakePublicKeyVersion + 1);
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseEmptyComponentId) {
  // Submit a policy fetch response having an empty component ID.
  builder_.policy_data().set_settings_entity_id(std::string());
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()),
      CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());

  // Submit a policy fetch response having an empty component ID with empty data
  // fields, requesting the deletion of policy.
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()),
      CreateResponse());

  task_env_.RunUntilIdle();

  // Verify that the policy fetch response has been ignored.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, AlreadyCached) {
  // Cache policy for an extension.
  builder_.Build();
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(
      store_->Store(kTestPolicyNS, builder_.GetBlob(), &builder_.policy_data(),
                    crypto::SHA256HashString(kTestPolicy), kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Submit a policy fetch response whose extension ID and hash match the
  // already cached policy.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that no download has been started.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, DataTooLarge) {
  // Submit three policy fetch responses.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  builder_.payload().set_download_url(kTestDownload2);
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension2),
      CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension3),
      CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first and second downloads have been started.
  EXPECT_EQ(2, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload2));

  // Complete the request with a policy data size that exceeds allowed maximum.
  loader_factory_.AddResponse(kTestDownload2,
                              std::string(6 * 1024 * 1024, 'a'));
  task_env_.RunUntilIdle();

  // Verify that the third download has been started.
  EXPECT_EQ(2, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload3));
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedData) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first download has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Submit a second policy fetch response for the same extension with an
  // updated download URL.
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first download is no longer running and the second download
  // has been started.
  EXPECT_EQ(1, loader_factory_.NumPending());
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload2));
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedDataWithoutPolicy) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the download has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);
  task_env_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Submit a second policy fetch response for the same extension with no
  // download URL, meaning that no policy should be provided for this extension.
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  Mock::VerifyAndClearExpectations(&store_delegate_);
  task_env_.RunUntilIdle();

  // Verify that no download has been started.
  EXPECT_EQ(0, loader_factory_.NumPending());

  // Verify that the policy is no longer being served.
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(store_->policy().Equals(kEmptyBundle));
}

TEST_F(ComponentCloudPolicyUpdaterTest, NoPolicy) {
  // Submit a policy fetch response with a valid download URL.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the download has been started.
  EXPECT_EQ(1, loader_factory_.NumPending());

  // Update the policy fetch response before the download has finished. The new
  // policy fetch response has no download URL.
  builder_.payload().Clear();
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the download is no longer running.
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, CancelUpdate) {
  // Submit a policy fetch response with a valid download URL.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the download has been started.
  EXPECT_EQ(1, loader_factory_.NumPending());

  // Now cancel that update before the download completes.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated()).Times(0);
  updater_->CancelUpdate(kTestPolicyNS);
  task_env_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_EQ(0, loader_factory_.NumPending());
}

TEST_F(ComponentCloudPolicyUpdaterTest, RetryAfterDataTooLarge) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first download has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Indicate that the policy data size will exceed allowed maximum.
  loader_factory_.AddResponse(kTestDownload, std::string(6 * 1024 * 1024, 'a'));
  task_env_.RunUntilIdle();
  loader_factory_.ClearResponses();

  // After 12 hours (minus some random jitter), the next download attempt
  // happens.
  EXPECT_EQ(0, loader_factory_.NumPending());
  task_env_.FastForwardBy(base::Hours(12));
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);
  task_env_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
}

TEST_F(ComponentCloudPolicyUpdaterTest, RetryAfterDataValidationFails) {
  // Submit a policy fetch response that is calculated for an empty (that is,
  // invalid) JSON.
  builder_.payload().set_secure_hash(crypto::SHA256HashString(std::string()));
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // Verify that the first download has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download with an invalid (empty) JSON.
  loader_factory_.AddResponse(kTestDownload, std::string());
  task_env_.RunUntilIdle();
  loader_factory_.ClearResponses();

  // Verify that no policy is being served.
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(store_->policy().Equals(kEmptyBundle));

  // After 12 hours (minus some random jitter), the next download attempt
  // happens.
  EXPECT_EQ(0, loader_factory_.NumPending());
  task_env_.FastForwardBy(base::Hours(12));
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download with an invalid (empty) JSON. This tests against the
  // regression that was tracked at https://crbug.com/706781.
  loader_factory_.AddResponse(kTestDownload, std::string());
  task_env_.RunUntilIdle();
  loader_factory_.ClearResponses();

  // Submit a policy fetch response that is calculated for the correct JSON.
  builder_.payload().set_secure_hash(crypto::SHA256HashString(kTestPolicy));
  updater_->UpdateExternalPolicy(kTestPolicyNS, CreateResponse());
  task_env_.RunUntilIdle();

  // The next download attempt has been started.
  EXPECT_TRUE(loader_factory_.IsPending(kTestDownload));

  // Complete the download.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  loader_factory_.AddResponse(kTestDownload, kTestPolicy);
  task_env_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
}

}  // namespace policy
