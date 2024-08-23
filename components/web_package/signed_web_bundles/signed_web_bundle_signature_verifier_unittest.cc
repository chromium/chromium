// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/cbor/values.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package::test {

namespace {

// The following values are representations of the keys used to generate
// components/test/data/web_package/simple_b2_signed_v2.swbn
// It has been generated using
// components/web_package/test_support/signed_web_bundles/web_bundle_signer.h.
constexpr uint8_t kEd25519PublicKey[] = {
    0xe4, 0xd5, 0x16, 0xc9, 0x85, 0x9a, 0xf8, 0x63, 0x56, 0xa3, 0x51,
    0x66, 0x7d, 0xbd, 0x00, 0x43, 0x61, 0x10, 0x1a, 0x92, 0xd4, 0x02,
    0x72, 0xfe, 0x2b, 0xce, 0x81, 0xbb, 0x3b, 0x71, 0x3f, 0x2d};

constexpr uint8_t kEd25519Signature[] = {
    0xEA, 0xFD, 0x2A, 0xF5, 0xF4, 0x3D, 0x8D, 0x25, 0x40, 0xDF, 0xE5,
    0x15, 0x60, 0x05, 0x21, 0x08, 0x07, 0x28, 0x7C, 0x60, 0x20, 0xEF,
    0x92, 0xD0, 0x84, 0x17, 0x69, 0x56, 0x98, 0x68, 0xD8, 0x51, 0xDC,
    0x58, 0x97, 0x87, 0x6A, 0x69, 0xE4, 0x0D, 0x30, 0x78, 0xE1, 0xE2,
    0xE5, 0x16, 0x17, 0x35, 0xED, 0xB6, 0x77, 0xE2, 0x78, 0x7E, 0xB1,
    0x90, 0x57, 0xDC, 0xC4, 0x29, 0x21, 0x83, 0x1A, 0x04};

constexpr uint8_t kEd25519PubkeyCbor[] = {
    // 0xa0 + 1 -> map of length 1
    0xa1,

    // 0x60 + 16 -> UTF-8 string of length 16
    0x70,
    // attribute key
    'e', 'd', '2', '5', '5', '1', '9', 'P', 'u', 'b', 'l', 'i', 'c', 'K', 'e',
    'y',

    // 0x58 -> byte array, 0x20 = 32 -> length
    0x58, 0x20,
    // attribute value (public key)
    0xe4, 0xd5, 0x16, 0xc9, 0x85, 0x9a, 0xf8, 0x63, 0x56, 0xa3, 0x51, 0x66,
    0x7d, 0xbd, 0x00, 0x43, 0x61, 0x10, 0x1a, 0x92, 0xd4, 0x02, 0x72, 0xfe,
    0x2b, 0xce, 0x81, 0xbb, 0x3b, 0x71, 0x3f, 0x2d};

constexpr uint8_t kEcdsaP256PublicKey[] = {
    0x03, 0x0A, 0x22, 0xFC, 0x5C, 0x0B, 0x1E, 0x14, 0x85, 0x90, 0xE1,
    0xF9, 0x87, 0xCC, 0x4E, 0x0D, 0x49, 0x2E, 0xF8, 0xE5, 0x1E, 0x23,
    0xF9, 0xB3, 0x63, 0x75, 0xE1, 0x52, 0xB2, 0x4A, 0xEC, 0xA5, 0xE6};

constexpr uint8_t kEcdsaP256Signature[] = {
    0x30, 0x44, 0x02, 0x20, 0x38, 0xB2, 0x95, 0x58, 0x51, 0x10, 0x61, 0xCF,
    0xAB, 0xDC, 0x8E, 0x5B, 0xB6, 0x5A, 0xC2, 0x97, 0x6D, 0x1A, 0xA2, 0xBA,
    0x9A, 0x5D, 0xEA, 0xB8, 0x07, 0x40, 0x97, 0xB2, 0x17, 0x2B, 0xB9, 0xAD,
    0x02, 0x20, 0x5A, 0x25, 0x31, 0x7F, 0xDE, 0x37, 0xB5, 0x2D, 0xF6, 0xB7,
    0x90, 0x08, 0x56, 0x4B, 0x73, 0x2D, 0x70, 0x61, 0xE4, 0x55, 0xD9, 0x6D,
    0x6A, 0xA0, 0x41, 0xF9, 0x3F, 0xE7, 0x4E, 0x11, 0x36, 0xBA};

constexpr uint8_t kEcdsaP256PubkeyCbor[] = {
    // 0xa0 + 1 -> map of length 1
    0xa1,

    // 0x78 -> UTF-8 string, 0x18 = 24 -> length
    0x78, 0x18,
    // attribute key
    'e', 'c', 'd', 's', 'a', 'P', '2', '5', '6', 'S', 'H', 'A', '2', '5', '6',
    'P', 'u', 'b', 'l', 'i', 'c', 'K', 'e', 'y',

    // 0x58 -> byte array, 0x21 = 33 -> length
    0x58, 0x21,
    // attribute value (public key)
    0x03, 0x0A, 0x22, 0xFC, 0x5C, 0x0B, 0x1E, 0x14, 0x85, 0x90, 0xE1, 0xF9,
    0x87, 0xCC, 0x4E, 0x0D, 0x49, 0x2E, 0xF8, 0xE5, 0x1E, 0x23, 0xF9, 0xB3,
    0x63, 0x75, 0xE1, 0x52, 0xB2, 0x4A, 0xEC, 0xA5, 0xE6};

constexpr char kEcdsaP256BundleId[] =
    "amfcf7c4bmpbjbmq4h4yptcobves56hfdyr7tm3doxqvfmsk5ss6maacai";

constexpr uint8_t kEcdsaP256BundleIdCbor[] = {
    // 0xa0 + 1 -> map of length 1
    0xa1,

    // 0x60 + 11 -> UTF-8 string of length 11
    0x6b,
    // attribute key
    'w', 'e', 'b', 'B', 'u', 'n', 'd', 'l', 'e', 'I', 'd',

    // 0x78 -> UTF-8 string, 0x3a = 58 -> length
    0x78, 0x3a,
    // value - bundle ID
    'a', 'm', 'f', 'c', 'f', '7', 'c', '4', 'b', 'm', 'p', 'b', 'j', 'b', 'm',
    'q', '4', 'h', '4', 'y', 'p', 't', 'c', 'o', 'b', 'v', 'e', 's', '5', '6',
    'h', 'f', 'd', 'y', 'r', '7', 't', 'm', '3', 'd', 'o', 'x', 'q', 'v', 'f',
    'm', 's', 'k', '5', 's', 's', '6', 'm', 'a', 'a', 'c', 'a', 'i'

};

SignedWebBundleId CreateForKeyPair(const KeyPair& key_pair) {
  return absl::visit(
      [](const auto& key_pair) {
        return SignedWebBundleId::CreateForPublicKey(key_pair.public_key);
      },
      key_pair);
}

}  // namespace

