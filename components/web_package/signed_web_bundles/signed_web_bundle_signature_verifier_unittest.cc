// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/cbor/values.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/shared_file.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {

namespace {

// The following values have been extracted by hand from the Signed Web Bundle
// generated with the Go tool from github.com/WICG/webpackage located at
// components/test/data/web_package/simple_b2_signed.swbn.
constexpr uint8_t kEd25519PublicKey[] = {
    0xe4, 0xd5, 0x16, 0xc9, 0x85, 0x9a, 0xf8, 0x63, 0x56, 0xa3, 0x51,
    0x66, 0x7d, 0xbd, 0x00, 0x43, 0x61, 0x10, 0x1a, 0x92, 0xd4, 0x02,
    0x72, 0xfe, 0x2b, 0xce, 0x81, 0xbb, 0x3b, 0x71, 0x3f, 0x2d};

constexpr uint8_t kEd25519Signature[] = {
    0x64, 0xc1, 0xb6, 0xee, 0x74, 0xbf, 0x8d, 0x01, 0x92, 0xc8, 0xcd,
    0xe7, 0x47, 0x13, 0xda, 0x2c, 0xed, 0x4f, 0x7f, 0x9e, 0xe3, 0x8f,
    0x70, 0x27, 0xbf, 0x79, 0x4a, 0x64, 0x0e, 0xf9, 0xbd, 0xcc, 0xeb,
    0x66, 0x39, 0x50, 0xf8, 0x92, 0x67, 0x1a, 0x71, 0xe9, 0xce, 0x15,
    0xf5, 0xa4, 0xf6, 0x22, 0xc5, 0xcf, 0x04, 0x15, 0xdb, 0x63, 0x59,
    0xb2, 0xff, 0xee, 0x13, 0x93, 0x2c, 0x99, 0x68, 0x0d};

constexpr uint8_t kAttributesCbor[] = {
    0xa1,

    0x70,
    // attribute key
    'e', 'd', '2', '5', '5', '1', '9', 'P', 'u', 'b', 'l', 'i', 'c', 'K', 'e',
    'y',

    0x58, 0x20,
    // attribute value (public key)
    0xe4, 0xd5, 0x16, 0xc9, 0x85, 0x9a, 0xf8, 0x63, 0x56, 0xa3, 0x51, 0x66,
    0x7d, 0xbd, 0x00, 0x43, 0x61, 0x10, 0x1a, 0x92, 0xd4, 0x02, 0x72, 0xfe,
    0x2b, 0xce, 0x81, 0xbb, 0x3b, 0x71, 0x3f, 0x2d};

constexpr uint8_t kCompleteEntryCbor[] = {
    0x82, 0xa1,

    0x70,
    // attribute key
    'e', 'd', '2', '5', '5', '1', '9', 'P', 'u', 'b', 'l', 'i', 'c', 'K', 'e',
    'y',

    0x58, 0x20,
    // attribute value (public key)
    0xe4, 0xd5, 0x16, 0xc9, 0x85, 0x9a, 0xf8, 0x63, 0x56, 0xa3, 0x51, 0x66,
    0x7d, 0xbd, 0x00, 0x43, 0x61, 0x10, 0x1a, 0x92, 0xd4, 0x02, 0x72, 0xfe,
    0x2b, 0xce, 0x81, 0xbb, 0x3b, 0x71, 0x3f, 0x2d,

    0x58, 0x40,
    // signature
    0x64, 0xc1, 0xb6, 0xee, 0x74, 0xbf, 0x8d, 0x01, 0x92, 0xc8, 0xcd, 0xe7,
    0x47, 0x13, 0xda, 0x2c, 0xed, 0x4f, 0x7f, 0x9e, 0xe3, 0x8f, 0x70, 0x27,
    0xbf, 0x79, 0x4a, 0x64, 0x0e, 0xf9, 0xbd, 0xcc, 0xeb, 0x66, 0x39, 0x50,
    0xf8, 0x92, 0x67, 0x1a, 0x71, 0xe9, 0xce, 0x15, 0xf5, 0xa4, 0xf6, 0x22,
    0xc5, 0xcf, 0x04, 0x15, 0xdb, 0x63, 0x59, 0xb2, 0xff, 0xee, 0x13, 0x93,
    0x2c, 0x99, 0x68, 0x0d};

mojom::BundleIntegrityBlockSignatureStackEntryPtr MakeSignatureStackEntry(
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> complete_entry_cbor,
    base::span<const uint8_t> attributes_cbor) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key =
      *web_package::Ed25519PublicKey::Create(public_key);
  raw_signature_stack_entry->signature =
      *web_package::Ed25519Signature::Create(signature);
  raw_signature_stack_entry->complete_entry_cbor = std::vector(
      std::begin(complete_entry_cbor), std::end(complete_entry_cbor));
  raw_signature_stack_entry->attributes_cbor =
      std::vector(std::begin(attributes_cbor), std::end(attributes_cbor));
  return raw_signature_stack_entry;
}

}  // namespace

