// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

#include <array>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

// Some random bytes that are treated as an Ed25519 public key.
const std::array<uint8_t, 32> kEd25519PublicKeyBytes(
    {0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
     0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
     0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73});

constexpr std::string_view kEd25519SignedWebBundleId =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

// Some random bytes to use as a proxy mode key.
const std::array<uint8_t, 32> kProxyModeBytes(
    {0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7a, 0x14, 0x42, 0x14, 0xa2,
     0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
     0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73});

constexpr std::string_view kProxyModeSignedWebBundleId =
    "airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

// Valid ECDSA P-256 public key.
constexpr std::array<uint8_t, 33> kEcdsaP256PublicKeyBytes = {
    0x03, 0x42, 0x06, 0xc0, 0x35, 0xe5, 0x87, 0x9f, 0xdd, 0x31, 0x51,
    0x95, 0x44, 0xfd, 0x8d, 0x6c, 0x1b, 0xe9, 0x99, 0x11, 0xe8, 0x40,
    0x5b, 0xae, 0x6a, 0x36, 0x1b, 0xf5, 0x17, 0x12, 0xa1, 0x17, 0xe3};

constexpr std::string_view kEcdsaP256SignedWebBundleId =
    "anbanqbv4wdz7xjrkgkuj7mnnqn6tgir5bafxltkgyn7kfysuel6gaacai";

}  // namespace

class SignedWebBundleIdValidTest
    : public ::testing::TestWithParam<
          std::tuple<std::string, SignedWebBundleId::Type>> {
 public:
  SignedWebBundleIdValidTest()
      : raw_id_(std::get<0>(GetParam())), type_(std::get<1>(GetParam())) {}

 protected:
  std::string raw_id_;
  SignedWebBundleId::Type type_;
};

TEST_P(SignedWebBundleIdValidTest, ParseValidIDs) {
  EXPECT_THAT(SignedWebBundleId::Create(raw_id_),
              base::test::ValueIs(
                  ::testing::Property(&SignedWebBundleId::type, type_)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleIdValidTest,
    ::testing::Values(
        // Development-only key suffix
        std::make_tuple(kProxyModeSignedWebBundleId,
                        SignedWebBundleId::Type::kProxyMode),
        // Ed25519 key suffix
        std::make_tuple(kEd25519SignedWebBundleId,
                        SignedWebBundleId::Type::kEd25519PublicKey),
        // Ecdsa P-256 key suffix
        std::make_tuple(kEcdsaP256SignedWebBundleId,
                        SignedWebBundleId::Type::kEcdsaP256PublicKey)),
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
      Ed25519PublicKey::Create(base::span(kEd25519PublicKeyBytes));

  auto id = SignedWebBundleId::CreateForPublicKey(public_key);
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kEd25519PublicKey);
  EXPECT_EQ(id.id(), kEd25519SignedWebBundleId);
}

TEST(SignedWebBundleIdTest, CreateForEcdsaP256PublicKey) {
  ASSERT_OK_AND_ASSIGN(auto public_key,
                       EcdsaP256PublicKey::Create(kEcdsaP256PublicKeyBytes));

  auto id = SignedWebBundleId::CreateForPublicKey(public_key);
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kEcdsaP256PublicKey);
  EXPECT_EQ(id.id(), kEcdsaP256SignedWebBundleId);
}

TEST(SignedWebBundleIdTest, CreateForProxyMode) {
  auto id = SignedWebBundleId::CreateForProxyMode(kProxyModeBytes);
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kProxyMode);
  EXPECT_EQ(id.id(), kProxyModeSignedWebBundleId);
}

TEST(SignedWebBundleIdTest, CreateRandomForProxyMode) {
  auto id = SignedWebBundleId::CreateRandomForProxyMode();
  EXPECT_EQ(id.type(), SignedWebBundleId::Type::kProxyMode);
}

}  // namespace web_package
