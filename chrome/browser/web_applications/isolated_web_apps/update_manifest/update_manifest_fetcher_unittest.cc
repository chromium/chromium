// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Property;

constexpr std::string_view kValidManifestUrl =
    "https://example.com/valid_update_manifest.json";

constexpr std::string_view kInvalidManifestUrl =
    "https://example.com/invalid_update_manifest.json";

constexpr std::string_view kManifestWithoutVersionsUrl =
    "https://example.com/update_manifest_without_versions.json";

constexpr std::string_view kInvalidJsonUrl =
    "https://example.com/invalid_json.json";

constexpr std::string_view k404Url = "https://example.com/404.json";

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override {
    mojo::Remote<network::mojom::ResolveHostClient> client(
        std::move(response_client));
    client->OnComplete(net::OK, net::ResolveErrorInfo(net::OK),
                       resolved_addresses_,
                       /*alternative_endpoints=*/{});
  }

  void set_resolved_addresses(net::AddressList addresses) {
    resolved_addresses_ = std::move(addresses);
  }

 private:
  net::AddressList resolved_addresses_{
      net::IPEndPoint(net::IPAddress(8, 8, 8, 8), 80)};
};

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
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  FakeNetworkContext fake_network_context_;
};

TEST_F(UpdateManifestFetcherTest, FetchesValidManifest) {
  auto fetcher = UpdateManifestFetcher(
      GURL(kValidManifestUrl), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  EXPECT_THAT(
      update_manifest,
      ValueIs(Property("versions", &UpdateManifest::versions,
                       ElementsAre(
                           UpdateManifest::VersionEntry{
                               GURL("https://other.com/bundle.swbn"),
                               *IwaVersion::Create("1.2.3"),
                               {*UpdateChannel::Create("default")}},
                           UpdateManifest::VersionEntry{
                               GURL("https://example.com/foo/bundle.swbn"),
                               *IwaVersion::Create("3.2.1"),
                               {*UpdateChannel::Create("default")}}))));
}

TEST_F(UpdateManifestFetcherTest, SucceedsWhenManifestHasNoVersions) {
  auto fetcher = UpdateManifestFetcher(
      GURL(kManifestWithoutVersionsUrl), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto update_manifest, future.Take());

  EXPECT_THAT(update_manifest.versions(), IsEmpty());
}

TEST_F(UpdateManifestFetcherTest, FailsWhenManifestIsInvalid) {
  auto fetcher = UpdateManifestFetcher(
      GURL(kInvalidManifestUrl), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  EXPECT_THAT(update_manifest,
              ErrorIs(UpdateManifestFetcher::Error::kInvalidManifest));
}

TEST_F(UpdateManifestFetcherTest, FailsWhenJsonIsInvalid) {
  auto fetcher = UpdateManifestFetcher(
      GURL(kInvalidJsonUrl), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  EXPECT_THAT(update_manifest,
              ErrorIs(UpdateManifestFetcher::Error::kInvalidJson));
}

TEST_F(UpdateManifestFetcherTest, FailedDownload) {
  auto fetcher =
      UpdateManifestFetcher(GURL(k404Url), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  auto update_manifest = future.Take();

  EXPECT_THAT(update_manifest,
              ErrorIs(UpdateManifestFetcher::Error::kDownloadFailed));
}

TEST_F(UpdateManifestFetcherTest, SetsCorrectClientSecurityState) {
  GURL unknown_url("https://other-example.com/manifest.json");
  auto fetcher =
      UpdateManifestFetcher(unknown_url, PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kPublic);
  EXPECT_TRUE(
      request.trusted_params->client_security_state->is_web_secure_context);
  EXPECT_EQ(request.trusted_params->client_security_state
                ->local_network_access_request_policy,
            network::mojom::LocalNetworkAccessRequestPolicy::kBlock);
}

TEST_F(UpdateManifestFetcherTest, SetsCorrectClientSecurityStateForIpLiteral) {
  GURL ip_url("http://127.0.0.1/manifest.json");
  // No response added for this URL.

  auto fetcher =
      UpdateManifestFetcher(ip_url, PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kLoopback);
}

TEST_F(UpdateManifestFetcherTest,
       SetsCorrectClientSecurityStateForMultipleAddresses) {
  GURL mixed_url("https://mixed.com/manifest.json");
  // One public, one private. Public should win.
  fake_network_context_.set_resolved_addresses(net::AddressList({
      net::IPEndPoint(net::IPAddress(192, 168, 0, 1), 80),
      net::IPEndPoint(net::IPAddress(8, 8, 8, 8), 80),
  }));

  auto fetcher =
      UpdateManifestFetcher(mixed_url, PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  // kPublic should be chosen over kLocal (private).
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kPublic);
}

TEST_F(UpdateManifestFetcherTest,
       SetsCorrectClientSecurityStateForLocalAndLoopback) {
  GURL mixed_url("https://mixed.com/manifest.json");
  // One local, one loopback. Local should win (it's more public).
  fake_network_context_.set_resolved_addresses(net::AddressList({
      net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 80),
      net::IPEndPoint(net::IPAddress(192, 168, 0, 1), 80),
  }));

  auto fetcher =
      UpdateManifestFetcher(mixed_url, PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                            shared_url_loader_factory_, &fake_network_context_);

  base::test::TestFuture<
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>>
      future;
  fetcher.FetchUpdateManifest(future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  // kLocal should be chosen over kLoopback.
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kLocal);
}

TEST_F(UpdateManifestFetcherTest, ErrorToString) {
  EXPECT_EQ(UpdateManifestFetcher::ErrorToString(
                UpdateManifestFetcher::Error::kDownloadFailed),
            "Failed to download update manifest");
  EXPECT_EQ(UpdateManifestFetcher::ErrorToString(
                UpdateManifestFetcher::Error::kInvalidJson),
            "Update manifest contains invalid JSON");
  EXPECT_EQ(UpdateManifestFetcher::ErrorToString(
                UpdateManifestFetcher::Error::kInvalidManifest),
            "Invalid update manifest format");
}

}  // namespace
}  // namespace web_app
