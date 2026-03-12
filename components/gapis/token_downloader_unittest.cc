// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gapis/token_downloader.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/gapis/proto/obtain_token.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gapis {

namespace {

constexpr char kTestURL[] = "http://chromium.org/gapis";
constexpr char kObtainTokenURL[] = "http://chromium.org/gapis/token";
constexpr char kToken[] = "test_token";
constexpr char kAccessToken[] = "test_access_token";
constexpr char kSignedChallenge[] = "test_signed_challenge";

}  // namespace

class TokenDownloaderTest : public testing::Test {
 public:
  TokenDownloaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(kEnableGapis);
  }

  ~TokenDownloaderTest() override = default;

  network::TestURLLoaderFactory* url_loader_factory() {
    return &test_url_loader_factory_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(TokenDownloaderTest, DownloadTokenSuccess) {
  TokenDownloader downloader(GURL(kTestURL), GetSharedURLLoaderFactory());

  base::RunLoop run_loop;
  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run(kToken))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  downloader.FetchToken(callback.Get(), kAccessToken, kSignedChallenge);

  gapis_pb::ObtainTokenResponse response;
  response.set_token(kToken);
  ASSERT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      kObtainTokenURL, response.SerializeAsString()));

  run_loop.Run();
}

TEST_F(TokenDownloaderTest, DownloadTokenFailure) {
  TokenDownloader downloader(GURL(kTestURL), GetSharedURLLoaderFactory());

  base::RunLoop run_loop;
  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run(""))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  downloader.FetchToken(callback.Get(), kAccessToken, kSignedChallenge);

  ASSERT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      kObtainTokenURL, "", net::HTTP_NOT_FOUND));

  run_loop.Run();
}

TEST_F(TokenDownloaderTest, DownloadTokenTimeout) {
  TokenDownloader downloader(GURL(kTestURL), GetSharedURLLoaderFactory());

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run(""));

  downloader.FetchToken(callback.Get(), kAccessToken, kSignedChallenge);

  EXPECT_EQ(1, url_loader_factory()->NumPending());

  // Fast forward time to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(11));
}

}  // namespace gapis
