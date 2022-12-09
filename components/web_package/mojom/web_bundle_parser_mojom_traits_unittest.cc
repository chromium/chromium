// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/containers/span.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsTrue;

constexpr uint8_t kTestPublicKey[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
                                      2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
                                      3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

constexpr uint8_t kTestSignature[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

}  // namespace

TEST(StructTraitsTest, Ed25519PublicKey) {
  auto input = Ed25519PublicKey::Create(base::make_span(kTestPublicKey));
  EXPECT_THAT(input.bytes(), ElementsAreArray(kTestPublicKey));

  Ed25519PublicKey output;
  ASSERT_THAT(
      mojo::test::SerializeAndDeserialize<web_package::mojom::Ed25519PublicKey>(
          input, output),
      IsTrue());

  ASSERT_THAT(output.bytes_.has_value(), IsTrue());
  EXPECT_THAT(input, Eq(output));
}

TEST(StructTraitsTest, Ed25519Signature) {
  auto input = Ed25519Signature::Create(base::make_span(kTestSignature));
  EXPECT_THAT(input.bytes(), ElementsAreArray(kTestSignature));

  Ed25519Signature output;
  ASSERT_THAT(
      mojo::test::SerializeAndDeserialize<web_package::mojom::Ed25519Signature>(
          input, output),
      IsTrue());

  ASSERT_THAT(output.bytes_.has_value(), IsTrue());
  EXPECT_THAT(input, Eq(output));
}

}  // namespace web_package
