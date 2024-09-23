// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key.h"

#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "crypto/ec_private_key.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

TEST(ECPrivateKeyTest, KeyWorksAsExpected) {
  auto ec_private_key =
      base::MakeRefCounted<ECPrivateKey>(crypto::ECPrivateKey::Create());

  EXPECT_EQ(ec_private_key->GetAlgorithm(),
            crypto::SignatureVerifier::ECDSA_SHA256);

  auto spki_bytes = ec_private_key->GetSubjectPublicKeyInfo();
  EXPECT_GT(spki_bytes.size(), 0U);

  auto signature = ec_private_key->SignSlowly(spki_bytes);
  ASSERT_TRUE(signature.has_value());
  EXPECT_GT(signature->size(), 0U);

  auto proto_key = ec_private_key->ToProto();
  EXPECT_EQ(proto_key.source(),
            client_certificates_pb::PrivateKey::PRIVATE_SOFTWARE_KEY);
  EXPECT_GT(proto_key.wrapped_key().size(), 0U);
}

}  // namespace client_certificates
