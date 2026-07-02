// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_simple_task_environment.h"

#include <memory>
#include <string>
#include <utility>

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/client/attestation/attestation_verifier.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto_test_util.h"
#include "third_party/federated_compute/src/fcp/protos/confidentialcompute/signed_endorsements.pb.h"
#include "third_party/federated_compute/src/fcp/protos/federatedcompute/confidential_encryption_config.pb.h"
#include "third_party/federated_compute/src/fcp/protos/plan.pb.h"

namespace private_insights {

TEST(FcpSimpleTaskEnvironmentTest, CreateExampleIterator) {
  fcp::client::ExampleQueryResult query_result;
  fcp::client::ExampleQueryResult::VectorData::Values values;
  values.mutable_string_values()->add_value("example_data");
  (*query_result.mutable_vector_data()->mutable_vectors())["example"] = values;

  scoped_refptr<FcpSimpleTaskEnvironment> task_env =
      base::MakeRefCounted<FcpSimpleTaskEnvironment>("base_dir", "cache_dir",
                                                     nullptr, false);
  task_env->result() = query_result;

  google::internal::federated::plan::ExampleSelector selector;
  auto iterator_or = task_env->CreateExampleIterator(selector);
  ASSERT_TRUE(iterator_or.ok());
  ASSERT_NE(*iterator_or, nullptr);

  std::unique_ptr<fcp::client::ExampleIterator> iterator =
      *std::move(iterator_or);

  // First call should return the serialized query result.
  auto first_result = iterator->Next();
  ASSERT_TRUE(first_result.ok());
  EXPECT_EQ(*first_result, query_result.SerializeAsString());

  // Second call should return OutOfRange error.
  auto second_result = iterator->Next();
  EXPECT_FALSE(second_result.ok());
  EXPECT_EQ(second_result.status().code(), absl::StatusCode::kOutOfRange);

  // Closing the iterator should not crash and subsequent calls remain
  // OutOfRange.
  iterator->Close();
  auto third_result = iterator->Next();
  EXPECT_FALSE(third_result.ok());
  EXPECT_EQ(third_result.status().code(), absl::StatusCode::kOutOfRange);
}

TEST(FcpSimpleTaskEnvironmentTest, CreateAttestationVerifier) {
  scoped_refptr<FcpSimpleTaskEnvironment> task_env =
      base::MakeRefCounted<FcpSimpleTaskEnvironment>("base_dir", "cache_dir",
                                                     nullptr, false);

  auto verifier = task_env->CreateAttestationVerifier();
  ASSERT_NE(verifier, nullptr);

  auto [public_key, private_key] =
      fcp::confidential_compute::GenerateHpkeKeyPair("test_key_id");

  google::internal::federatedcompute::v1::ConfidentialEncryptionConfig
      encryption_config;
  encryption_config.set_public_key(public_key);

  fcp::confidentialcompute::SignedEndorsements signed_endorsements;
  absl::Cord access_policy;

  auto result =
      verifier->Verify(access_policy, signed_endorsements, encryption_config);

  ASSERT_TRUE(result.ok());
}

TEST(FcpSimpleTaskEnvironmentTest, CreateHttpClient) {
  scoped_refptr<FcpSimpleTaskEnvironment> task_env =
      base::MakeRefCounted<FcpSimpleTaskEnvironment>("base_dir", "cache_dir",
                                                     nullptr, false);
  auto http_client = task_env->CreateHttpClient();
  EXPECT_NE(http_client, nullptr);
}

}  // namespace private_insights
