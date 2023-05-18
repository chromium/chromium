// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using testing::_;

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

constexpr std::array<uint8_t, 32> kEd25519PublicKey = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73, 0x01,
    0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2, 0xb6,
    0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26, 0x62,
    0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

class MockIsolatedWebAppTrustChecker : public IsolatedWebAppTrustChecker {
 public:
  MockIsolatedWebAppTrustChecker()
      : IsolatedWebAppTrustChecker(TestingPrefServiceSimple()) {}

  MOCK_METHOD(
      IsolatedWebAppTrustChecker::Result,
      IsTrusted,
      (const web_package::SignedWebBundleId& web_bundle_id,
       const web_package::SignedWebBundleIntegrityBlock& integrity_block),
      (const, override));
};

web_package::SignedWebBundleIntegrityBlock MakeIntegrityBlock() {
  auto raw_integrity_block = web_package::mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 123;

  auto raw_signature_stack_entry =
      web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key =
      web_package::Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey));
  raw_signature_stack_entry->signature =
      web_package::Ed25519Signature::Create(base::make_span(kEd25519Signature));

  raw_integrity_block->signature_stack.push_back(
      std::move(raw_signature_stack_entry));

  auto integrity_block = web_package::SignedWebBundleIntegrityBlock::Create(
      std::move(raw_integrity_block));
  CHECK(integrity_block.has_value()) << integrity_block.error();

  return *integrity_block;
}

}  // namespace

class IsolatedWebAppValidatorTest : public ::testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

class IsolatedWebAppValidatorIntegrityBlockTest
    : public IsolatedWebAppValidatorTest {};

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsTrusted) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::Create(kSignedWebBundleId);
  ASSERT_TRUE(web_bundle_id.has_value()) << web_bundle_id.error();
  auto integrity_block = MakeIntegrityBlock();

  auto isolated_web_app_trust_checker =
      std::make_unique<MockIsolatedWebAppTrustChecker>();
  EXPECT_CALL(*isolated_web_app_trust_checker,
              IsTrusted(*web_bundle_id, integrity_block))
      .WillOnce([](auto web_bundle_id,
                   auto integrity_block) -> IsolatedWebAppTrustChecker::Result {
        return {.status = IsolatedWebAppTrustChecker::Result::Status::kTrusted};
      });

  IsolatedWebAppValidator validator(std::move(isolated_web_app_trust_checker));
  base::test::TestFuture<absl::optional<std::string>> future;
  validator.ValidateIntegrityBlock(*web_bundle_id, integrity_block,
                                   future.GetCallback());
  EXPECT_EQ(future.Get(), absl::nullopt);
}

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsUntrusted) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::Create(kSignedWebBundleId);
  ASSERT_TRUE(web_bundle_id.has_value()) << web_bundle_id.error();
  auto integrity_block = MakeIntegrityBlock();

  auto isolated_web_app_trust_checker =
      std::make_unique<MockIsolatedWebAppTrustChecker>();
  EXPECT_CALL(*isolated_web_app_trust_checker,
              IsTrusted(*web_bundle_id, integrity_block))
      .WillOnce(
          [](auto web_bundle_id,
             auto public_key_stack) -> IsolatedWebAppTrustChecker::Result {
            return {
                .status = IsolatedWebAppTrustChecker::Result::Status::
                    kErrorPublicKeysNotTrusted,
                .message = "test error",
            };
          });

  IsolatedWebAppValidator validator(std::move(isolated_web_app_trust_checker));
  base::test::TestFuture<absl::optional<std::string>> future;
  validator.ValidateIntegrityBlock(*web_bundle_id, integrity_block,
                                   future.GetCallback());
  EXPECT_EQ(future.Get(), "test error");
}

class IsolatedWebAppValidatorMetadataTest
    : public IsolatedWebAppValidatorTest,
      public ::testing::WithParamInterface<
          std::tuple<absl::optional<std::string>,
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
  absl::optional<GURL> primary_url_;
  std::vector<GURL> entries_;
  base::expected<void, UnusableSwbnFileError> status_;
};

TEST_P(IsolatedWebAppValidatorMetadataTest, Validate) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::Create(kSignedWebBundleId);
  ASSERT_TRUE(web_bundle_id.has_value()) << web_bundle_id.error();

  auto isolated_web_app_trust_checker =
      std::make_unique<MockIsolatedWebAppTrustChecker>();
  IsolatedWebAppValidator validator(std::move(isolated_web_app_trust_checker));
  EXPECT_EQ(validator.ValidateMetadata(*web_bundle_id, primary_url_, entries_),
            status_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppValidatorMetadataTest,
    ::testing::Values(
        std::make_tuple(absl::nullopt,
                        std::vector<std::string>({kUrl}),
                        base::ok()),
        std::make_tuple(
            absl::nullopt,
            std::vector<std::string>({kUrl, kUrl + "/foo#bar"}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: URLs must not have "
                "a fragment part."))),
        std::make_tuple(
            absl::nullopt,
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
            absl::nullopt,
            std::vector<std::string>({kUrl, "https://foo/"}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: The URL scheme "
                "must be isolated-app, but was https"))),
        std::make_tuple(
            absl::nullopt,
            std::vector<std::string>({kUrl, kUrlFromAnotherIsolatedWebApp}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange contains the wrong Signed Web Bundle "
                "ID: "
                "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic")))));

}  // namespace web_app
