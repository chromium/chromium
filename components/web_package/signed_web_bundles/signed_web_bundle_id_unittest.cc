// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

#include <array>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {

namespace {

// Some random bytes that are treated as an Ed25519 public key.
const std::array<uint8_t, 32> kEd25519PublicKeyBytes(
    {0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
     0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
     0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73});

constexpr char kEd25519SignedWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

// Some random bytes to use as a development-only key.
const std::array<uint8_t, 32> kDevelopmentBytes(
    {0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7a, 0x14, 0x42, 0x14, 0xa2,
     0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
     0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73});

constexpr char kDevelopmentSignedWebBundleId[] =
    "airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

}  // namespace

class SignedWebBundleIdValidTest
    : public ::testing::TestWithParam<
          std::tuple<std::string,
                     SignedWebBundleId::Type,
                     absl::optional<Ed25519PublicKey>>> {
 public:
  SignedWebBundleIdValidTest()
      : raw_id_(std::get<0>(GetParam())),
        type_(std::get<1>(GetParam())),
        public_key_(std::get<2>(GetParam())) {}

 protected:
  std::string raw_id_;
  SignedWebBundleId::Type type_;
  absl::optional<Ed25519PublicKey> public_key_;
};

TEST_P(SignedWebBundleIdValidTest, ParseValidIDs) {
  const auto parsed_id = SignedWebBundleId::Create(raw_id_);
  EXPECT_TRUE(parsed_id.has_value());
  EXPECT_EQ(parsed_id->type(), type_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleIdValidTest,
    ::testing::Values(
        // Development-only key suffix
        std::make_tuple(kDevelopmentSignedWebBundleId,
                        SignedWebBundleId::Type::kDevelopment,
                        absl::nullopt),
        // Ed25519 key suffix
        std::make_tuple(
            kEd25519SignedWebBundleId,
            SignedWebBundleId::Type::kEd25519PublicKey,
            Ed25519PublicKey::Create(base::make_span(kEd25519PublicKeyBytes)))),
    [](const testing::TestParamInfo<SignedWebBundleIdValidTest::ParamType>&
           info) { return std::get<0>(info.param); });

class SignedWebBundleIdInvalidTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(SignedWebBundleIdInvalidTest, ParseInvalidIDs) {
  EXPECT_FALSE(SignedWebBundleId::Create(GetParam().second).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleIdInvalidTest,
    ::testing::Values(
        std::make_pair("emptyKey", ""),
        std::make_pair(
            "oneCharacterShort",
            "erugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"),
        std::make_pair(
            "invalidSuffix",
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaayc"),
        std::make_pair(
            "usesPadding",
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdj74aagaa="),
        std::make_pair(
            "validKeyButInUppercase",
            "AERUGQZTIJ5BIQQUUK3MFWPSAIBUEGAQCITGFCHWUOSUOFDJABZQAAIC"),
        std::make_pair(
            "invalidCharacter9",
            "9erugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac")),
    [](const testing::TestParamInfo<SignedWebBundleIdInvalidTest::ParamType>&
           info) { return info.param.first; });

TEST(SignedWebBundleIdTest, Comparators) {
  const auto a1 = *SignedWebBundleId::Create(
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac");
  const auto a2 = *SignedWebBundleId::Create(
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac");
  const auto b = *SignedWebBundleId::Create(
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac");

  EXPECT_TRUE(a1 == a1);
  EXPECT_TRUE(a1 == a2);
  EXPECT_FALSE(a1 == b);

  EXPECT_FALSE(a1 != a1);
  EXPECT_FALSE(a1 != a2);
  EXPECT_TRUE(a1 != b);

  EXPECT_TRUE(a1 < b);
  EXPECT_FALSE(b < a2);
}

TEST(SignedWebBundleIdTest, CreateForEd25519PublicKey) {
  auto public_key =
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKeyBytes));

  auto id = SignedWebBundleId::CreateForEd25519PublicKey(public_key);
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kEd25519PublicKey);
  EXPECT_EQ(id.id(), kEd25519SignedWebBundleId);
}

TEST(SignedWebBundleIdTest, CreateForDevelopment) {
  auto id = SignedWebBundleId::CreateForDevelopment(
      base::make_span(kDevelopmentBytes));
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kDevelopment);
  EXPECT_EQ(id.id(), kDevelopmentSignedWebBundleId);
}

TEST(SignedWebBundleIdTest, CreateRandomForDevelopmentDefaultGenerator) {
  auto id = SignedWebBundleId::CreateRandomForDevelopment();
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kDevelopment);
}

TEST(SignedWebBundleIdTest, CreateRandomForDevelopmentCustomGenerator) {
  auto custom_callback =
      base::BindLambdaForTesting([](void* ptr, size_t len) -> void {
        DCHECK_EQ(len, kDevelopmentBytes.size());
        base::ranges::copy(kDevelopmentBytes, static_cast<uint8_t*>(ptr));
      });

  SignedWebBundleId id =
      SignedWebBundleId::CreateRandomForDevelopment(custom_callback);

  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kDevelopment);
  EXPECT_EQ(id.id(), kDevelopmentSignedWebBundleId);
}

}  // namespace web_package
