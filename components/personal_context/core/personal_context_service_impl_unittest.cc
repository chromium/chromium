// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_key_manager.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {

namespace {

using ::base::test::TestMessage;

proto::FetchContextResponse BuildFetchContextResponse(std::string_view output) {
  proto::FetchContextResponse fetch_response;
  proto::Any* any_metadata = fetch_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/test.Message");
  any_metadata->set_value(std::string(output));
  return fetch_response;
}

class PersonalContextServiceImplTest : public testing::Test {
 public:
  PersonalContextServiceImplTest() = default;
  ~PersonalContextServiceImplTest() override = default;

  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    personal_context_service_ = std::make_unique<PersonalContextServiceImpl>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        &pref_service_);
  }

  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://contextmemoryservice.pa.googleapis.com/v1:fetchContext",
        content, http_status, network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse() {
    std::string serialized_response;
    proto::FetchContextResponse fetch_response =
        BuildFetchContextResponse("foo response");
    fetch_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

  PersonalContextServiceImpl* personal_context_service() {
    return personal_context_service_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPersonalContext};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PersonalContextServiceImpl> personal_context_service_;
};

TEST_F(PersonalContextServiceImplTest, FetchContextDelegatesToManager) {
  SetAutomaticIssueOfAccessTokens();

  base::test::TestFuture<FetchContextResult> future;

  ContextMemoryRequestOptions options;
  personal_context_service()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, TestMessage(), options,
      future.GetCallback());

  EXPECT_TRUE(SimulateSuccessfulResponse());

  FetchContextResult result = future.Take();
  ASSERT_TRUE(result.response.has_value());
  ASSERT_EQ("foo response", result.response.value().value());
}

TEST_F(PersonalContextServiceImplTest, FetchPiiEntitiesDelegatesToManager) {
  SetAutomaticIssueOfAccessTokens();

  base::test::TestFuture<FetchPiiEntitiesResult> future;

  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  ContextMemoryRequestOptions options;
  personal_context_service()->FetchPiiEntities(request, options,
                                               future.GetCallback());

  proto::FetchPiiEntitiesResponse pii_response;
  pii_response.set_server_request_id("test_id");
  std::string serialized_response;
  pii_response.SerializeToString(&serialized_response);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://contextmemoryservice.pa.googleapis.com/v1:fetchPiiEntities",
      serialized_response, net::HTTP_OK,
      network::TestURLLoaderFactory::kUrlMatchPrefix);

  FetchPiiEntitiesResult result = future.Take();
  ASSERT_TRUE(result.response.has_value());
  EXPECT_EQ("test_id", result.response.value().server_request_id());
}

TEST_F(PersonalContextServiceImplTest, DecryptEntitySuccess) {
  PersonalContextKeyManager key_manager(&pref_service_);

  proto::DecryptedEntity decrypted_entity;
  decrypted_entity.mutable_passport()->set_full_name("Jane Doe");
  decrypted_entity.mutable_passport()->set_number("123456789");
  decrypted_entity.mutable_passport()->set_issuing_country("US");

  proto::DecryptedReference* gmail_ref = decrypted_entity.add_references();
  gmail_ref->mutable_gmail_message()->set_subject("Your Passport Application");
  gmail_ref->mutable_gmail_message()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  proto::DecryptedReference* drive_ref = decrypted_entity.add_references();
  drive_ref->mutable_drive_file()->set_name("passport_scan.pdf");
  drive_ref->mutable_drive_file()->set_url(
      "https://drive.google.com/file/d/456");

  std::string serialized_entity = decrypted_entity.SerializeAsString();
  std::optional<std::vector<uint8_t>> ciphertext = key_manager.Seal(
      key_manager.GetPublicKey(), base::as_byte_span(serialized_entity));
  ASSERT_TRUE(ciphertext.has_value());

  proto::Entity entity;
  entity.set_encrypted_entity(
      std::string(ciphertext->begin(), ciphertext->end()));

  base::HistogramTester histogram_tester;
  std::optional<proto::DecryptedEntity> result =
      personal_context_service()->DecryptEntity(entity);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->passport().full_name(), "Jane Doe");
  EXPECT_EQ(result->passport().number(), "123456789");
  EXPECT_EQ(result->passport().issuing_country(), "US");

  ASSERT_EQ(result->references_size(), 2);
  EXPECT_EQ(result->references(0).gmail_message().subject(),
            "Your Passport Application");
  EXPECT_EQ(result->references(0).gmail_message().message_url(),
            "https://mail.google.com/mail/u/0/#inbox/123");
  EXPECT_EQ(result->references(1).drive_file().name(), "passport_scan.pdf");
  EXPECT_EQ(result->references(1).drive_file().url(),
            "https://drive.google.com/file/d/456");

  histogram_tester.ExpectUniqueSample("PersonalContext.DecryptEntity.Result",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.DecryptEntity.Status",
      PersonalContextDecryptionStatus::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PersonalContext.DecryptEntity.Latency", 1);
}

