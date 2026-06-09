// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mojom/unexportable_keys_mojom_traits.h"

#include "base/unguessable_token.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

TEST(UnexportableKeysTraitsTest, UnexportableKeyId) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  UnexportableKeyId input(token);
  UnexportableKeyId output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::UnexportableKeyId>(
      input, output));
  EXPECT_EQ(input, output);
}

TEST(UnexportableKeysTraitsTest, UnexportableSigningKeyId) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  UnexportableSigningKeyId input(token);
  UnexportableSigningKeyId output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::UnexportableSigningKeyId>(
          input, output));
  EXPECT_EQ(input, output);
}

TEST(UnexportableKeysTraitsTest, UnexportableAttestationKeyId) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  UnexportableAttestationKeyId input(token);
  UnexportableAttestationKeyId output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::UnexportableAttestationKeyId>(
          input, output));
  EXPECT_EQ(input, output);
}

TEST(UnexportableKeysTraitsTest, AttestationStatement) {
  crypto::AttestationStatement input;
  input.format = crypto::AttestationStatement::Format::kSecureEnclave;
  input.statement = {1, 2, 3};
  input.signature = {4, 5, 6};

  crypto::AttestationStatement output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AttestationStatement>(
      input, output));
  EXPECT_EQ(input.format, output.format);
  EXPECT_EQ(input.statement, output.statement);
  EXPECT_EQ(input.signature, output.signature);
}

}  // namespace unexportable_keys
