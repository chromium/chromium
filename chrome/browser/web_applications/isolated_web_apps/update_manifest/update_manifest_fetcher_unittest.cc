// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;

constexpr std::string_view kValidManifestUrl =
    "https://example.com/valid_update_manifest.json";

constexpr std::string_view kInvalidManifestUrl =
    "https://example.com/invalid_update_manifest.json";

constexpr std::string_view kManifestWithoutVersionsUrl =
    "https://example.com/update_manifest_without_versions.json";

constexpr std::string_view kInvalidJsonUrl =
    "https://example.com/invalid_json.json";

constexpr std::string_view k404Url = "https://example.com/404.json";

class UpdateManifestFetcherTest : public ::testing::Test {
 public:
  UpdateManifestFetcherTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

  void SetUp() override {
    AddJsonResponse(kValidManifestUrl, R"(
      {
        "versions": [
          { "src": "https://other.com/bundle.swbn", "version": "1.2.3" },
          { "src": "/foo/bundle.swbn", "version": "3.2.1" }
        ]
      }
    )");
    AddJsonResponse(kManifestWithoutVersionsUrl, R"(
      {
        "versions": []
      }
    )");
    AddJsonResponse(kInvalidManifestUrl, R"(
      { "versions": 123 }
    )");
    AddJsonResponse(kInvalidJsonUrl, R"(
      invalid json
    )");
    test_factory_.AddResponse(std::string(k404Url), "",
                              net::HttpStatusCode::HTTP_NOT_FOUND);
  }

 protected:
  void AddJsonResponse(std::string_view url, std::string content) {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status;
    test_factory_.AddResponse(GURL(url), std::move(head), std::move(content),
                              status);
  }

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(UpdateManifestFetcherTest, FetchesValidManifest) {
  auto fetcher = UpdateManifestFetcher(GURL(kValidManifestUrl),
                                       PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                       shared_url_loader_factory_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(
      update_manifest->versions(),
      ElementsAre(
          UpdateManifest::VersionEntry{GURL("https://other.com/bundle.swbn"),
                                       base::Version("1.2.3"),
                                       {*UpdateChannel::Create("default")}},
          UpdateManifest::VersionEntry{
              GURL("https://example.com/foo/bundle.swbn"),
              base::Version("3.2.1"),
              {*UpdateChannel::Create("default")}}));
}

TEST_F(UpdateManifestFetcherTest, SucceedsWhenManifestHasNoVersions) {
  auto fetcher = UpdateManifestFetcher(GURL(kManifestWithoutVersionsUrl),
                                       PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                       shared_url_loader_factory_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto update_manifest, future.Take());

  EXPECT_THAT(update_manifest.versions(), IsEmpty());
}

TEST_F(UpdateManifestFetcherTest, FailsWhenManifestIsInvalid) {
  auto fetcher = UpdateManifestFetcher(GURL(kInvalidManifestUrl),
                                       PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                       shared_url_loader_factory_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifestFetcher::Error::kInvalidManifest));
}

TEST_F(UpdateManifestFetcherTest, FailsWhenJsonIsInvalid) {
  auto fetcher = UpdateManifestFetcher(GURL(kInvalidJsonUrl),
                                       PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                       shared_url_loader_factory_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifestFetcher::Error::kInvalidJson));
}

TEST_F(UpdateManifestFetcherTest, FailedDownload) {
  auto fetcher =
      UpdateManifestFetcher(GURL(k404Url), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifestFetcher::Error::kDownloadFailed));
}

}  // namespace
}  // namespace web_app
