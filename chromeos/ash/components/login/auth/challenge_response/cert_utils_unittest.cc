// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"

#include <string>
#include <vector>

#include "base/hash/sha1.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using KeySignatureAlgorithm = ChallengeResponseKey::SignatureAlgorithm;

namespace {

class ChallengeResponseCertUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    certificate_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "nist.der");
    ASSERT_TRUE(certificate_);
  }

  const net::X509Certificate& certificate() const { return *certificate_; }

 private:
  scoped_refptr<net::X509Certificate> certificate_;
};

}  // namespace

TEST_F(ChallengeResponseCertUtilsTest, Success) {
  const std::vector<KeySignatureAlgorithm> kSignatureAlgorithms = {
      KeySignatureAlgorithm::kRsassaPkcs1V15Sha512,
      KeySignatureAlgorithm::kRsassaPkcs1V15Sha256};

  ChallengeResponseKey challenge_response_key;
  ASSERT_TRUE(ExtractChallengeResponseKeyFromCert(
      certificate(), kSignatureAlgorithms, &challenge_response_key));

  EXPECT_EQ(base::SHA1HashString(challenge_response_key.public_key_spki_der()),
            kNistSPKIHash);
  EXPECT_EQ(challenge_response_key.signature_algorithms(),
            kSignatureAlgorithms);
}

TEST_F(ChallengeResponseCertUtilsTest, EmptyAlgorithmsFailure) {
  ChallengeResponseKey challenge_response_key;
  EXPECT_FALSE(ExtractChallengeResponseKeyFromCert(
      certificate(), {} /* signature_algorithms */, &challenge_response_key));
}

}  // namespace ash
