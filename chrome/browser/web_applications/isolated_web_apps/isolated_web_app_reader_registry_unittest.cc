// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
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
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "chrome/common/url_constants.h"
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
#include "url/url_constants.h"

namespace web_app {

namespace {

using testing::ElementsAre;

using ReadResponseError = IsolatedWebAppReaderRegistry::ReadResponseError;
using VerifierError = web_package::SignedWebBundleSignatureVerifier::Error;

constexpr uint8_t kEd25519PublicKey[32] = {0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 2, 2, 0, 0, 0};

constexpr uint8_t kEd25519Signature[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

// This class needs to be a IsolatedWebAppVaidator, but also must provide
// a TestingPrefServiceSimple that outlives it. So rather than making
// TestingPrefServiceSimple a member, make it the leftmost base class.
class FakeIsolatedWebAppValidator : public TestingPrefServiceSimple,
                                    public IsolatedWebAppValidator {
 public:
  explicit FakeIsolatedWebAppValidator(
      absl::optional<std::string> integrity_block_error)
      : IsolatedWebAppValidator(std::make_unique<IsolatedWebAppTrustChecker>(
            // Disambiguate the constructor using the form that takes the
            // already-initialized leftmost base class, rather than the copy
            // constructor for the uninitialized rightmost base class.
            *static_cast<TestingPrefServiceSimple*>(this))),
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
      absl::optional<VerifierError> error,
      base::RepeatingClosure on_verify_signatures = base::DoNothing())
      : error_(error), on_verify_signatures_(on_verify_signatures) {}

  void VerifySignatures(
      base::File file,
      web_package::SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback) override {
    on_verify_signatures_.Run();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error_));
  }

 private:
  absl::optional<VerifierError> error_;
  base::RepeatingClosure on_verify_signatures_;
};

}  // namespace

class IsolatedWebAppReaderRegistryTest : public ::testing::Test {
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

    registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
        std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
        base::BindRepeating(
            []() -> std::unique_ptr<
                     web_package::SignedWebBundleSignatureVerifier> {
              return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
            }));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path_));
    EXPECT_TRUE(base::WriteFile(web_bundle_path_, kResponseBody));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));
  }

  void TearDown() override { registry_.reset(); }

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

  std::unique_ptr<IsolatedWebAppReaderRegistry> registry_;
  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;
  web_package::mojom::BundleMetadataPtr metadata_;
  web_package::mojom::BundleResponsePtr response_;
};

using ReadResult =
    base::expected<IsolatedWebAppResponseReader::Response, ReadResponseError>;

TEST_F(IsolatedWebAppReaderRegistryTest, TestSingleRequest) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                       read_response_future.Take());
  EXPECT_EQ(response.head()->response_code, 200);

  GURL expected_parser_base_url(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    kWebBundleId.id()}));
  EXPECT_EQ(expected_parser_base_url, on_create_parser_future_.Take());

  histogram_tester.ExpectBucketCount(
      ToSuccessHistogramName("WebApp.Isolated.SwbnFileUsability"),
      /*success*/ 1, 1);

  std::string response_body = ReadAndFulfillResponseBody(
      response.head()->payload_length,
      base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                     base::Unretained(&response)));
  EXPECT_EQ(kResponseBody, response_body);
}

TEST_F(IsolatedWebAppReaderRegistryTest,
       TestSingleRequestWithQueryAndFragment) {
  network::ResourceRequest resource_request;
  resource_request.url = kUrl.Resolve("/?bar=baz#foo");

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                       read_response_future.Take());
  EXPECT_EQ(response.head()->response_code, 200);

  std::string response_body = ReadAndFulfillResponseBody(
      response.head()->payload_length,
      base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                     base::Unretained(&response)));
  EXPECT_EQ(kResponseBody, response_body);
}

