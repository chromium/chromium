// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/attestation_handler_impl.h"

#include "components/legion/attestation/server_evidence.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/messages.pb.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

namespace {

TEST(AttestationHandlerImplTest, GetAttestationRequest) {
  AttestationHandlerImpl attestation_handler;
  auto request = attestation_handler.GetAttestationRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->assertions().empty());
  EXPECT_TRUE(request->endorsed_evidence().empty());
}

TEST(AttestationHandlerImplTest, VerifyAttestationResponse) {
  AttestationHandlerImpl attestation_handler;
  AttestationEvidence evidence;
  EXPECT_TRUE(attestation_handler.VerifyAttestationResponse(evidence));
}

}  // namespace

}  // namespace legion
