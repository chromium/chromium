// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_verifier.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

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
      : integrity_block_error_(integrity_block_error) {}

  [[nodiscard]] absl::optional<std::string> ValidateIntegrityBlock(
      web_package::SignedWebBundleId web_bundle_id,
      const std::vector<web_package::Ed25519PublicKey>& public_key_stack)
      override {
    return integrity_block_error_;
  }

 private:
  absl::optional<std::string> integrity_block_error_;
};

class FakeSignatureVerifier : public SignedWebBundleSignatureVerifier {
 public:
  explicit FakeSignatureVerifier(
      absl::optional<SignedWebBundleSignatureVerifier::Error> error)
      : error_(error) {}

  void VerifySignatures(scoped_refptr<web_package::SharedFile> file,
                        SignedWebBundleIntegrityBlock integrity_block,
                        SignatureVerificationCallback callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error_));
  }

 private:
  absl::optional<SignedWebBundleSignatureVerifier::Error> error_;
};

}  // namespace

class IsolatedWebAppReaderRegistryTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);

    parser_factory_ =
        std::make_unique<web_package::MockWebBundleParserFactory>();

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr>
        requests;
    requests.insert(
        {kPrimaryUrl,
         web_package::mojom::BundleResponseLocation::New(
             response_->payload_offset, response_->payload_length)});

    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->primary_url = kPrimaryUrl;
    metadata_->requests = std::move(requests);

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->public_key =
        std::vector(std::begin(kEd25519PublicKey), std::end(kEd25519PublicKey));
    signature_stack_entry->signature =
        std::vector(std::begin(kEd25519Signature), std::end(kEd25519Signature));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    integrity_block_->size = 42;
    integrity_block_->signature_stack = std::move(signature_stack);

    registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
        std::make_unique<IsolatedWebAppValidator>(),
        base::BindRepeating(
            []() -> std::unique_ptr<SignedWebBundleSignatureVerifier> {
              return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
            }));

    std::string test_file_data = kResponseBody;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path_));
    EXPECT_EQ(
        test_file_data.size(),
        static_cast<size_t>(base::WriteFile(
            web_bundle_path_, test_file_data.data(), test_file_data.size())));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));
  }

  void TearDown() override {
    registry_.reset();
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

  const web_package::SignedWebBundleId kWebBundleId =
      *web_package::SignedWebBundleId::Create(
          "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac");
  const GURL kPrimaryUrl = GURL("isolated-app://" + kWebBundleId.id());

  constexpr static char kResponseBody[] = "test";

  constexpr static char kInvalidIsolatedAppUrl[] = "isolated-app://foo/";

  std::unique_ptr<IsolatedWebAppReaderRegistry> registry_;
  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;
  web_package::mojom::BundleMetadataPtr metadata_;
  web_package::mojom::BundleResponsePtr response_;
};

using Result = base::expected<IsolatedWebAppReaderRegistry::Response,
                              IsolatedWebAppReaderRegistry::ReadResponseError>;

TEST_F(IsolatedWebAppReaderRegistryTest, TestSingleRequest) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  Result result = read_response_future.Take();
  ASSERT_TRUE(result.has_value()) << result.error().message;
  EXPECT_EQ(result->head()->response_code, 200);

  std::string response_body = ReadAndFulfillResponseBody(
      result->head()->payload_length,
      base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                     base::Unretained(&*result)));
  EXPECT_EQ(kResponseBody, response_body);
}

TEST_F(IsolatedWebAppReaderRegistryTest,
       TestSingleRequestWithQueryAndFragment) {
  network::ResourceRequest resource_request;
  resource_request.url = GURL(kPrimaryUrl.spec() + "?bar=baz#foo");

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  Result result = read_response_future.Take();
  ASSERT_TRUE(result.has_value()) << result.error().message;
  EXPECT_EQ(result->head()->response_code, 200);

  std::string response_body = ReadAndFulfillResponseBody(
      result->head()->payload_length,
      base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                     base::Unretained(&*result)));
  EXPECT_EQ(kResponseBody, response_body);
}

TEST_F(IsolatedWebAppReaderRegistryTest,
       TestReadingResponseAfterSignedWebBundleReaderIsDeleted) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);

  Result result = read_response_future.Take();
  ASSERT_TRUE(result.has_value()) << result.error().message;
  EXPECT_EQ(result->head()->response_code, 200);

  // Delete the registry so that the `SignedWebBundleReader`, which `result`
  // holds onto weakly, is deleted, which should make `result->ReadBody()`
  // fail with `net::ERR_FAILED`.
  registry_.reset();

  base::test::TestFuture<net::Error> error_future;
  ReadResponseBody(
      result->head()->payload_length,
      base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                     base::Unretained(&*result)),
      error_future.GetCallback());
  EXPECT_EQ(net::ERR_FAILED, error_future.Take());
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestRequestToNonExistingResponse) {
  network::ResourceRequest resource_request;
  resource_request.url = GURL(kPrimaryUrl.spec() + "foo");

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().type,
      IsolatedWebAppReaderRegistry::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(result.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/foo");
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidIntegrityBlock) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  auto error = web_package::mojom::BundleIntegrityBlockParseError::New();
  error->message = "test error";
  parser_factory_->RunIntegrityBlockCallback(nullptr, std::move(error));

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to parse integrity block: test error");
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestUntrustedPublicKeys) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
      std::make_unique<FakeIsolatedWebAppValidator>("test error"),
      base::BindRepeating(
          []() -> std::unique_ptr<SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(absl::nullopt);
          }));

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Public keys of the Isolated Web App are untrusted: test error");
}