// Tests that signatures created with the Go tool from
// github.com/WICG/webpackage are verified correctly.
class SignedWebBundleSignatureVerifierGoToolTest
    : public ::testing::TestWithParam<std::tuple<
          std::pair<base::FilePath,
                    absl::optional<SignedWebBundleSignatureVerifier::Error>>,
          uint64_t>> {
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

// TODO(crbug.com/1366303): Add additional tests for Signed Web Bundles that
// have more than one signature once the Go tool supports it.

TEST_P(SignedWebBundleSignatureVerifierGoToolTest, VerifySimpleWebBundle) {
  auto file_path = GetTestFilePath(std::get<0>(GetParam()).first);

  base::test::TestFuture<
      absl::optional<SignedWebBundleSignatureVerifier::Error>>
      future;

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(
      MakeSignatureStackEntry(kEd25519PublicKey, kEd25519Signature,
                              kCompleteEntryCbor, kAttributesCbor));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 135;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_TRUE(integrity_block.has_value()) << integrity_block.error();

  auto shared_file =
      base::MakeRefCounted<SharedFile>(std::make_unique<base::File>(
          file_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  ASSERT_TRUE((*shared_file)->IsValid());

  SignedWebBundleSignatureVerifier signature_verifier(std::get<1>(GetParam()));
  signature_verifier.VerifySignatures(shared_file, std::move(*integrity_block),
                                      future.GetCallback());

  auto error = future.Take();
  auto expected_error = std::get<0>(GetParam()).second;
  if (expected_error.has_value()) {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->type, expected_error->type);
    EXPECT_EQ(error->message, expected_error->message);
  } else {
    ASSERT_FALSE(error.has_value()) << error->message;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleSignatureVerifierGoToolTest,
    ::testing::Combine(
        ::testing::Values(
            std::make_pair(
                base::FilePath(FILE_PATH_LITERAL("simple_b2_signed.swbn")),
                absl::nullopt),
            std::make_pair(
                base::FilePath(
                    FILE_PATH_LITERAL("simple_b2_signed_tampered.swbn")),
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

class SignedWebBundleSignatureVerifierTest
    : public ::testing::TestWithParam<
          std::pair<std::vector<WebBundleSigner::KeyPair>,
                    absl::optional<SignedWebBundleSignatureVerifier::Error>>> {
 protected:
  void SetUp() override { EXPECT_TRUE(temp_dir.CreateUniqueTempDir()); }

  std::tuple<std::vector<uint8_t>, cbor::Value, size_t> CreateSignedWebBundle(
      const std::vector<WebBundleSigner::KeyPair>& key_pairs) {
    WebBundleBuilder builder;
    auto web_bundle = builder.CreateBundle();
    auto integrity_block =
        WebBundleSigner::CreateIntegrityBlockForBundle(web_bundle, key_pairs);
    auto integrity_block_cbor = *cbor::Writer::Write(integrity_block);
    std::vector<uint8_t> signed_web_bundle;
    signed_web_bundle.insert(signed_web_bundle.end(),
                             integrity_block_cbor.begin(),
                             integrity_block_cbor.end());
    signed_web_bundle.insert(signed_web_bundle.end(), web_bundle.begin(),
                             web_bundle.end());
    return std::make_tuple(signed_web_bundle, std::move(integrity_block),
                           integrity_block_cbor.size());
  }

  base::FilePath WriteSignedWebBundleToDisk(
      const std::vector<uint8_t> signed_web_bundle) {
    base::FilePath signed_web_bundle_path;
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir.GetPath(), &signed_web_bundle_path));
    EXPECT_TRUE(base::WriteFile(signed_web_bundle_path, signed_web_bundle));
    return signed_web_bundle_path;
  }

  scoped_refptr<SharedFile> MakeSharedFile(
      const base::FilePath& signed_web_bundle_path) {
    auto file = std::make_unique<base::File>(
        signed_web_bundle_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file->IsValid())
        << base::File::ErrorToString(file->error_details());
    return base::MakeRefCounted<SharedFile>(std::move(file));
  }

  SignedWebBundleIntegrityBlock CreateParsedIntegrityBlock(
      const cbor::Value& integrity_block,
      size_t integrity_block_size) {
    std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        raw_signature_stack;
    for (const auto& signature_stack_entry :
         integrity_block.GetArray()[2].GetArray()) {
      auto complete_entry_cbor = *cbor::Writer::Write(signature_stack_entry);
      const auto& attributes = signature_stack_entry.GetArray()[0];
      auto attributes_cbor = *cbor::Writer::Write(attributes);
      const auto& public_key = attributes.GetMap()
                                   .at(cbor::Value("ed25519PublicKey"))
                                   .GetBytestring();
      const auto& signature =
          signature_stack_entry.GetArray()[1].GetBytestring();

      raw_signature_stack.push_back(MakeSignatureStackEntry(
          public_key, signature, complete_entry_cbor, attributes_cbor));
    }

    auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
    raw_integrity_block->size = integrity_block_size;
    raw_integrity_block->signature_stack = std::move(raw_signature_stack);

    auto parsed_integrity_block =
        SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
    EXPECT_TRUE(parsed_integrity_block.has_value())
        << parsed_integrity_block.error();
    return std::move(*parsed_integrity_block);
  }

 private:
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
};

TEST_P(SignedWebBundleSignatureVerifierTest, VerifySignatures) {
  auto [signed_web_bundle, integrity_block, integrity_block_size] =
      CreateSignedWebBundle(std::get<0>(GetParam()));
  base::FilePath signed_web_bundle_path =
      WriteSignedWebBundleToDisk(signed_web_bundle);
  auto shared_file = MakeSharedFile(signed_web_bundle_path);
  auto parsed_integrity_block =
      CreateParsedIntegrityBlock(integrity_block, integrity_block_size);

  base::test::TestFuture<
      absl::optional<SignedWebBundleSignatureVerifier::Error>>
      future;
  SignedWebBundleSignatureVerifier signature_verifier;
  signature_verifier.VerifySignatures(
      shared_file, std::move(parsed_integrity_block), future.GetCallback());

  auto error = future.Take();
  auto expected_error = std::get<1>(GetParam());
  if (expected_error.has_value()) {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->type, expected_error->type);
    EXPECT_EQ(error->message, expected_error->message);
  } else {
    ASSERT_FALSE(error.has_value()) << error->message;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleSignatureVerifierTest,
    ::testing::Values(
        // one signature
        std::make_pair(std::vector{WebBundleSigner::KeyPair::CreateRandom()},
                       absl::nullopt),
        std::make_pair(
            std::vector{WebBundleSigner::KeyPair::CreateRandom(
                /*produce_invalid_signature=*/true)},
            SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
                "The signature is invalid.")),

        // two signatures
        std::make_pair(
            std::vector{WebBundleSigner::KeyPair::CreateRandom(),
                        WebBundleSigner::KeyPair::CreateRandom()},
            SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
                "Only a single signature is currently supported, got 2 "
                "signatures."))),
    [](const ::testing::TestParamInfo<
        SignedWebBundleSignatureVerifierTest::ParamType>& info) {
      return base::StringPrintf(
          "%zu_%zu_signatures_%s", info.index, info.param.first.size(),
          info.param.second.has_value() ? "error" : "no_error");
    });

}  // namespace web_package
