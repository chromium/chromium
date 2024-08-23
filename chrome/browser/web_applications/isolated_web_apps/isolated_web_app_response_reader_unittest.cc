// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_identity_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsTrue;

class IsolatedWebAppResponseReaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IwaIdentityValidator::CreateSingleton();
    SetTrustedWebBundleIdsForTesting({web_bundle_id_});
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath CreateSignedBundleAndWriteToDisk() {
    web_package::WebBundleBuilder builder;
    builder.AddExchange("/",
                        {{":status", "200"}, {"content-type", "text/html"}},
                        "Hello World");
    auto unsigned_bundle = builder.CreateBundle();

    auto signed_bundle = web_package::test::WebBundleSigner::SignBundle(
        unsigned_bundle, test::GetDefaultEd25519KeyPair());

    base::FilePath web_bundle_path;
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path));
    EXPECT_TRUE(base::WriteFile(web_bundle_path, signed_bundle));

    return web_bundle_path;
  }

  base::expected<void, UnusableSwbnFileError> ReadIntegrityBlockAndMetadata(
      SignedWebBundleReader& reader) {
    base::test::TestFuture<base::expected<void, UnusableSwbnFileError>> future;
    reader.ReadIntegrityBlock(base::BindLambdaForTesting(
        [&](base::expected<web_package::SignedWebBundleIntegrityBlock,
                           UnusableSwbnFileError> integrity_block) {
          reader.ProceedWithAction(
              integrity_block.has_value()
                  ? SignedWebBundleReader::SignatureVerificationAction::
                        ContinueAndVerifySignatures()
                  : SignedWebBundleReader::SignatureVerificationAction::Abort(
                        integrity_block.error()),
              future.GetCallback());
        }));
    return future.Take();
  }

  IsolatedWebAppResponseReaderImpl::TrustChecker CreateTrustChecker() {
    return base::BindRepeating(
        &IsolatedWebAppTrustChecker::IsTrusted,
        std::make_unique<IsolatedWebAppTrustChecker>(profile_), web_bundle_id_,
        /*is_dev_mode_bundle=*/false);
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;

  TestingProfile profile_;
  web_package::SignedWebBundleId web_bundle_id_ =
      test::GetDefaultEd25519WebBundleId();

  GURL base_url_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_)
          .origin()
          .GetURL();
};

TEST_F(IsolatedWebAppResponseReaderTest, ChecksWhetherBundleIsStillTrusted) {
  base::FilePath web_bundle_path = CreateSignedBundleAndWriteToDisk();
  auto reader = SignedWebBundleReader::Create(web_bundle_path, base_url_);
  auto status = ReadIntegrityBlockAndMetadata(*reader.get());
  ASSERT_THAT(status, HasValue());

  auto response_reader = std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader), CreateTrustChecker());

  {
    network::ResourceRequest request;
    request.url = base_url_;
    base::test::TestFuture<
        base::expected<IsolatedWebAppResponseReader::Response,
                       IsolatedWebAppResponseReader::Error>>
        response_future;
    response_reader->ReadResponse(request, response_future.GetCallback());
    EXPECT_THAT(response_future.Get(), HasValue());
  }

  SetTrustedWebBundleIdsForTesting({});
  {
    network::ResourceRequest request;
    request.url = base_url_;
    base::test::TestFuture<
        base::expected<IsolatedWebAppResponseReader::Response,
                       IsolatedWebAppResponseReader::Error>>
        response_future;
    response_reader->ReadResponse(request, response_future.GetCallback());
    EXPECT_THAT(
        response_future.Get(),
        ErrorIs(
            Field(&IsolatedWebAppResponseReader::Error::type,
                  Eq(IsolatedWebAppResponseReader::Error::Type::kNotTrusted))));
  }
}