class IsolatedWebAppReaderRegistrySignatureVerificationErrorTest
    : public IsolatedWebAppReaderRegistryTest,
      public ::testing::WithParamInterface<
          SignedWebBundleSignatureVerifier::Error> {};

TEST_P(IsolatedWebAppReaderRegistrySignatureVerificationErrorTest,
       SignatureVerificationError) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  registry_ = std::make_unique<IsolatedWebAppReaderRegistry>(
      std::make_unique<FakeIsolatedWebAppValidator>(absl::nullopt),
      base::BindRepeating(
          []() -> std::unique_ptr<SignedWebBundleSignatureVerifier> {
            return std::make_unique<FakeSignatureVerifier>(GetParam());
          }));

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, signatures are only verified at installation-time, thus the
  // `FakeSignatureVerifier` set up above will never be called.
  // TODO(crbug.com/1366309): Make sure signatures are actually verified during
  // installation once installation is implemented.
  FulfillMetadata();
  FulfillResponse(resource_request);

  Result result = read_response_future.Take();
  ASSERT_TRUE(result.has_value()) << result.error().message;
#else
  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            base::StringPrintf("Failed to verify signatures: %s",
                               GetParam().message.c_str()));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppReaderRegistrySignatureVerificationErrorTest,
    ::testing::Values(
        SignedWebBundleSignatureVerifier::Error::ForInternalError(
            "internal error"),
        SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
            "invalid signature")));

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidMetadata) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto error = web_package::mojom::BundleMetadataParseError::New();
  error->message = "test error";
  parser_factory_->RunMetadataCallback(integrity_block_->size, nullptr,
                                       std::move(error));

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message, "Failed to parse metadata: test error");
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidMetadataPrimaryUrl) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->primary_url = GURL(kInvalidIsolatedAppUrl);
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(
      result.error().message,
      base::StringPrintf("Invalid metadata: Primary URL must be %s, but "
                         "was %s",
                         kPrimaryUrl.spec().c_str(), kInvalidIsolatedAppUrl));
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidMetadataInvalidExchange) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  auto metadata = metadata_->Clone();
  metadata->requests.insert_or_assign(
      GURL(kInvalidIsolatedAppUrl),
      web_package::mojom::BundleResponseLocation::New());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Invalid metadata: The URL of an exchange is invalid: The host of "
            "isolated-app:// URLs must be a valid Signed Web Bundle ID (got "
            "foo): The signed web bundle ID must be exactly 56 characters "
            "long, but was 3 characters long.");
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestInvalidResponse) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  base::test::TestFuture<Result> read_response_future;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();

  auto error = web_package::mojom::BundleResponseParseError::New();
  error->message = "test error";
  parser_factory_->RunResponseCallback(
      web_package::mojom::BundleResponseLocation::New(
          response_->payload_offset, response_->payload_length),
      nullptr, std::move(error));

  Result result = read_response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().type,
            IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError);
  EXPECT_EQ(result.error().message,
            "Failed to parse response head: test error");
}

TEST_F(IsolatedWebAppReaderRegistryTest, TestConcurrentRequests) {
  network::ResourceRequest resource_request;
  resource_request.url = kPrimaryUrl;

  // Simulate two simultaneous requests for the same web bundle
  base::test::TestFuture<Result> read_response_future_1;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_1.GetCallback());
  base::test::TestFuture<Result> read_response_future_2;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_2.GetCallback());

  FulfillIntegrityBlock();
  FulfillMetadata();
  FulfillResponse(resource_request);
  {
    Result result = read_response_future_1.Take();
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        result->head()->payload_length,
        base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                       base::Unretained(&*result)));
    EXPECT_EQ(kResponseBody, response_body);
  }

  FulfillResponse(resource_request);
  {
    Result result = read_response_future_2.Take();
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        result->head()->payload_length,
        base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                       base::Unretained(&*result)));
    EXPECT_EQ(kResponseBody, response_body);
  }

  base::test::TestFuture<Result> read_response_future_3;
  registry_->ReadResponse(web_bundle_path_, kWebBundleId, resource_request,
                          read_response_future_3.GetCallback());

  FulfillResponse(resource_request);
  {
    Result result = read_response_future_3.Take();
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->head()->response_code, 200);

    std::string response_body = ReadAndFulfillResponseBody(
        result->head()->payload_length,
        base::BindOnce(&IsolatedWebAppReaderRegistry::Response::ReadBody,
                       base::Unretained(&*result)));
    EXPECT_EQ(kResponseBody, response_body);
  }
}

// TODO(crbug.com/1365853): Add a test that checks the behavior when
// `SignedWebBundleReader`s for two different Web Bundle IDs are requested
// concurrently. Testing this is currently not possible, since running two
// `MockWebBundleParser`s at the same time is not yet possible.

}  // namespace web_app
