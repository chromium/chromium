// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

namespace {

using testing::Eq;

constexpr std::array<uint8_t, 32> kEd25519PublicKey = {
    0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2,
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

class FakeSignatureVerifier
    : public web_package::SignedWebBundleSignatureVerifier {
 public:
  explicit FakeSignatureVerifier(
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          error)
      : error_(error) {}

  void VerifySignatures(
      scoped_refptr<web_package::SharedFile> file,
      web_package::SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error_));
  }

 private:
  absl::optional<web_package::SignedWebBundleSignatureVerifier::Error> error_;
};

}  // namespace

class SignedWebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    parser_factory_ =
        std::make_unique<web_package::MockWebBundleParserFactory>();

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert(
        {kUrl, web_package::mojom::BundleResponseLocation::New(
                   response_->payload_offset, response_->payload_length)});
    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->primary_url = kUrl;
    metadata_->requests = std::move(items);

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
    integrity_block_->size = 123;
    integrity_block_->signature_stack = std::move(signature_stack);
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  using VerificationAction = SignedWebBundleReader::SignatureVerificationAction;

  std::unique_ptr<SignedWebBundleReader> CreateReaderAndInitialize(
      SignedWebBundleReader::ReadErrorCallback callback,
      VerificationAction verification_action =
          VerificationAction::ContinueAndVerifySignatures(),
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          signature_verifier_error = absl::nullopt,
      const absl::optional<GURL>& base_url = absl::nullopt,
      const std::string test_file_data = kResponseBody) {
    // Provide a buffer that contains the contents of just a single
    // response. We do not need to provide an integrity block or metadata
    // here, since reading them is completely mocked. Only response bodies
    // are read from an actual (temporary) file.
    base::FilePath temp_file_path;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path));
    EXPECT_EQ(test_file_data.size(), static_cast<size_t>(base::WriteFile(
                                         temp_file_path, test_file_data.data(),
                                         test_file_data.size())));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));

    std::unique_ptr<SignedWebBundleReader> reader =
        SignedWebBundleReader::Create(
            temp_file_path, base_url,
            std::make_unique<FakeSignatureVerifier>(signature_verifier_error));

    reader->StartReading(
        base::BindLambdaForTesting(
            [verification_action](
                web_package::SignedWebBundleIntegrityBlock integrity_block,
                base::OnceCallback<void(VerificationAction)> callback) {
              EXPECT_THAT(integrity_block.signature_stack().size(), Eq(1ul));
              EXPECT_THAT(integrity_block.signature_stack()
                              .entries()[0]
                              .public_key()
                              .bytes(),
                          Eq(kEd25519PublicKey));

              std::move(callback).Run(verification_action);
            }),
        std::move(callback));

    return reader;
  }

  base::expected<web_package::mojom::BundleResponsePtr,
                 SignedWebBundleReader::ReadResponseError>
  ReadAndFulfillResponse(
      SignedWebBundleReader& reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_read_response_args,
      web_package::mojom::BundleResponsePtr response,
      web_package::mojom::BundleResponseParseErrorPtr error = nullptr) {
    base::test::TestFuture<
        base::expected<web_package::mojom::BundleResponsePtr,
                       SignedWebBundleReader::ReadResponseError>>
        response_result;
    reader.ReadResponse(resource_request, response_result.GetCallback());

    parser_factory_->RunResponseCallback(std::move(expected_read_response_args),
                                         std::move(response), std::move(error));

    return response_result.Take();
  }

  void SimulateAndWaitForParserDisconnect(SignedWebBundleReader& reader) {
    base::RunLoop run_loop;
    reader.SetParserDisconnectCallbackForTesting(run_loop.QuitClosure());
    parser_factory_->SimulateParserDisconnect();
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;

  const GURL kUrl = GURL("https://example.com");
  web_package::mojom::BundleMetadataPtr metadata_;

  constexpr static char kResponseBody[] = "test";
  web_package::mojom::BundleResponsePtr response_;
};

TEST_F(SignedWebBundleReaderTest, ReadValidIntegrityBlockAndMetadata) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  base::HistogramTester histogram_tester;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_EQ(reader->GetPrimaryURL(), kUrl);
  EXPECT_EQ(reader->GetEntries().size(), 1ul);
  EXPECT_EQ(reader->GetEntries()[0], kUrl);

  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationFileLength", 1);
}