// Tests that query parameters and fragment are stripped from requests before
// looking up the corresponding resources inside of the bundle.
TEST_F(IsolatedWebAppResponseReaderTest,
       ReadResponseStripsQueryParametersAndFragment) {
  base::FilePath web_bundle_path = CreateSignedBundleAndWriteToDisk();
  auto reader = SignedWebBundleReader::Create(web_bundle_path, base_url_);
  auto status = ReadIntegrityBlockAndMetadata(*reader.get());
  ASSERT_THAT(status, HasValue());

  auto response_reader = std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader), CreateTrustChecker());

  {
    network::ResourceRequest request;
    request.url = base_url_;
    base::test::TestFuture<
        base::expected<IsolatedWebAppResponseReader::Response,
                       IsolatedWebAppResponseReader::Error>>
        response_future;
    response_reader->ReadResponse(request, response_future.GetCallback());
    EXPECT_THAT(response_future.Get(), HasValue());
  }

  {
    network::ResourceRequest request;
    request.url = base_url_.Resolve("/?some-query-parameter#some-fragment");
    base::test::TestFuture<
        base::expected<IsolatedWebAppResponseReader::Response,
                       IsolatedWebAppResponseReader::Error>>
        response_future;
    response_reader->ReadResponse(request, response_future.GetCallback());
    EXPECT_THAT(response_future.Get(), HasValue());
  }
}

TEST_F(IsolatedWebAppResponseReaderTest, ReadResponseBody) {
  base::FilePath web_bundle_path = CreateSignedBundleAndWriteToDisk();
  auto reader = SignedWebBundleReader::Create(web_bundle_path, base_url_);
  auto status = ReadIntegrityBlockAndMetadata(*reader.get());
  ASSERT_THAT(status, HasValue());

  auto response_reader = std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader), CreateTrustChecker());

  network::ResourceRequest request;
  request.url = base_url_;
  base::test::TestFuture<base::expected<IsolatedWebAppResponseReader::Response,
                                        IsolatedWebAppResponseReader::Error>>
      response_future;
  response_reader->ReadResponse(request, response_future.GetCallback());
  ASSERT_THAT(response_future.Get(), HasValue());

  IsolatedWebAppResponseReader::Response response = *response_future.Take();
  EXPECT_THAT(response.head()->response_code, Eq(200));

  {
    std::string response_content = ReadAndFulfillResponseBody(
        response.head()->payload_length,
        base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                       base::Unretained(&response)));
    EXPECT_THAT(response_content, Eq("Hello World"));
  }

  // If the response_reader is deleted, then reading the response should return
  // `net::ERR_FAILED`.
  response_reader.reset();
  {
    base::test::TestFuture<net::Error> response_body_future;
    ReadResponseBody(
        response.head()->payload_length,
        base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                       base::Unretained(&response)),
        response_body_future.GetCallback());
    EXPECT_THAT(response_body_future.Get(), Eq(net::ERR_FAILED));
  }
}

TEST_F(IsolatedWebAppResponseReaderTest, Close) {
  base::FilePath web_bundle_path = CreateSignedBundleAndWriteToDisk();
  auto reader = SignedWebBundleReader::Create(web_bundle_path, base_url_);
  // TODO: handle return value.
  auto status = ReadIntegrityBlockAndMetadata(*reader.get());
  ASSERT_THAT(status, HasValue());
  auto* raw_reader = reader.get();

  auto response_reader = std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader), CreateTrustChecker());

  network::ResourceRequest request;
  request.url = base_url_;
  base::test::TestFuture<base::expected<IsolatedWebAppResponseReader::Response,
                                        IsolatedWebAppResponseReader::Error>>
      response_future;
  response_reader->ReadResponse(request, response_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppResponseReader::Response response,
                       response_future.Take());

  base::test::TestFuture<void> close_future;
  response_reader->Close(close_future.GetCallback());
  ASSERT_TRUE(close_future.Wait());

  EXPECT_EQ(raw_reader->GetState(), SignedWebBundleReader::State::kClosed);

  // If the response_reader is closed, then reading the response should return
  // `net::ERR_FAILED`.
  base::test::TestFuture<net::Error> response_body_future;
  ReadResponseBody(
      response.head()->payload_length,
      base::BindOnce(&IsolatedWebAppResponseReader::Response::ReadBody,
                     base::Unretained(&response)),
      response_body_future.GetCallback());
  EXPECT_THAT(response_body_future.Get(), Eq(net::ERR_FAILED));
}

}  // namespace
}  // namespace web_app