TEST_F(IsolatedWebAppReaderRegistryTest,
       TestReadingResponseAfterSignedWebBundleReaderIsDeleted) {
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                       read_response_future.Take());
  EXPECT_EQ(response.head()->response_code, 200);

  // Delete the registry so that the `SignedWebBundleReader`, which `response`
  // holds onto weakly, is deleted, which should make `response.ReadBody()`
  // fail with `net::ERR_FAILED`.
  registry_.reset();

  base::test::TestFuture<net::Error> error_future;
  ReadResponseBody(
      response.head()->payload_length,
      base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                     base::Unretained(&response)),
      error_future.GetCallback());
  EXPECT_EQ(net::ERR_FAILED, error_future.Take());
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestRequestToNonExistingResponse) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = GURL(kUrl.spec() + "foo");

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(result.error().message,
            "Failed to read response: The Web Bundle does not contain a "
            "response for the provided URL: "
            "isolated-app://"
            "aaaaaaacaibaaaaaaaaaaaaaaiaaeaaaaaaaaaaaaabaeaqaaaaaaaic/foo");

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.ReadResponseHead"),
      IsolatedWebAppReaderRegistry::ReadResponseHeadError::
          kResponseNotFoundError,
      1);
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestSignedWebBundleReaderLifetime) {
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  size_t num_signature_verifications = 0;
  registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
      std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
      base::BindLambdaForTesting(
          [&]() -> std::unique_ptr<
                    web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(
                absl::nullopt, base::BindLambdaForTesting(
                                   [&]() { ++num_signature_verifications; }));
          }));

  // Verify that the cache cleanup timer has not yet started.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0ul)
      << (task_environment_.DescribeCurrentTasks(),
          "Pending Tasks have been logged.");

  {
    base::test::TestFuture<ReadResult> read_response_future;
    registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                            read_response_future.GetCallback());

    // `SignedWebBundleReader`s should not be evicted from the cache while they
    // are still parsing integrity block and metadata, thus the following two
    // calls to fast forward time should not have any effect.
    task_environment_.FastForwardBy(base::Hours(1));

    FulfillIntegrityBlock();

    task_environment_.FastForwardBy(base::Hours(1));

    FulfillMetadata();
    FulfillResponse(resource_request);

    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future.Take());
    EXPECT_EQ(response.head()->response_code, 200);
  }

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(num_signature_verifications, 0ul);
#else
  EXPECT_EQ(num_signature_verifications, 1ul);
#endif

  // Verify that the cache cleanup timer has started.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1ul)
      << (task_environment_.DescribeCurrentTasks(),
          "Pending Tasks have been logged.");

  {
    base::test::TestFuture<ReadResult> read_response_future;
    registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                            read_response_future.GetCallback());

    // Notably, no `FulfillIntegrityBlock` or `FulfillMetadata` here, since the
    // `SignedWebBundleReader` should still be cached.
    FulfillResponse(resource_request);

    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future.Take());
    EXPECT_EQ(response.head()->response_code, 200);
  }

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(num_signature_verifications, 0ul);
#else
  EXPECT_EQ(num_signature_verifications, 1ul);
#endif

  // Verify that the cache cleanup timer is still running.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1ul)
      << (task_environment_.DescribeCurrentTasks(),
          "Pending Tasks have been logged.");

  // After some time has passed, the `SignedWebBundleReader` should be evicted
  // from the cache.
  task_environment_.FastForwardBy(base::Hours(1));

  // Verify that the cache cleanup timer has stopped, given that the cache is
  // now empty again.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0ul)
      << (task_environment_.DescribeCurrentTasks(),
          "Pending Tasks have been logged.");

  {
    base::test::TestFuture<ReadResult> read_response_future;
    registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                            read_response_future.GetCallback());

    // Since the SignedWebBundleReader has been evicted from cache, integrity
    // block and metadata have to be read again.
    FulfillIntegrityBlock();
    FulfillMetadata();
    FulfillResponse(resource_request);

    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future.Take());
    EXPECT_EQ(response.head()->response_code, 200);
  }

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(num_signature_verifications, 0ul);
#else
  // Signatures should not have been verified again, since we only verify them
  // once per session per file path.
  EXPECT_EQ(num_signature_verifications, 1ul);