TEST_F(SignedWebBundleReaderTest,
       ReadValidIntegrityBlockAndMetadataWithoutPrimaryUrl) {
  auto metadata = metadata_->Clone();
  metadata->primary_url = absl::nullopt;

  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       std::move(metadata));

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_FALSE(reader->GetPrimaryURL().has_value());
  EXPECT_EQ(reader->GetEntries().size(), 1ul);
  EXPECT_EQ(reader->GetEntries()[0], kUrl);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockError) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(
      nullptr, web_package::mojom::BundleIntegrityBlockParseError::New());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadInvalidIntegrityBlockSize) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  web_package::mojom::BundleIntegrityBlockPtr integrity_block =
      web_package::mojom::BundleIntegrityBlock::New();
  integrity_block->size = 0;
  parser_factory_->RunIntegrityBlockCallback(std::move(integrity_block));

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockWithParserCrash) {
  parser_factory_->SimulateParseIntegrityBlockCrash();
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
  EXPECT_EQ(absl::get<web_package::mojom::BundleIntegrityBlockParseErrorPtr>(
                *parse_error)
                ->type,
            web_package::mojom::BundleParseErrorType::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockAndAbort) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader =
      CreateReaderAndInitialize(parse_error_future.GetCallback(),
                                VerificationAction::Abort("test error"));

  parser_factory_->RunIntegrityBlockCallback(integrity_block_.Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  ASSERT_TRUE(parse_error.has_value());

  auto* error =
      absl::get_if<SignedWebBundleReader::AbortedByCaller>(&*parse_error);
  ASSERT_TRUE(error);
  EXPECT_EQ(error->message, "test error");
}

class SignedWebBundleReaderSignatureVerificationErrorTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<
          web_package::SignedWebBundleSignatureVerifier::Error> {};

TEST_P(SignedWebBundleReaderSignatureVerificationErrorTest,
       SignatureVerificationError) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(
      parse_error_future.GetCallback(),
      VerificationAction::ContinueAndVerifySignatures(), GetParam());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  ASSERT_TRUE(parse_error.has_value());

  auto* error =
      absl::get_if<web_package::SignedWebBundleSignatureVerifier::Error>(
          &*parse_error);
  ASSERT_TRUE(error);
  EXPECT_EQ(error->message, GetParam().message);
  EXPECT_EQ(error->type, GetParam().type);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleReaderSignatureVerificationErrorTest,
    ::testing::Values(
        web_package::SignedWebBundleSignatureVerifier::Error::ForInternalError(
            "internal error"),
        web_package::SignedWebBundleSignatureVerifier::Error::
            ForInvalidSignature("invalid signature")));

#if BUILDFLAG(IS_CHROMEOS)

// Test that signatures are not verified when the
// `integrity_block_callback` asks to skip signature verification and
// thus the provided `web_package::SignedWebBundleSignatureVerifier::Error` is
// never triggered.
TEST_F(SignedWebBundleReaderTest,
       ReadIntegrityBlockAndSkipSignatureVerification) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(
      parse_error_future.GetCallback(),
      VerificationAction::ContinueAndSkipSignatureVerification(),
      web_package::SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
          "invalid signature"));

  parser_factory_->RunIntegrityBlockCallback(integrity_block_.Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SignedWebBundleReaderTest, ReadMetadataError) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(
      integrity_block_->size, nullptr,
      web_package::mojom::BundleMetadataParseError::New());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<web_package::mojom::BundleMetadataParseErrorPtr>(
          *parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadMetadataWithParserCrash) {
  parser_factory_->SimulateParseMetadataCrash();
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<web_package::mojom::BundleMetadataParseErrorPtr>(
          *parse_error));
  EXPECT_EQ(
      absl::get<web_package::mojom::BundleMetadataParseErrorPtr>(*parse_error)
          ->type,
      web_package::mojom::BundleParseErrorType::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadResponse) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;
  EXPECT_EQ((*response)->response_code, 200);
  EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
  EXPECT_EQ((*response)->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithFragment) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetRefStr("baz");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;
  EXPECT_EQ((*response)->response_code, 200);
  EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
  EXPECT_EQ((*response)->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithPath) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetPathStr("/foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "https://example.com/foo");
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithQuery) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetQueryStr("foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "https://example.com/?foo");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseError) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request, metadata_->requests[kUrl]->Clone(),
      nullptr,
      web_package::mojom::BundleResponseParseError::New(
          web_package::mojom::BundleParseErrorType::kFormatError, "test"));
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kFormatError);
  EXPECT_EQ(response.error().message, "test");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserDisconnect) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                           metadata_->requests[kUrl]->Clone(),
                                           response_->Clone());
    EXPECT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ((*response)->response_code, 200);
    EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
    EXPECT_EQ((*response)->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 2);

  // Simulate another disconnect to verify that the reader can recover from
  // multiple disconnects over the course of its lifetime.
  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                           metadata_->requests[kUrl]->Clone(),
                                           response_->Clone());
    EXPECT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ((*response)->response_code, 200);
    EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
    EXPECT_EQ((*response)->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 3);
}

TEST_F(SignedWebBundleReaderTest,
       SimulateParserDisconnectWithFileErrorWhenReconnecting) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  SimulateAndWaitForParserDisconnect(*reader.get());
  reader->SetReconnectionFileErrorForTesting(
      base::File::Error::FILE_ERROR_ACCESS_DENIED);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());
  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
  EXPECT_EQ(response.error().message,
            "Unable to open file: FILE_ERROR_ACCESS_DENIED");
  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 1);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserCrash) {
  parser_factory_->SimulateParseResponseCrash();
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  EXPECT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseBody) {
  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;

  std::string response_body =
      ReadAndFulfillResponseBody(*reader.get(), std::move(*response));
  EXPECT_EQ(response_body, kResponseBody);
}

class SignedWebBundleReaderBaseUrlTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<absl::optional<std::string>> {
 public:
  SignedWebBundleReaderBaseUrlTest() {
    if (GetParam().has_value()) {
      base_url_ = GURL(*GetParam());
    }
  }

 protected:
  absl::optional<GURL> base_url_;
};

TEST_P(SignedWebBundleReaderBaseUrlTest, IsPassedThroughCorrectly) {
  base::test::RepeatingTestFuture<absl::optional<GURL>> on_create_parser_future;
  parser_factory_ = std::make_unique<web_package::MockWebBundleParserFactory>(
      on_create_parser_future.GetCallback());

  base::test::TestFuture<
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(
      parse_error_future.GetCallback(),
      VerificationAction::ContinueAndVerifySignatures(), absl::nullopt,
      base_url_);

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());
  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());

  SimulateAndWaitForParserDisconnect(*reader.get());
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;
  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedWebBundleReaderBaseUrlTest,
                         ::testing::Values(absl::nullopt,
                                           "https://example.com"));

}  // namespace web_app
