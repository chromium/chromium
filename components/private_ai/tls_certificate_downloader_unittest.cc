// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_certificate_downloader.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_ai {
namespace {

constexpr char kTcaPem[] = "-----BEGIN CERTIFICATE-----\ntca\n";
constexpr char kOakCtPem[] = "-----BEGIN CERTIFICATE-----\noakct\n";

using PemBundle = TlsCertificateDownloader::PemBundle;

// A NetworkContext that hands out URLLoaderFactory pipes backed by a
// TestURLLoaderFactory.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  explicit FakeNetworkContext(network::TestURLLoaderFactory* factory)
      : factory_(factory) {}

  int create_factory_count() const { return create_factory_count_; }

  // network::TestNetworkContext:
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override {
    ++create_factory_count_;
    factory_->Clone(std::move(receiver));
  }

 private:
  raw_ptr<network::TestURLLoaderFactory> factory_;
  int create_factory_count_ = 0;
};

class TlsCertificateDownloaderTest : public testing::Test {
 protected:
  // Creates a downloader that uses `test_url_loader_factory_` directly.
  std::unique_ptr<TlsCertificateDownloader> CreateDownloader() {
    auto downloader = std::make_unique<TlsCertificateDownloader>(
        tca_url_, oakct_url_, network::NetworkContextGetter());
    downloader->SetURLLoaderFactoryForTesting(&test_url_loader_factory_);
    return downloader;
  }

  void AddValidResponses() {
    test_url_loader_factory_.AddResponse(tca_url_.spec(), kTcaPem);
    test_url_loader_factory_.AddResponse(oakct_url_.spec(), kOakCtPem);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  const GURL tca_url_{"https://www.gstatic.com/tca.pem"};
  const GURL oakct_url_{"https://www.gstatic.com/oakct.pem"};
};

TEST_F(TlsCertificateDownloaderTest, DownloadsBothCertificates) {
  AddValidResponses();
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->tca_pem, kTcaPem);
  EXPECT_EQ(result->oakct_pem, kOakCtPem);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 2u);
}

TEST_F(TlsCertificateDownloaderTest, CreatesFactoryFromNetworkContext) {
  AddValidResponses();
  FakeNetworkContext network_context(&test_url_loader_factory_);
  TlsCertificateDownloader downloader(
      tca_url_, oakct_url_,
      base::BindRepeating(
          [](network::mojom::NetworkContext* context) { return context; },
          base::Unretained(&network_context)));

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader.Download(future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(network_context.create_factory_count(), 1);

  // The factory is reused for subsequent downloads.
  base::test::TestFuture<std::optional<PemBundle>> future2;
  downloader.Download(future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());
  EXPECT_EQ(network_context.create_factory_count(), 1);
}

TEST_F(TlsCertificateDownloaderTest, NoNetworkContextFailsAsynchronously) {
  TlsCertificateDownloader downloader(
      tca_url_, oakct_url_,
      base::BindRepeating(
          []() -> network::mojom::NetworkContext* { return nullptr; }));

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader.Download(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(TlsCertificateDownloaderTest, TcaDownloadFailsReturnsNullopt) {
  test_url_loader_factory_.AddResponse(tca_url_.spec(), "",
                                       net::HTTP_NOT_FOUND);
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  // The other download is cancelled.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TlsCertificateDownloaderTest, OakCtDownloadFailsReturnsNullopt) {
  test_url_loader_factory_.AddResponse(oakct_url_.spec(), "",
                                       net::HTTP_NOT_FOUND);
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TlsCertificateDownloaderTest, OversizedResponseReturnsNullopt) {
  test_url_loader_factory_.AddResponse(tca_url_.spec(),
                                       std::string(64 * 1024 + 1, 'a'));
  test_url_loader_factory_.AddResponse(oakct_url_.spec(), kOakCtPem);
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(TlsCertificateDownloaderTest, CanDownloadAgainAfterFailure) {
  test_url_loader_factory_.AddResponse(tca_url_.spec(), "",
                                       net::HTTP_NOT_FOUND);
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future1;
  downloader->Download(future1.GetCallback());
  EXPECT_FALSE(future1.Get().has_value());

  AddValidResponses();
  base::test::TestFuture<std::optional<PemBundle>> future2;
  downloader->Download(future2.GetCallback());
  auto result = future2.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->tca_pem, kTcaPem);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 4u);
}

TEST_F(TlsCertificateDownloaderTest, ReentrantDownloadFromCallback) {
  AddValidResponses();
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(base::BindOnce(
      [](TlsCertificateDownloader* downloader,
         base::test::TestFuture<std::optional<PemBundle>>* future,
         std::optional<PemBundle> bundle) {
        downloader->Download(future->GetCallback());
      },
      downloader.get(), &future));

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 4u);
}

TEST_F(TlsCertificateDownloaderTest, CoalescesConcurrentDownloads) {
  AddValidResponses();
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future1;
  base::test::TestFuture<std::optional<PemBundle>> future2;
  downloader->Download(future1.GetCallback());
  downloader->Download(future2.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  auto result1 = future1.Get();
  auto result2 = future2.Get();
  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result1->tca_pem, kTcaPem);
  EXPECT_EQ(result1->oakct_pem, kOakCtPem);
  EXPECT_EQ(result2->tca_pem, kTcaPem);
  EXPECT_EQ(result2->oakct_pem, kOakCtPem);

  // Only one pair of network requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 2u);
}

TEST_F(TlsCertificateDownloaderTest, ConcurrentDownloadsFailTogether) {
  test_url_loader_factory_.AddResponse(tca_url_.spec(), "",
                                       net::HTTP_NOT_FOUND);
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future1;
  base::test::TestFuture<std::optional<PemBundle>> future2;
  downloader->Download(future1.GetCallback());
  downloader->Download(future2.GetCallback());

  EXPECT_FALSE(future1.Get().has_value());
  EXPECT_FALSE(future2.Get().has_value());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TlsCertificateDownloaderTest, LoadFlagsAlwaysBypassHttpCache) {
  auto downloader = CreateDownloader();

  base::test::TestFuture<std::optional<PemBundle>> future;
  downloader->Download(future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 2);
  for (const auto& pending : *test_url_loader_factory_.pending_requests()) {
    EXPECT_TRUE(pending.request.load_flags & net::LOAD_BYPASS_CACHE);
    EXPECT_TRUE(pending.request.load_flags & net::LOAD_DISABLE_CACHE);
    EXPECT_EQ(pending.request.credentials_mode,
              network::mojom::CredentialsMode::kOmit);
  }
}

}  // namespace
}  // namespace private_ai
