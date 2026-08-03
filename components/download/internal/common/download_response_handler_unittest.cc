// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_response_handler.h"

#include <memory>
#include <utility>

#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace download {
namespace {

class FakeDelegate : public DownloadResponseHandler::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() = default;

  // DownloadResponseHandler::Delegate:
  void OnResponseStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      mojom::DownloadStreamHandlePtr stream_handle) override {
    create_info_ = std::move(download_create_info);
  }
  void OnReceiveRedirect() override { ++redirect_count_; }
  void OnResponseCompleted() override { response_completed_ = true; }
  bool CanRequestURL(const GURL& url) override { return true; }
  void OnUploadProgress(uint64_t bytes_uploaded) override {}

  const DownloadCreateInfo* create_info() const { return create_info_.get(); }
  int redirect_count() const { return redirect_count_; }
  bool response_completed() const { return response_completed_; }

 private:
  std::unique_ptr<DownloadCreateInfo> create_info_;
  int redirect_count_ = 0;
  bool response_completed_ = false;
};

class DownloadResponseHandlerTest : public testing::Test {
 protected:
  std::unique_ptr<DownloadResponseHandler> CreateHandler(
      const GURL& request_url,
      network::mojom::RedirectMode cross_origin_redirects) {
    resource_request_.url = request_url;
    resource_request_.method = "GET";
    return std::make_unique<DownloadResponseHandler>(
        &resource_request_, &delegate_, std::make_unique<DownloadSaveInfo>(),
        /*is_parallel_request=*/false,
        /*is_transient=*/false,
        /*fetch_error_body=*/false, cross_origin_redirects,
        DownloadUrlParameters::RequestHeadersType(),
        /*request_origin=*/std::string(), DownloadSource::UNKNOWN,
        /*require_safety_checks=*/true, std::vector<GURL>(1, request_url),
        /*is_background_mode=*/false);
  }

  static net::RedirectInfo MakeRedirectInfo(const GURL& new_url) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = new_url;
    return redirect_info;
  }

  network::ResourceRequest resource_request_;
  FakeDelegate delegate_;
};

TEST_F(DownloadResponseHandlerTest, HttpRequestFollowsRedirect) {
  const GURL kRequestUrl("https://a.example.com/file");
  const GURL kRedirectUrl("https://b.example.com/file");
  auto handler =
      CreateHandler(kRequestUrl, network::mojom::RedirectMode::kFollow);

  handler->OnReceiveRedirect(MakeRedirectInfo(kRedirectUrl),
                             network::mojom::URLResponseHead::New());

  EXPECT_EQ(1, delegate_.redirect_count());
  EXPECT_FALSE(delegate_.response_completed());
  EXPECT_FALSE(delegate_.create_info());
}

// Loading a blob: URL never produces an HTTP redirect, so a redirect reported
// while the current request URL is blob: must not be followed and must not be
// appended to the download's URL chain.
TEST_F(DownloadResponseHandlerTest, BlobRequestRejectsRedirect) {
  const GURL kBlobUrl("blob:https://a.example.com/1234");
  const GURL kRedirectUrl("https://b.example.com/payload");
  auto handler = CreateHandler(kBlobUrl, network::mojom::RedirectMode::kFollow);

  handler->OnReceiveRedirect(MakeRedirectInfo(kRedirectUrl),
                             network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, delegate_.redirect_count());
  EXPECT_TRUE(delegate_.response_completed());
  ASSERT_TRUE(delegate_.create_info());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            delegate_.create_info()->result);
  ASSERT_EQ(1u, delegate_.create_info()->url_chain.size());
  EXPECT_EQ(kBlobUrl, delegate_.create_info()->url_chain.back());
}

// Same as above with the manual redirect mode: the redirect target must not be
// surfaced to the delegate as an interrupted cross-origin redirect either.
TEST_F(DownloadResponseHandlerTest, BlobRequestRejectsManualRedirect) {
  const GURL kBlobUrl("blob:https://a.example.com/1234");
  const GURL kRedirectUrl("https://b.example.com/payload");
  auto handler = CreateHandler(kBlobUrl, network::mojom::RedirectMode::kManual);

  handler->OnReceiveRedirect(MakeRedirectInfo(kRedirectUrl),
                             network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, delegate_.redirect_count());
  ASSERT_TRUE(delegate_.create_info());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            delegate_.create_info()->result);
  ASSERT_EQ(1u, delegate_.create_info()->url_chain.size());
  EXPECT_EQ(kBlobUrl, delegate_.create_info()->url_chain.back());
}

TEST_F(DownloadResponseHandlerTest, DataRequestRejectsRedirect) {
  const GURL kDataUrl("data:text/plain,hello");
  const GURL kRedirectUrl("https://b.example.com/payload");
  auto handler = CreateHandler(kDataUrl, network::mojom::RedirectMode::kFollow);

  handler->OnReceiveRedirect(MakeRedirectInfo(kRedirectUrl),
                             network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, delegate_.redirect_count());
  EXPECT_TRUE(delegate_.response_completed());
  ASSERT_TRUE(delegate_.create_info());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            delegate_.create_info()->result);
  ASSERT_EQ(1u, delegate_.create_info()->url_chain.size());
  EXPECT_EQ(kDataUrl, delegate_.create_info()->url_chain.back());
}

TEST_F(DownloadResponseHandlerTest, FileRequestRejectsRedirect) {
  const GURL kFileUrl("file:///path/to/test.file");
  const GURL kRedirectUrl("https://b.example.com/payload");
  auto handler = CreateHandler(kFileUrl, network::mojom::RedirectMode::kFollow);

  handler->OnReceiveRedirect(MakeRedirectInfo(kRedirectUrl),
                             network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, delegate_.redirect_count());
  EXPECT_TRUE(delegate_.response_completed());
  ASSERT_TRUE(delegate_.create_info());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            delegate_.create_info()->result);
  ASSERT_EQ(1u, delegate_.create_info()->url_chain.size());
  EXPECT_EQ(kFileUrl, delegate_.create_info()->url_chain.back());
}

}  // namespace
}  // namespace download
