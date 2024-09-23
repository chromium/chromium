// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::ElementsAre;
using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;
using testing::Property;
using testing::StartsWith;

using VerifierError = web_package::SignedWebBundleSignatureVerifier::Error;

constexpr uint8_t kEd25519PublicKey[32] = {0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 2, 2, 0, 0, 0};

constexpr uint8_t kEd25519Signature[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

class FakeIsolatedWebAppValidator : public IsolatedWebAppValidator {
 public:
  explicit FakeIsolatedWebAppValidator(
      base::expected<void, std::string> integrity_block_validation_result)
      : integrity_block_validation_result_(integrity_block_validation_result) {}

  base::expected<void, std::string> ValidateIntegrityBlock(
      const web_package::SignedWebBundleId& web_bundle_id,
      const web_package::SignedWebBundleIntegrityBlock& integrity_block,
      bool dev_mode,
      const IsolatedWebAppTrustChecker& trust_checker) override {
    return integrity_block_validation_result_;
  }

 private:
  base::expected<void, std::string> integrity_block_validation_result_;
};

class IsolatedWebAppResponseReaderFactoryTest : public WebAppTest {
 public:
  IsolatedWebAppResponseReaderFactoryTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    parser_factory_ = std::make_unique<web_package::MockWebBundleParserFactory>(
        on_create_parser_future_.GetCallback());

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr>
        requests;
    requests.insert(
        {kUrl, web_package::mojom::BundleResponseLocation::New(
                   response_->payload_offset, response_->payload_length)});

    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->requests = std::move(requests);

    auto signature_info_ed25519 =
        web_package::mojom::SignatureInfoEd25519::New();
    signature_info_ed25519->public_key = web_package::Ed25519PublicKey::Create(
        base::make_span(kEd25519PublicKey));
    signature_info_ed25519->signature = web_package::Ed25519Signature::Create(
        base::make_span(kEd25519Signature));

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->signature_info =
        web_package::mojom::SignatureInfo::NewEd25519(
            std::move(signature_info_ed25519));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    integrity_block_->size = 42;
    integrity_block_->signature_stack = std::move(signature_stack);

    integrity_block_->attributes =
        web_package::test::GetAttributesForSignedWebBundleId(kWebBundleId.id());

    factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
        *profile(), std::make_unique<FakeIsolatedWebAppValidator>(base::ok()),
        base::BindRepeating(
            []() -> std::unique_ptr<
                     web_package::SignedWebBundleSignatureVerifier> {
              return std::make_unique<web_package::test::FakeSignatureVerifier>(
                  std::nullopt);
            }));

    CHECK(temp_dir_.CreateUniqueTempDir());
    CHECK(CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path_));
    CHECK(base::WriteFile(web_bundle_path_, kResponseBody));

    in_process_data_decoder_.SetWebBundleParserFactoryBinder(
        base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));
  }

  void TearDown() override {
    factory_.reset();
    WebAppTest::TearDown();
  }

  void FulfillIntegrityBlock() {
    parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  }

  void FulfillMetadata() {
    parser_factory_->RunMetadataCallback(integrity_block_->size,
                                         metadata_->Clone());
  }

  void FulfillResponse(const network::ResourceRequest& resource_request) {
    parser_factory_->RunResponseCallback(
        web_package::mojom::BundleResponseLocation::New(
            response_->payload_offset, response_->payload_length),
        response_->Clone());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  base::FilePath web_bundle_path_;
  base::test::RepeatingTestFuture<std::optional<GURL>> on_create_parser_future_;

  const web_package::SignedWebBundleId kWebBundleId =
      *web_package::SignedWebBundleId::Create(
          "aaaaaaacaibaaaaaaaaaaaaaaiaaeaaaaaaaaaaaaabaeaqaaaaaaaic");
  const GURL kUrl = GURL("isolated-app://" + kWebBundleId.id());

  constexpr static char kResponseBody[] = "test";

  constexpr static char kInvalidIsolatedWebAppUrl[] = "isolated-app://foo/";

  std::unique_ptr<IsolatedWebAppResponseReaderFactory> factory_;
  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;
  web_package::mojom::BundleMetadataPtr metadata_;
  web_package::mojom::BundleResponsePtr response_;
};

using ReaderResult =
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError>;

class IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    UnusableSwbnFileError::Error>> {};

TEST_P(IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest,
       TestIntegrityBlockParserError) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*flags=*/{}, reader_future.GetCallback());

  auto error = web_package::mojom::BundleIntegrityBlockParseError::New();
  error->type = GetParam().first;
  error->message = "test error";
  parser_factory_->RunIntegrityBlockCallback(nullptr, error->Clone());

  EXPECT_THAT(reader_future.Take(), ErrorIs(UnusableSwbnFileError(error)));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      GetParam().second, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest,
    ::testing::Values(
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError),
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kVersionError,
            UnusableSwbnFileError::Error::kIntegrityBlockParserVersionError),
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kFormatError,
            UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError)));