#endif

  // Verify that the cache cleanup timer has started again.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1ul)
      << (task_environment_.DescribeCurrentTasks(),
          "Pending Tasks have been logged.");
}

class IsolatedWebAppReaderRegistryIntegrityBlockParserErrorTest
    : public IsolatedWebAppReaderRegistryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    UnusableSwbnFileError::Error>> {};

TEST_P(IsolatedWebAppReaderRegistryIntegrityBlockParserErrorTest,
       TestIntegrityBlockParserError) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  auto error = web_package::mojom::BundleIntegrityBlockParseError::New();
  error->type = GetParam().first;
  error->message = "test error";
  parser_factory_->RunIntegrityBlockCallback(nullptr, std::move(error));

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to parse integrity block: test error");

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      GetParam().second, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppReaderRegistryIntegrityBlockParserErrorTest,
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

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidIntegrityBlockContents) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
      std::make_unique<FakeIsolatedWebAppValidator>("test error"),
      base::BindRepeating(
          []() -> std::unique_ptr<
                   web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
          }));

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to validate integrity block: test error");

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      UnusableSwbnFileError::Error::kIntegrityBlockValidationError, 1);
}

class IsolatedWebAppReaderRegistrySignatureVerificationErrorTest
    : public IsolatedWebAppReaderRegistryTest,
      public ::testing::WithParamInterface<VerifierError> {};

TEST_P(IsolatedWebAppReaderRegistrySignatureVerificationErrorTest,
       SignatureVerificationError) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
      std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
      base::BindRepeating(
          []() -> std::unique_ptr<
                   web_package::SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(GetParam());
          }));

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, signatures are only verified at installation-time, thus the
  // `FakeSignatureVerifier` set up above will never be called.
  FulfillMetadata();
  FulfillResponse(resource_request);

  ASSERT_TRUE(read_response_future.Take().has_value());

  histogram_tester.ExpectBucketCount(
      ToSuccessHistogramName("WebApp.Isolated.SwbnFileUsability"),
      /*success*/ 1, 1);
#else
  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            base::StringPrintf("Failed to verify signatures: %s",
                               GetParam().message.c_str()));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      UnusableSwbnFileError::Error::kSignatureVerificationError, 1);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppReaderRegistrySignatureVerificationErrorTest,
    ::testing::Values(VerifierError::ForInternalError("internal error"),
                      VerifierError::ForInvalidSignature("invalid signature")));

class IsolatedWebAppReaderRegistryMetadataParserErrorTest
    : public IsolatedWebAppReaderRegistryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    UnusableSwbnFileError::Error>> {};

TEST_P(IsolatedWebAppReaderRegistryMetadataParserErrorTest,
       TestMetadataParserError) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto error = web_package::mojom::BundleMetadataParseError::New();
  error->message = "test error";
  error->type = GetParam().first;
  parser_factory_->RunMetadataCallback(integrity_block_->size, nullptr,
                                       std::move(error));

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message, "Failed to parse metadata: test error");

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      GetParam().second, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppReaderRegistryMetadataParserErrorTest,
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

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidMetadataPrimaryUrl) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->primary_url = kUrl;
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            base::StringPrintf("Failed to validate metadata: Primary URL must "
                               "not be present, but was %s",
                               kUrl.spec().c_str()));

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.SwbnFileUsability"),
      UnusableSwbnFileError::Error::kMetadataValidationError, 1);
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidMetadataInvalidExchange) {
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->requests.insert_or_assign(
      GURL(kInvalidIsolatedWebAppUrl),
      web_package::mojom::BundleResponseLocation::New());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to validate metadata: The URL of an exchange is invalid: "
            "The host of isolated-app:// URLs must be a valid Signed Web "
            "Bundle ID (got foo): The signed web bundle ID must be exactly 56 "
            "characters long, but was 3 characters long.");
}

