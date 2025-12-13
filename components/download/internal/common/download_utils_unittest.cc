// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace download {
namespace {

TEST(DownloadUtilsTest, HandleServerResponse200) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            HandleSuccessfulServerResponse(*headers.get(), nullptr,
                                           /*fetch_error_body=*/false));
}

TEST(DownloadUtilsTest, HandleServerResponse200_RangeRequest) {
  // Arbitrary range request must expect HTTP 206 as a successful response.
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  DownloadSaveInfo save_info;
  save_info.range_request_to = 100;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            HandleSuccessfulServerResponse(*headers.get(), &save_info,
                                           /*fetch_error_body=*/false));
}

TEST(DownloadUtilsTest, HandleServerResponse206_RangeRequest) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 206 Partial Content"));
  headers->AddHeader("Content-Range", "bytes 105-125/500");
  DownloadSaveInfo save_info;
  save_info.range_request_from = 100;
  EXPECT_TRUE(save_info.IsArbitraryRangeRequest());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            HandleSuccessfulServerResponse(*headers.get(), &save_info,
                                           /*fetch_error_body=*/false));
}

void VerifyRangeHeader(DownloadUrlParameters* params,
                       const std::string& expected_range_header) {
  auto resource_request = CreateResourceRequest(params);
  EXPECT_EQ(expected_range_header, resource_request->headers.GetHeader(
                                       net::HttpRequestHeaders::kRange));
  ASSERT_FALSE(
      resource_request->headers.HasHeader(net::HttpRequestHeaders::kIfRange));
}

TEST(DownloadUtilsTest, CreateResourceRequest) {
  auto params = std::make_unique<DownloadUrlParameters>(
      GURL(), TRAFFIC_ANNOTATION_FOR_TESTS);
  params->set_use_if_range(false);
  params->set_range_request_offset(100, kInvalidRange);
  VerifyRangeHeader(params.get(), "bytes=100-");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=105-");

  params->set_offset(0);
  params->set_range_request_offset(100, 200);
  VerifyRangeHeader(params.get(), "bytes=100-200");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=105-200");

  params->set_offset(0);
  params->set_range_request_offset(kInvalidRange, 200);
  VerifyRangeHeader(params.get(), "bytes=-200");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=-195");
}

TEST(DownloadUtilsTest, IsContentDispositionAttachmentInHead) {
  network::mojom::URLResponseHead response_head;
  EXPECT_FALSE(IsContentDispositionAttachmentInHead(response_head));

  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200 OK");
  response_head.headers = builder.Build();
  EXPECT_FALSE(IsContentDispositionAttachmentInHead(response_head));

  builder.AddHeader("Content-Disposition", "attachment");
  response_head.headers = builder.Build();
  EXPECT_TRUE(IsContentDispositionAttachmentInHead(response_head));
}

TEST(DownloadUtilsTest, CreateResourceRequestWithPermissionsPolicy) {
  // TODO(crbug.com/382291442): Remove feature enablement once feature launched.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPopulatePermissionsPolicyOnRequest);

  auto params = std::make_unique<DownloadUrlParameters>(
      GURL(), TRAFFIC_ANNOTATION_FOR_TESTS);
  url::Origin origin = url::Origin::Create(GURL("https://a.test"));
  const std::unique_ptr<network::PermissionsPolicy> permissions_policy =
      network::PermissionsPolicy::CreateFromParsedPolicy(
          {{{network::mojom::PermissionsPolicyFeature::kStorageAccessAPI,
             /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(origin)},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false}}},
          std::nullopt, origin);
  params->set_permissions_policy(permissions_policy.get());
  auto resource_request = CreateResourceRequest(params.get());
  EXPECT_EQ(*resource_request->permissions_policy, *permissions_policy);
}

// TODO(crbug.com/382291442): Remove test once feature launched.
TEST(DownloadUtilsTest,
     CreateResourceRequestWithPermissionsPolicy_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      network::features::kPopulatePermissionsPolicyOnRequest);

  auto params = std::make_unique<DownloadUrlParameters>(
      GURL(), TRAFFIC_ANNOTATION_FOR_TESTS);
  url::Origin origin = url::Origin::Create(GURL("https://a.test"));
  const std::unique_ptr<network::PermissionsPolicy> permissions_policy =
      network::PermissionsPolicy::CreateFromParsedPolicy(
          {{{network::mojom::PermissionsPolicyFeature::kStorageAccessAPI,
             /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(origin)},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false}}},
          std::nullopt, origin);
  params->set_permissions_policy(permissions_policy.get());
  auto resource_request = CreateResourceRequest(params.get());
  EXPECT_EQ(resource_request->permissions_policy, std::nullopt);
}

}  // namespace
}  // namespace download