TEST_F(PersonalContextServiceImplTest, DecryptEntityNullKeyManager) {
  PersonalContextServiceImpl service_without_prefs(
      url_loader_factory_, identity_test_env_.identity_manager(),
      /*pref_service=*/nullptr);

  base::HistogramTester histogram_tester;
  proto::Entity entity;
  entity.set_encrypted_entity("some_ciphertext");
  EXPECT_EQ(service_without_prefs.DecryptEntity(entity), std::nullopt);

  histogram_tester.ExpectUniqueSample("PersonalContext.DecryptEntity.Result",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.DecryptEntity.Status",
      PersonalContextDecryptionStatus::kNoKeyManager,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PersonalContext.DecryptEntity.Latency", 1);
}

TEST_F(PersonalContextServiceImplTest, DecryptEntityEmptyEntityReturnsNullopt) {
  base::HistogramTester histogram_tester;
  proto::Entity empty_entity;
  empty_entity.set_encrypted_entity("");
  EXPECT_EQ(personal_context_service()->DecryptEntity(empty_entity),
            std::nullopt);

  histogram_tester.ExpectUniqueSample("PersonalContext.DecryptEntity.Result",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.DecryptEntity.Status",
      PersonalContextDecryptionStatus::kEmptyEncryptedEntity,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PersonalContext.DecryptEntity.Latency", 1);
}

TEST_F(PersonalContextServiceImplTest,
       DecryptEntityCorruptedCiphertextReturnsNullopt) {
  base::HistogramTester histogram_tester;
  proto::Entity entity;
  entity.set_encrypted_entity("invalid_ciphertext");
  EXPECT_EQ(personal_context_service()->DecryptEntity(entity), std::nullopt);

  histogram_tester.ExpectUniqueSample("PersonalContext.DecryptEntity.Result",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.DecryptEntity.Status",
      PersonalContextDecryptionStatus::kDecryptionFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PersonalContext.DecryptEntity.Latency", 1);
}

TEST_F(PersonalContextServiceImplTest,
       DecryptEntityInvalidProtoPayloadReturnsNullopt) {
  PersonalContextKeyManager key_manager(&pref_service_);

  const std::string invalid_proto_bytes = "\xFF\xFF\xFF\xFF\xFF";
  std::optional<std::vector<uint8_t>> ciphertext = key_manager.Seal(
      key_manager.GetPublicKey(), base::as_byte_span(invalid_proto_bytes));
  ASSERT_TRUE(ciphertext.has_value());

  proto::Entity entity;
  entity.set_encrypted_entity(
      std::string(ciphertext->begin(), ciphertext->end()));
  base::HistogramTester histogram_tester;
  EXPECT_EQ(personal_context_service()->DecryptEntity(entity), std::nullopt);

  histogram_tester.ExpectUniqueSample("PersonalContext.DecryptEntity.Result",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.DecryptEntity.Status",
      PersonalContextDecryptionStatus::kProtoParseFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PersonalContext.DecryptEntity.Latency", 1);
}

}  // namespace

}  // namespace personal_context
