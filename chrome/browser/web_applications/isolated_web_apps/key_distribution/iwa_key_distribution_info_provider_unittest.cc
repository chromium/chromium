// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

#include "base/base64.h"
#include "base/containers/extend.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_identity_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::_;
using testing::Eq;
using testing::Field;
using testing::FieldsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Property;
using testing::VariantWith;

using ComponentUpdateError =
    IwaKeyDistributionInfoProvider::ComponentUpdateError;

constexpr std::array<uint8_t, 4> kExpectedKey = {0x00, 0x00, 0x00, 0x00};
constexpr char kWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

IwaKeyDistribution CreateValidData() {
  IwaKeyDistribution key_distribution;

  IwaKeyRotations key_rotations;
  IwaKeyRotations::KeyRotationInfo kr_info;

  kr_info.set_expected_key(base::Base64Encode(kExpectedKey));
  key_rotations.mutable_key_rotations()->emplace(kWebBundleId,
                                                 std::move(kr_info));
  *key_distribution.mutable_key_rotation_data() = std::move(key_rotations);

  return key_distribution;
}

}  // namespace

class IwaIwaKeyDistributionInfoProviderTest : public testing::Test {
 public:
  void TearDown() override {
    IwaKeyDistributionInfoProvider::DestroyInstanceForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponent) {
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"),
                                              CreateValidData()),
              HasValue());
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest,
       LoadComponentAndThenStaleComponent) {
  auto data = CreateValidData();
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"), data),
              HasValue());
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("0.9.0"), data),
              ErrorIs(Eq(ComponentUpdateError::kStaleVersion)));
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponentWrongPath) {
  EXPECT_THAT(
      test::UpdateKeyDistributionInfo(base::Version("1.0.0"), base::FilePath()),
      ErrorIs(Eq(ComponentUpdateError::kFileNotFound)));
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponentFaultyData) {
  base::ScopedTempDir component_install_dir;
  CHECK(component_install_dir.CreateUniqueTempDir());
  auto path = component_install_dir.GetPath().AppendASCII("krc");
  CHECK(base::WriteFile(path, "not_a_proto"));

  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"), path),
              ErrorIs(Eq(ComponentUpdateError::kProtoParsingFailure)));
}

class SignedWebBundleSignatureVerifierWithKeyDistributionTest
    : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    IwaIdentityValidator::CreateSingleton();
  }

  base::FilePath WriteSignedWebBundleToDisk(
      base::span<const uint8_t> signed_web_bundle) {
    base::FilePath signed_web_bundle_path;
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &signed_web_bundle_path));
    EXPECT_TRUE(base::WriteFile(signed_web_bundle_path, signed_web_bundle));
    return signed_web_bundle_path;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SignedWebBundleSignatureVerifierWithKeyDistributionTest,
       VerifySignaturesWithKeyDistribution) {
  using Error = web_package::SignedWebBundleSignatureVerifier::Error;

  auto key_pairs = web_package::test::KeyPairs{
      web_package::test::EcdsaP256KeyPair::CreateRandom(),
      web_package::test::Ed25519KeyPair::CreateRandom()};

  web_package::test::WebBundleSigner::IntegrityBlockAttributes ib_attributes(
      {.web_bundle_id = kWebBundleId});

  auto signed_web_bundle = web_package::test::WebBundleSigner::SignBundle(
      web_package::WebBundleBuilder().CreateBundle(), key_pairs, ib_attributes);
  base::FilePath signed_web_bundle_path =
      WriteSignedWebBundleToDisk(signed_web_bundle);
  auto file = base::File(signed_web_bundle_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(file.IsValid());

  auto parsed_integrity_block =
      web_package::test::ParseIntegrityBlock(signed_web_bundle);
  EXPECT_EQ(parsed_integrity_block.web_bundle_id().id(),
            ib_attributes.web_bundle_id);

  web_package::SignedWebBundleSignatureVerifier signature_verifier;
  EXPECT_THAT(web_package::test::VerifySignatures(signature_verifier, file,
                                                  parsed_integrity_block),
              ErrorIs(FieldsAre(
                  Error::Type::kWebBundleIdError,
                  base::StringPrintf("Web Bundle ID <%s> doesn't match any "
                                     "public key in the signature list.",
                                     kWebBundleId))));

  auto expected_key = absl::visit(
      [](const auto& key_pair) -> base::span<const uint8_t> {
        return key_pair.public_key.bytes();
      },
      key_pairs[0]);
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"),
                                              kWebBundleId, expected_key),
              HasValue());

  EXPECT_THAT(web_package::test::VerifySignatures(signature_verifier, file,
                                                  parsed_integrity_block),
              HasValue());

  auto random_key = web_package::test::Ed25519KeyPair::CreateRandom();
  EXPECT_THAT(
      test::UpdateKeyDistributionInfo(base::Version("1.0.1"), kWebBundleId,
                                      random_key.public_key.bytes()),
      HasValue());

  EXPECT_THAT(
      web_package::test::VerifySignatures(signature_verifier, file,
                                          parsed_integrity_block),
      ErrorIs(FieldsAre(Error::Type::kWebBundleIdError,
                        HasSubstr(base::StringPrintf(
                            "Rotated key for Web Bundle ID <%s> doesn't match",
                            kWebBundleId)))));

  EXPECT_THAT(
      test::UpdateKeyDistributionInfo(base::Version("1.0.2"), kWebBundleId,
                                      /*expected_key=*/std::nullopt),
      HasValue());

  EXPECT_THAT(
      web_package::test::VerifySignatures(signature_verifier, file,
                                          parsed_integrity_block),
      ErrorIs(FieldsAre(Error::Type::kWebBundleIdError,
                        HasSubstr(base::StringPrintf(
                            "Web Bundle ID <%s> is disabled", kWebBundleId)))));
}

}  // namespace web_app
