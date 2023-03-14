// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_relationship_verification/response_header_verifier.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content_relationship_verification {

TEST(ResponseHeaderVerifier, VerifyEmptyHeader) {
  EXPECT_EQ(ResponseHeaderVerificationResult::kMissing,
            ResponseHeaderVerifier::Verify("any.package.name", ""));
}

TEST(ResponseHeaderVerifier, VerifyStar) {
  EXPECT_EQ(ResponseHeaderVerificationResult::kAllow,
            ResponseHeaderVerifier::Verify("any.package.name", "*"));
}

TEST(ResponseHeaderVerifier, VerifyNone) {
  EXPECT_EQ(ResponseHeaderVerificationResult::kDisallow,
            ResponseHeaderVerifier::Verify("any.package.name", "none"));
}

TEST(ResponseHeaderVerifier, VerifyListOfPackageNames) {
  EXPECT_EQ(ResponseHeaderVerificationResult::kAllow,
            ResponseHeaderVerifier::Verify(
                "one.package", "one.package, two.package, three.package"));
  EXPECT_EQ(ResponseHeaderVerificationResult::kAllow,
            ResponseHeaderVerifier::Verify(
                "two.package", "one.package, two.package, three.package"));
  EXPECT_EQ(ResponseHeaderVerificationResult::kAllow,
            ResponseHeaderVerifier::Verify(
                "three.package", "one.package, two.package, three.package"));

  EXPECT_EQ(ResponseHeaderVerificationResult::kDisallow,
            ResponseHeaderVerifier::Verify(
                "unknown.package", "one.package, two.package, three.package"));
  EXPECT_EQ(ResponseHeaderVerificationResult::kDisallow,
            ResponseHeaderVerifier::Verify("any.package", "any.package.name"));

  // 'none' and '*' get ignored if package names are listed.
  EXPECT_EQ(ResponseHeaderVerificationResult::kAllow,
            ResponseHeaderVerifier::Verify("a.package", "none, a.package"));
  EXPECT_EQ(ResponseHeaderVerificationResult::kDisallow,
            ResponseHeaderVerifier::Verify("another.package", "*, a.package"));
}

}  // namespace content_relationship_verification