TEST_F(IsolatedWebAppResponseReaderFactoryTest,
       TestInvalidIntegrityBlockContents) {
  base::HistogramTester histogram_tester;

  factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
      *profile(),
      std::make_unique<FakeIsolatedWebAppValidator>(
          base::unexpected("test error")),
      base::BindRepeating(
          []() -> std::unique_ptr<
                   web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<web_package::test::FakeSignatureVerifier>(
                std::nullopt);
          }));

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*flags=*/{}, reader_future.GetCallback());

  FulfillIntegrityBlock();

  ASSERT_THAT(
      reader_future.Take(),
      ErrorIs(Property(&UnusableSwbnFileError::message, Eq("test error"))));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      UnusableSwbnFileError::Error::kIntegrityBlockValidationError, 1);
}

class IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<std::tuple<VerifierError, bool>> {
 public:
  IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest()
      : IsolatedWebAppResponseReaderFactoryTest(),
        error_(std::get<0>(GetParam())),
        skip_signature_verification_(std::get<1>(GetParam())) {}

 protected:
  VerifierError error_;
  bool skip_signature_verification_;
};

TEST_P(IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest,
       SignatureVerificationError) {
  base::HistogramTester histogram_tester;

  factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
      *profile(), std::make_unique<FakeIsolatedWebAppValidator>(base::ok()),
      base::BindRepeating(
          [](VerifierError error)
              -> std::unique_ptr<
                  web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<web_package::test::FakeSignatureVerifier>(
                error);
          },
          error_));

  IsolatedWebAppResponseReaderFactory::Flags flags;
  if (skip_signature_verification_) {
    flags.Put(
        IsolatedWebAppResponseReaderFactory::Flag::kSkipSignatureVerification);
  }
  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId, flags,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();

  // When signature verification is skipped, then the signature verification
  // error seeded above should not be triggered.
  if (skip_signature_verification_) {
    FulfillMetadata();

    EXPECT_THAT(reader_future.Take(), HasValue());

    histogram_tester.ExpectBucketCount(
        ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
        UnusableSwbnFileError::Error::kSignatureVerificationError, 0);
  } else {
    ASSERT_THAT(
        reader_future.Take(),
        ErrorIs(Property(&UnusableSwbnFileError::message, Eq(error_.message))));

    histogram_tester.ExpectBucketCount(
        ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
        UnusableSwbnFileError::Error::kSignatureVerificationError, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest,
    ::testing::Combine(
        ::testing::Values(
            VerifierError::ForInternalError("internal error"),
            VerifierError::ForInvalidSignature("invalid signature")),
        // skip_signature_verification
        ::testing::Bool()));

class IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    UnusableSwbnFileError::Error>> {};

TEST_P(IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest,
       TestMetadataParserError) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*flags=*/{}, reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto error = web_package::mojom::BundleMetadataParseError::New();
  error->message = "test error";
  error->type = GetParam().first;
  parser_factory_->RunMetadataCallback(integrity_block_->size, nullptr,
                                       error->Clone());

  EXPECT_THAT(reader_future.Take(), ErrorIs(Eq(UnusableSwbnFileError(error))));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      GetParam().second, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest,
    ::testing::Values(
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            UnusableSwbnFileError::Error::kMetadataParserInternalError),
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kVersionError,
            UnusableSwbnFileError::Error::kMetadataParserVersionError),
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kFormatError,
            UnusableSwbnFileError::Error::kMetadataParserFormatError)));

TEST_F(IsolatedWebAppResponseReaderFactoryTest, TestInvalidMetadataPrimaryUrl) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*flags=*/{}, reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->primary_url = kUrl;
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  EXPECT_THAT(reader_future.Take(),
              ErrorIs(Property(&UnusableSwbnFileError::message,
                               StartsWith("Primary URL must not be present"))));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      UnusableSwbnFileError::Error::kMetadataValidationError, 1);
}

TEST_F(IsolatedWebAppResponseReaderFactoryTest,
       TestInvalidMetadataInvalidExchange) {
  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*flags=*/{}, reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->requests.insert_or_assign(
      GURL(kInvalidIsolatedWebAppUrl),
      web_package::mojom::BundleResponseLocation::New());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  EXPECT_THAT(
      reader_future.Take(),
      ErrorIs(Property(&UnusableSwbnFileError::message,
                       StartsWith("The URL of an exchange is invalid"))));
}

}  // namespace

}  // namespace web_app
