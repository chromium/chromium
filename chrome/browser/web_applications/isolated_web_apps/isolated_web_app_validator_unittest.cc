// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::_;
using testing::Eq;
using testing::HasSubstr;

const char kSignedWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

const char kAnotherSignedWebBundleId[] =
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

// NOLINTNEXTLINE(runtime/string)
const std::string kUrl =
    base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                  kSignedWebBundleId});

// NOLINTNEXTLINE(runtime/string)
const std::string kUrlFromAnotherIsolatedWebApp =
    std::string(chrome::kIsolatedAppScheme) + url::kStandardSchemeSeparator +
    kAnotherSignedWebBundleId;

constexpr std::array<uint8_t, 32> kPublicKeyBytes1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kPublicKeyBytes2 = {
    0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

}  // namespace

class IsolatedWebAppValidatorTest : public ::testing::Test {
 protected:
  using IntegrityBlockFuture =
      base::test::TestFuture<base::expected<void, std::string>>;

  web_package::SignedWebBundleIntegrityBlock MakeIntegrityBlock(
      const std::vector<web_package::Ed25519PublicKey>& public_keys = {
          kPublicKey1}) {
    auto raw_integrity_block = web_package::mojom::BundleIntegrityBlock::New();
    raw_integrity_block->size = 123;
    raw_integrity_block
        ->signature_stack = base::ToVector(public_keys, [](const auto&
                                                               public_key) {
      auto entry =
          web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
      entry->signature_info = web_package::mojom::SignatureInfo::NewEd25519(
          web_package::mojom::SignatureInfoEd25519::New());
      entry->signature_info->get_ed25519()->public_key = public_key;
      return entry;
    });

    auto signed_web_bundle_id =
        web_package::SignedWebBundleId::CreateForPublicKey(public_keys[0]);
    raw_integrity_block->attributes =
        web_package::test::GetAttributesForSignedWebBundleId(
            signed_web_bundle_id.id());
    auto integrity_block = web_package::SignedWebBundleIntegrityBlock::Create(
        std::move(raw_integrity_block));
    CHECK(integrity_block.has_value()) << integrity_block.error();

    return *integrity_block;
  }

  static inline const web_package::Ed25519PublicKey kPublicKey1 =
      web_package::Ed25519PublicKey::Create(base::make_span(kPublicKeyBytes1));
  static inline const web_package::Ed25519PublicKey kPublicKey2 =
      web_package::Ed25519PublicKey::Create(base::make_span(kPublicKeyBytes2));

  static inline const web_package::SignedWebBundleId kWebBundleId1 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey1);
  static inline const web_package::SignedWebBundleId kWebBundleId2 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey2);

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  IsolatedWebAppValidator validator_;
  IsolatedWebAppTrustChecker trust_checker_ =
      IsolatedWebAppTrustChecker(profile_);
};

using IsolatedWebAppValidatorIntegrityBlockTest = IsolatedWebAppValidatorTest;

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest,
       WebBundleIdAndPublicKeyDiffer) {
  auto integrity_block = MakeIntegrityBlock({kPublicKey2});

  EXPECT_THAT(
      validator_.ValidateIntegrityBlock(kWebBundleId1, integrity_block,
                                        /*dev_mode=*/false, trust_checker_),
      ErrorIs(HasSubstr("does not match the expected Web Bundle ID")));
}

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsTrusted) {
  auto integrity_block = MakeIntegrityBlock();
  SetTrustedWebBundleIdsForTesting({kWebBundleId1});

  EXPECT_THAT(
      validator_.ValidateIntegrityBlock(kWebBundleId1, integrity_block,
                                        /*dev_mode=*/false, trust_checker_),
      HasValue());
}

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsUntrusted) {
  auto integrity_block = MakeIntegrityBlock();
  SetTrustedWebBundleIdsForTesting({});

  EXPECT_THAT(
      validator_.ValidateIntegrityBlock(kWebBundleId1, integrity_block,
                                        /*dev_mode=*/false, trust_checker_),
      ErrorIs(HasSubstr("public key(s) are not trusted")));
}

class IsolatedWebAppValidatorMetadataTest
    : public IsolatedWebAppValidatorTest,
      public ::testing::WithParamInterface<
          std::tuple<std::optional<std::string>,
                     std::vector<std::string>,
                     base::expected<void, UnusableSwbnFileError>>> {
 public:
  IsolatedWebAppValidatorMetadataTest()
      : primary_url_(std::get<0>(GetParam())),
        status_(std::get<2>(GetParam())) {
    for (const std::string& entry : std::get<1>(GetParam())) {
      entries_.emplace_back(entry);
    }
  }

 protected:
  std::optional<GURL> primary_url_;
  std::vector<GURL> entries_;
  base::expected<void, UnusableSwbnFileError> status_;
};

TEST_P(IsolatedWebAppValidatorMetadataTest, Validate) {
  EXPECT_EQ(validator_.ValidateMetadata(kWebBundleId1, primary_url_, entries_),
            status_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppValidatorMetadataTest,
    ::testing::Values(
        std::make_tuple(std::nullopt,
                        std::vector<std::string>({kUrl}),
                        base::ok()),
        std::make_tuple(
            std::nullopt,
            std::vector<std::string>({kUrl, kUrl + "/foo#bar"}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: URLs must not have "
                "a fragment part."))),
        std::make_tuple(
            std::nullopt,
            std::vector<std::string>({kUrl, kUrl + "/foo?bar"}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: URLs must not have "
                "a query part."))),
        std::make_tuple(
            kUrl,
            std::vector<std::string>({kUrl}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "Primary URL must not be present, but was isolated-app://"
                "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"))),
        std::make_tuple(
            std::nullopt,
            std::vector<std::string>({kUrl, "https://foo/"}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: The URL scheme "
                "must be isolated-app, but was https"))),
        std::make_tuple(
            std::nullopt,
            std::vector<std::string>({kUrl, kUrlFromAnotherIsolatedWebApp}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange contains the wrong Signed Web Bundle "
                "ID: "
                "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic")))));

}  // namespace web_app