class IsolatedWebAppReaderRegistryResponseHeadParserErrorTest
    : public IsolatedWebAppReaderRegistryTest,
      public ::testing::WithParamInterface<
          std::pair<web_package::mojom::BundleParseErrorType,
                    IsolatedWebAppReaderRegistry::ReadResponseHeadError>> {};

TEST_P(IsolatedWebAppReaderRegistryResponseHeadParserErrorTest,
       TestResponseHeadParserError) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<ReadResult> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();

  auto error = web_package::mojom::BundleResponseParseError::New();
  error->message = "test error";
  error->type = GetParam().first;
  parser_factory_->RunResponseCallback(
      web_package::mojom::BundleResponseLocation::New(
          response_->payload_offset, response_->payload_length),
      nullptr, std::move(error));

  ReadResult result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type, ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to parse response head: test error");

  histogram_tester.ExpectBucketCount(
      ToErrorHistogramName("WebApp.Isolated.ReadResponseHead"),
      GetParam().second, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppReaderRegistryResponseHeadParserErrorTest,
    ::testing::Values(
        std::make_pair(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            IsolatedWebAppReaderRegistry::ReadResponseHeadError::
                kResponseHeadParserInternalError),
        std::make_pair(web_package::mojom::BundleParseErrorType::kFormatError,
                       IsolatedWebAppReaderRegistry::ReadResponseHeadError::
                           kResponseHeadParserFormatError)));

TEST_F(IsolatedWebAppReaderRegistryTest, TestConcurrentRequests) {
  using ReaderCacheState = IsolatedWebAppReaderRegistry::ReaderCacheState;
  base::HistogramTester histogram_tester;

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  // Simulate two simultaneous requests for the same web bundle
  base::test::TestFuture<ReadResult> read_response_future_1;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_1.GetCallback());

  histogram_tester.GetAllSamples("WebApp.Isolated.ResponseReaderCacheState"),
      ElementsAre(base::Bucket(ReaderCacheState::kNotCached, 1),
                  base::Bucket(ReaderCacheState::kCachedReady, 0),
                  base::Bucket(ReaderCacheState::kCachedPending, 0));

  base::test::TestFuture<ReadResult> read_response_future_2;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_2.GetCallback());

  histogram_tester.GetAllSamples("WebApp.Isolated.ResponseReaderCacheState"),
      ElementsAre(base::Bucket(ReaderCacheState::kNotCached, 1),
                  base::Bucket(ReaderCacheState::kCachedReady, 0),
                  base::Bucket(ReaderCacheState::kCachedPending, 1));

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);
  {
    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future_1.Take());
    EXPECT_EQ(response.head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        response.head()->payload_length,
        base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                       base::Unretained(&response)));
    EXPECT_EQ(kResponseBody, response_body);
  }

  FulfillResponse(resource_request);
  {
    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future_2.Take());
    EXPECT_EQ(response.head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        response.head()->payload_length,
        base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                       base::Unretained(&response)));
    EXPECT_EQ(kResponseBody, response_body);
  }

  base::test::TestFuture<ReadResult> read_response_future_3;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_3.GetCallback());

  histogram_tester.GetAllSamples("WebApp.Isolated.ResponseReaderCacheState"),
      ElementsAre(base::Bucket(ReaderCacheState::kNotCached, 1),
                  base::Bucket(ReaderCacheState::kCachedReady, 1),
                  base::Bucket(ReaderCacheState::kCachedPending, 1));

  FulfillResponse(resource_request);
  {
    ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                         read_response_future_3.Take());
    EXPECT_EQ(response.head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        response.head()->payload_length,
        base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                       base::Unretained(&response)));
    EXPECT_EQ(kResponseBody, response_body);
  }
}

// TODO(crbug.com/1365853): Add a test that checks the behavior when
// `SignedWebBundleReader`s for two different Web Bundle IDs are requested
// concurrently. Testing this is currently not possible, since running two
// `MockWebBundleParser`s at the same time is not yet possible.

}  // namespace web_app
