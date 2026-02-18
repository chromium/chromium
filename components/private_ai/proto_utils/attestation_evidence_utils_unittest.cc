// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/attestation_evidence_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "components/private_ai/attestation/server_evidence.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/attestation/endorsement.pb.h"
#include "third_party/oak/chromium/proto/crypto/certificate.pb.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/variant.pb.h"

namespace private_ai {

namespace {

constexpr char kEndorsedEvidenceKey[] = "test key";

TEST(ConvertToAttestationEvidenceTest, Success) {
  oak::session::v1::AttestResponse response;
  const std::string kMessage = "test message";
  const std::string kSignature = "test signature";

  {
    auto& endorsed_evidence =
        (*response.mutable_endorsed_evidence())[kEndorsedEvidenceKey];

    oak::attestation::v1::SessionBindingPublicKeyEndorsement endorsement{};
    auto* certificate =
        endorsement.mutable_ca_endorsement()->mutable_certificate();
    certificate->set_serialized_payload(kMessage);
    certificate->mutable_signature_info()->set_signature(kSignature);

    std::string endorsement_str{};
    endorsement.SerializeToString(&endorsement_str);

    auto* event = endorsed_evidence.mutable_endorsements()->add_events();
    event->set_value(endorsement_str);
  }

  auto result = ConvertToAttestationEvidence(response);
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(result->endorsed_evidence.size(), 1u);
  const auto& result_endorsed_evidence =
      result->endorsed_evidence.at(kEndorsedEvidenceKey);
  ASSERT_EQ(result_endorsed_evidence.endorsements.size(), 1u);
  EXPECT_EQ(result_endorsed_evidence.endorsements[0].message,
            base::ToVector(base::as_byte_span(kMessage)));
  EXPECT_EQ(result_endorsed_evidence.endorsements[0].signature,
            base::ToVector(base::as_byte_span(kSignature)));
}

TEST(ConvertToAttestationEvidenceTest, NoMatchingEndorsements) {
  oak::session::v1::AttestResponse response;
  {
    auto& endorsed_evidence =
        (*response.mutable_endorsed_evidence())[kEndorsedEvidenceKey];

    // Add an event that is not a SessionBindingPublicKeyEndorsement.
    auto* event = endorsed_evidence.mutable_endorsements()->add_events();
    event->set_value("not a valid endorsement");
  }

  auto result = ConvertToAttestationEvidence(response);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->endorsed_evidence.empty());
}

TEST(ConvertToAttestationEvidenceTest, MissingEndorsements) {
  oak::session::v1::AttestResponse response;
  {
    // Add an endorsed evidence entry that is missing endorsements.
    (*response.mutable_endorsed_evidence())[kEndorsedEvidenceKey];
  }

  auto result = ConvertToAttestationEvidence(response);
  EXPECT_FALSE(result.has_value());
}

TEST(ConvertToAttestationEvidenceTest, EmptyResponse) {
  oak::session::v1::AttestResponse response;
  auto result = ConvertToAttestationEvidence(response);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->endorsed_evidence.empty());
}

}  // namespace

}  // namespace private_ai