// Tests that signatures created with the Go tool from
// github.com/WICG/webpackage are verified correctly.
class SignedWebBundleSignatureVerifierGoToolTest
    : public ::testing::TestWithParam<std::tuple<
          std::pair<base::FilePath,
                    std::optional<SignedWebBundleSignatureVerifier::Error>>,
          uint64_t>> {
 public:
  void SetUp() override { IdentityValidator::CreateInstanceForTesting(); }

 protected:
  base::FilePath GetTestFilePath(const base::FilePath& path) {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.Append(
        base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));
    return test_data_dir.Append(path);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// TODO(crbug.com/40239682): Add additional tests for Signed Web Bundles that
// have more than one signature once the Go tool supports it.
TEST_P(SignedWebBundleSignatureVerifierGoToolTest, VerifySimpleWebBundle) {
  auto file_path = GetTestFilePath(std::get<0>(GetParam()).first);

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(test::MakeSignatureStackEntry(
      Ed25519PublicKey::Create(base::span(kEd25519PublicKey)),
      kEd25519Signature, kEd25519PubkeyCbor));
  raw_signature_stack.push_back(test::MakeSignatureStackEntry(
      EcdsaP256PublicKey::Create(base::span(kEcdsaP256PublicKey)).value(),
      kEcdsaP256Signature, kEcdsaP256PubkeyCbor));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 343;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);
  raw_integrity_block->attributes = {kEcdsaP256BundleId,
                                     base::ToVector(kEcdsaP256BundleIdCbor)};
  ASSERT_OK_AND_ASSIGN(
      auto integrity_block,
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block)));

  auto file =
      base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  SignedWebBundleSignatureVerifier signature_verifier;
  signature_verifier.SetWebBundleChunkSizeForTesting(std::get<1>(GetParam()));

  auto result =
      test::VerifySignatures(signature_verifier, file, integrity_block);
  auto expected_error = std::get<0>(GetParam()).second;

  if (expected_error.has_value()) {
    ASSERT_TRUE(!result.has_value());
    EXPECT_EQ(result.error().type, expected_error->type);
    EXPECT_EQ(result.error().message, expected_error->message);
  } else {
    ASSERT_TRUE(result.has_value()) << result.error().message;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleSignatureVerifierGoToolTest,
    ::testing::Combine(
        ::testing::Values(
            std::make_pair(
                base::FilePath(FILE_PATH_LITERAL("simple_b2_signed_v2.swbn")),
                std::nullopt),
            std::make_pair(
                base::FilePath(
                    FILE_PATH_LITERAL("simple_b2_signed_v2_tampered.swbn")),
                SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
                    "The signature is invalid."))),
        // Test with multiple web bundle chunk sizes.
        ::testing::Values(
            // Test with a very low value so that multiple chunks have to be
            // read, even for our very small test bundles.
            10ull,
            // Test with the default value of 10MB.
            10ull * 1000 * 1000,
            // Test with a value that should cause OOM errors in tests if a
            // buffer of that size is allocated, even when the file itself is
            // much smaller.
            1000ull * 1000 * 1000 * 1000)),
    [](const ::testing::TestParamInfo<
        SignedWebBundleSignatureVerifierGoToolTest::ParamType>& info) {
      const auto& file_path = std::get<0>(info.param).first.MaybeAsASCII();
      const auto& expected_error = std::get<0>(info.param).second;
      const auto chunk_size = std::get<1>(info.param);

      std::string file_name;
      base::ReplaceChars(file_path, ".", "_", &file_name);
      return file_name + "_" +
             (expected_error.has_value() ? "tampered" : "valid") + "_" +
             base::NumberToString(chunk_size);
    });

class SignedWebBundleSignatureVerifierTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
    IdentityValidator::CreateInstanceForTesting();
  }

  std::vector<uint8_t> CreateSignedWebBundle(const KeyPairs& key_pairs) {
    WebBundleBuilder builder;
    auto web_bundle = builder.CreateBundle();

    return WebBundleSigner::SignBundle(
        web_bundle, key_pairs,
        /*ib_attributes=*/
        WebBundleSigner::IntegrityBlockAttributes(
            {.web_bundle_id = CreateForKeyPair(key_pairs[0]).id()}));
  }

  base::FilePath WriteSignedWebBundleToDisk(
      const std::vector<uint8_t> signed_web_bundle) {
    base::FilePath signed_web_bundle_path;
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir.GetPath(), &signed_web_bundle_path));
    EXPECT_TRUE(base::WriteFile(signed_web_bundle_path, signed_web_bundle));
    return signed_web_bundle_path;
  }

  base::File MakeWebBundleFile(const base::FilePath& signed_web_bundle_path) {
    auto file = base::File(signed_web_bundle_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid())
        << base::File::ErrorToString(file.error_details());
    return file;
  }

 private:
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
};

class SignedWebBundleSignatureVerifierTest
    : public SignedWebBundleSignatureVerifierTestBase,
      public ::testing::WithParamInterface<
          std::pair<KeyPairs,
                    std::optional<SignedWebBundleSignatureVerifier::Error>>> {};

TEST_P(SignedWebBundleSignatureVerifierTest, VerifySignatures) {
  const auto& key_pairs = std::get<0>(GetParam());
  auto signed_web_bundle = CreateSignedWebBundle(key_pairs);
  base::FilePath signed_web_bundle_path =
      WriteSignedWebBundleToDisk(signed_web_bundle);
  auto file = MakeWebBundleFile(signed_web_bundle_path);

  auto parsed_integrity_block = test::ParseIntegrityBlock(signed_web_bundle);

  const auto& signatures = parsed_integrity_block.signature_stack().entries();
  ASSERT_EQ(signatures.size(), key_pairs.size());

  std::vector<PublicKey> inferred_public_keys =
      base::ToVector(signatures, [](const auto& signature) {
        return absl::visit(
            base::Overloaded{[](const auto& signature_info) -> PublicKey {
                               return signature_info.public_key();
                             },
                             [](const SignedWebBundleSignatureInfoUnknown&)
                                 -> PublicKey { NOTREACHED(); }},
            signature.signature_info());
      });
  std::vector<PublicKey> expected_public_keys =
      base::ToVector(key_pairs, [](const auto& key_pair) {
        return absl::visit(
            [](const auto& key_pair) -> PublicKey {
              return key_pair.public_key;
            },
            key_pair);
      });
  EXPECT_EQ(inferred_public_keys, expected_public_keys);

  SignedWebBundleSignatureVerifier signature_verifier;
  auto result =
      test::VerifySignatures(signature_verifier, file, parsed_integrity_block);
  auto expected_error = std::get<1>(GetParam());
  if (expected_error.has_value()) {
    ASSERT_TRUE(!result.has_value());
    EXPECT_EQ(result.error().type, expected_error->type);
    EXPECT_EQ(result.error().message, expected_error->message);
  } else {
    ASSERT_TRUE(result.has_value()) << result.error().message;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleSignatureVerifierTest,
    ::testing::Values(
        // one signature
        std::make_pair(KeyPairs{Ed25519KeyPair::CreateRandom()}, std::nullopt),
        std::make_pair(KeyPairs{EcdsaP256KeyPair::CreateRandom()},
                       std::nullopt),
        std::make_pair(
            KeyPairs{Ed25519KeyPair::CreateRandom(
                /*produce_invalid_signature=*/true)},
            SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
                "The signature is invalid.")),
        std::make_pair(
            KeyPairs{EcdsaP256KeyPair::CreateRandom(
                /*produce_invalid_signature=*/true)},
            SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
                "The signature is invalid.")),

        // two signatures
        std::make_pair(KeyPairs{Ed25519KeyPair::CreateRandom(),
                                Ed25519KeyPair::CreateRandom()},
                       std::nullopt),
        std::make_pair(KeyPairs{EcdsaP256KeyPair::CreateRandom(),
                                Ed25519KeyPair::CreateRandom()},
                       std::nullopt)),
    [](const ::testing::TestParamInfo<
        SignedWebBundleSignatureVerifierTest::ParamType>& info) {
      return base::StringPrintf(
          "%zu_%zu_signatures_%s", info.index, info.param.first.size(),
          info.param.second.has_value() ? "error" : "no_error");
    });

}  // namespace web_package::test
