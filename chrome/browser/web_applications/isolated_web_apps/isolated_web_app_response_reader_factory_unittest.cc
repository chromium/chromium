// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;
using testing::NotNull;
using testing::StartsWith;
using testing::VariantWith;

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
      absl::optional<std::string> integrity_block_error)
      : IsolatedWebAppValidator(std::make_unique<IsolatedWebAppTrustChecker>(
            TestingPrefServiceSimple())),
        integrity_block_error_(integrity_block_error) {}

  void ValidateIntegrityBlock(
      const web_package::SignedWebBundleId& web_bundle_id,
      const web_package::SignedWebBundleIntegrityBlock& integrity_block,
      base::OnceCallback<void(absl::optional<std::string>)> callback) override {
    std::move(callback).Run(integrity_block_error_);
  }

 private:
  absl::optional<std::string> integrity_block_error_;
};

class FakeSignatureVerifier
    : public web_package::SignedWebBundleSignatureVerifier {
 public:
  explicit FakeSignatureVerifier(
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          error,
      base::RepeatingClosure on_verify_signatures = base::DoNothing())
      : error_(error), on_verify_signatures_(on_verify_signatures) {}

  void VerifySignatures(
      scoped_refptr<web_package::SharedFile> file,
      web_package::SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback) override {
    on_verify_signatures_.Run();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error_));
  }

 private:
  absl::optional<web_package::SignedWebBundleSignatureVerifier::Error> error_;
  base::RepeatingClosure on_verify_signatures_;
};

class IsolatedWebAppResponseReaderFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);

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

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->public_key = web_package::Ed25519PublicKey::Create(
        base::make_span(kEd25519PublicKey));
    signature_stack_entry->signature = web_package::Ed25519Signature::Create(
        base::make_span(kEd25519Signature));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    integrity_block_->size = 42;
    integrity_block_->signature_stack = std::move(signature_stack);

    factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
        std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
        base::BindRepeating(
            []() -> std::unique_ptr<
                     web_package::SignedWebBundleSignatureVerifier> {
              return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
            }));

    std::string test_file_data = kResponseBody;
    CHECK(temp_dir_.CreateUniqueTempDir());
    CHECK(CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path_));
    CHECK_EQ(test_file_data.size(), static_cast<size_t>(base::WriteFile(
                                        web_bundle_path_, test_file_data.data(),
                                        test_file_data.size())));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));
  }

  void TearDown() override { factory_.reset(); }

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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  base::FilePath web_bundle_path_;
  base::test::RepeatingTestFuture<absl::optional<GURL>>
      on_create_parser_future_;

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
                   IsolatedWebAppResponseReaderFactory::Error>;

class IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    IsolatedWebAppResponseReaderFactory::
                        ReadIntegrityBlockAndMetadataStatus>> {};

TEST_P(IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest,
       TestIntegrityBlockParserError) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*skip_signature_verification=*/false,
                                 reader_future.GetCallback());

  auto error = web_package::mojom::BundleIntegrityBlockParseError::New();
  error->type = GetParam().first;
  error->message = "test error";
  parser_factory_->RunIntegrityBlockCallback(nullptr, error->Clone());

  ReaderResult result = reader_future.Take();
  ASSERT_THAT(result.has_value(), IsFalse());
  auto* actual_error =
      absl::get_if<web_package::mojom::BundleIntegrityBlockParseErrorPtr>(
          &result.error());
  ASSERT_THAT(actual_error, NotNull());
  EXPECT_THAT((*actual_error)->type, Eq(error->type));
  EXPECT_THAT((*actual_error)->message, Eq(error->message));

  histogram_tester.ExpectBucketCount(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus", GetParam().second,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactoryIntegrityBlockParserErrorTest,
    ::testing::Values(
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            IsolatedWebAppResponseReaderFactory::
                ReadIntegrityBlockAndMetadataStatus::
                    kIntegrityBlockParserInternalError),
        std::make_pair(web_package::mojom::BundleParseErrorType::kVersionError,
                       IsolatedWebAppResponseReaderFactory::
                           ReadIntegrityBlockAndMetadataStatus::
                               kIntegrityBlockParserVersionError),
        std::make_pair(web_package::mojom::BundleParseErrorType::kFormatError,
                       IsolatedWebAppResponseReaderFactory::
                           ReadIntegrityBlockAndMetadataStatus::
                               kIntegrityBlockParserFormatError)));

TEST_F(IsolatedWebAppResponseReaderFactoryTest,
       TestInvalidIntegrityBlockContents) {
  base::HistogramTester histogram_tester;

  factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
      std::make_unique<FakeIsolatedWebAppValidator>("test error"),
      base::BindRepeating(
          []() -> std::unique_ptr<
                   web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
          }));

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*skip_signature_verification=*/false,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();

  ReaderResult result = reader_future.Take();
  ASSERT_THAT(result.has_value(), IsFalse());
  auto* actual_error = absl::get_if<IntegrityBlockError>(&result.error());
  ASSERT_THAT(actual_error, NotNull());
  EXPECT_THAT(actual_error->message, Eq("test error"));

  histogram_tester.ExpectBucketCount(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
      IsolatedWebAppResponseReaderFactory::ReadIntegrityBlockAndMetadataStatus::
          kIntegrityBlockValidationError,
      1);
}

class IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<
          std::tuple<web_package::SignedWebBundleSignatureVerifier::Error,
                     bool>> {
 public:
  IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest()
      : IsolatedWebAppResponseReaderFactoryTest(),
        error_(std::get<0>(GetParam())),
        skip_signature_verification_(std::get<1>(GetParam())) {}

 protected:
  web_package::SignedWebBundleSignatureVerifier::Error error_;
  bool skip_signature_verification_;
};

TEST_P(IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest,
       SignatureVerificationError) {
  base::HistogramTester histogram_tester;

  factory_ = std::make_unique<IsolatedWebAppResponseReaderFactory>(
      std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
      base::BindRepeating(
          [](web_package::SignedWebBundleSignatureVerifier::Error error)
              -> std::unique_ptr<
                  web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(error);
          },
          error_));

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 skip_signature_verification_,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();

  // When signature verification is skipped, then the signature verification
  // error seeded above should not be triggered.
  if (skip_signature_verification_) {
    FulfillMetadata();

    ReaderResult result = reader_future.Take();
    EXPECT_THAT(result.has_value(), IsTrue());

    histogram_tester.ExpectBucketCount(
        "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
        IsolatedWebAppResponseReaderFactory::
            ReadIntegrityBlockAndMetadataStatus::kSignatureVerificationError,
        0);
  } else {
    ReaderResult result = reader_future.Take();
    ASSERT_THAT(result.has_value(), IsFalse());
    auto* actual_error =
        absl::get_if<web_package::SignedWebBundleSignatureVerifier::Error>(
            &result.error());
    ASSERT_THAT(actual_error, NotNull());
    EXPECT_THAT(actual_error->message, Eq(error_.message));

    histogram_tester.ExpectBucketCount(
        "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
        IsolatedWebAppResponseReaderFactory::
            ReadIntegrityBlockAndMetadataStatus::kSignatureVerificationError,
        1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactorySignatureVerificationErrorTest,
    ::testing::Combine(
        ::testing::Values(web_package::SignedWebBundleSignatureVerifier::Error::
                              ForInternalError("internal error"),
                          web_package::SignedWebBundleSignatureVerifier::Error::
                              ForInvalidSignature("invalid signature")),
        // skip_signature_verification
        ::testing::Bool()));

class IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest
    : public IsolatedWebAppResponseReaderFactoryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    IsolatedWebAppResponseReaderFactory::
                        ReadIntegrityBlockAndMetadataStatus>> {};

TEST_P(IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest,
       TestMetadataParserError) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*skip_signature_verification=*/false,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto error = web_package::mojom::BundleMetadataParseError::New();
  error->message = "test error";
  error->type = GetParam().first;
  parser_factory_->RunMetadataCallback(integrity_block_->size, nullptr,
                                       error->Clone());

  ReaderResult result = reader_future.Take();
  ASSERT_THAT(result.has_value(), IsFalse());
  auto* actual_error =
      absl::get_if<web_package::mojom::BundleMetadataParseErrorPtr>(
          &result.error());
  ASSERT_THAT(actual_error, NotNull());
  EXPECT_THAT((*actual_error)->type, Eq(error->type));
  EXPECT_THAT((*actual_error)->message, Eq(error->message));

  histogram_tester.ExpectBucketCount(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus", GetParam().second,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppResponseReaderFactoryMetadataParserErrorTest,
    ::testing::Values(
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            IsolatedWebAppResponseReaderFactory::
                ReadIntegrityBlockAndMetadataStatus::
                    kMetadataParserInternalError),
        std::make_pair(web_package::mojom::BundleParseErrorType::kVersionError,
                       IsolatedWebAppResponseReaderFactory::
                           ReadIntegrityBlockAndMetadataStatus::
                               kMetadataParserVersionError),
        std::make_pair(web_package::mojom::BundleParseErrorType::kFormatError,
                       IsolatedWebAppResponseReaderFactory::
                           ReadIntegrityBlockAndMetadataStatus::
                               kMetadataParserFormatError)));

TEST_F(IsolatedWebAppResponseReaderFactoryTest, TestInvalidMetadataPrimaryUrl) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*skip_signature_verification=*/false,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->primary_url = kUrl;
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  ReaderResult result = reader_future.Take();
  ASSERT_THAT(result.has_value(), IsFalse());
  auto* actual_error = absl::get_if<MetadataError>(&result.error());
  ASSERT_THAT(actual_error, NotNull());
  EXPECT_THAT(actual_error->message,
              StartsWith("Primary URL must not be present"));

  histogram_tester.ExpectBucketCount(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
      IsolatedWebAppResponseReaderFactory::ReadIntegrityBlockAndMetadataStatus::
          kMetadataValidationError,
      1);
}

TEST_F(IsolatedWebAppResponseReaderFactoryTest,
       TestInvalidMetadataInvalidExchange) {
  base::test::TestFuture<ReaderResult> reader_future;
  factory_->CreateResponseReader(web_bundle_path_, kWebBundleId,
                                 /*skip_signature_verification=*/false,
                                 reader_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->requests.insert_or_assign(
      GURL(kInvalidIsolatedWebAppUrl),
      web_package::mojom::BundleResponseLocation::New());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  ReaderResult result = reader_future.Take();
  ASSERT_THAT(result.has_value(), IsFalse());
  auto* actual_error = absl::get_if<MetadataError>(&result.error());
  ASSERT_THAT(actual_error, NotNull());
  EXPECT_THAT(actual_error->message,
              StartsWith("The URL of an exchange is invalid"));
}

}  // namespace

}  // namespace web_app
