// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/fallback_net_fetcher.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "components/update_client/network.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace {

class FakeFetcher : public update_client::NetworkFetcher {
 public:
  FakeFetcher() = delete;
  FakeFetcher(const FakeFetcher&) = delete;
  FakeFetcher& operator=(const FakeFetcher&) = delete;
  ~FakeFetcher() override = default;
  FakeFetcher(
      base::OnceCallback<
          void(update_client::NetworkFetcher::PostRequestCompleteCallback)>
          post_request,
      base::OnceCallback<base::OnceClosure(
          update_client::NetworkFetcher::DownloadToFileCompleteCallback)>
          download_to_file)
      : post_request_(std::move(post_request)),
        download_to_file_(std::move(download_to_file)) {}

  // Overrides for update_client::NetworkFetcher
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback) override {
    std::move(post_request_).Run(std::move(post_request_complete_callback));
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback) override {
    return std::move(download_to_file_)
        .Run(std::move(download_to_file_complete_callback));
  }

 private:
  base::OnceCallback<void(
      update_client::NetworkFetcher::PostRequestCompleteCallback)>
      post_request_;
  base::OnceCallback<base::OnceClosure(
      update_client::NetworkFetcher::DownloadToFileCompleteCallback)>
      download_to_file_;
};

std::unique_ptr<FakeFetcher> MakeFakeFetcherForPost(
    base::OnceCallback<int(void)> error_supplier) {
  return std::make_unique<FakeFetcher>(
      base::BindOnce(
          [](base::OnceCallback<int(void)> error_supplier,
             update_client::NetworkFetcher::PostRequestCompleteCallback
                 callback) {
            std::move(callback).Run(nullptr, std::move(error_supplier).Run(),
                                    {}, {}, 0);
          },
          std::move(error_supplier)),
      base::BindOnce(
          [](update_client::NetworkFetcher::DownloadToFileCompleteCallback
                 callback) {
            std::move(callback).Run(0, 0);
            return base::BindOnce([] {});
          }));
}

std::unique_ptr<FakeFetcher> MakeFakeFetcherForDownload(
    base::OnceCallback<int(void)> error_supplier) {
  return std::make_unique<FakeFetcher>(
      base::BindOnce(
          [](update_client::NetworkFetcher::PostRequestCompleteCallback
                 callback) { std::move(callback).Run(nullptr, 0, {}, {}, 0); }),
      base::BindOnce(
          [](base::OnceCallback<int(void)> error_supplier,
             update_client::NetworkFetcher::DownloadToFileCompleteCallback
                 callback) {
            std::move(callback).Run(std::move(error_supplier).Run(), 0);
            return base::BindOnce([] {});
          },
          std::move(error_supplier)));
}

}  // namespace

TEST(FallbackNetFetcher, NoFallbackOnSuccess_Post) {
  bool ran1 = false;
  bool ran2 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForPost(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 0;
                     })),
                     MakeFakeFetcherForPost(base::BindLambdaForTesting([&] {
                       ran2 = true;
                       return 0;
                     })))
      .PostRequest(
          {}, {}, {}, {}, base::BindRepeating([](int, int64_t) {}),
          base::BindRepeating([](int64_t) {}),
          base::BindLambdaForTesting([&](std::unique_ptr<std::string>, int,
                                         const std::string&, const std::string&,
                                         int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_TRUE(called_back);
}

TEST(FallbackNetFetcher, NoFallbackOnSuccess_Download) {
  bool ran1 = false;
  bool ran2 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForDownload(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 0;
                     })),
                     MakeFakeFetcherForDownload(base::BindLambdaForTesting([&] {
                       ran2 = true;
                       return 0;
                     })))
      .DownloadToFile({}, {}, base::BindRepeating([](int, int64_t) {}),
                      base::BindRepeating([](int64_t) {}),
                      base::BindLambdaForTesting(
                          [&](int, int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_TRUE(called_back);
}

TEST(FallbackNetFetcher, FallbackOnFailure_Post) {
  bool ran1 = false;
  bool ran2 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForPost(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 1;
                     })),
                     MakeFakeFetcherForPost(base::BindLambdaForTesting([&] {
                       ran2 = true;
                       return 0;
                     })))
      .PostRequest(
          {}, {}, {}, {}, base::BindRepeating([](int, int64_t) {}),
          base::BindRepeating([](int64_t) {}),
          base::BindLambdaForTesting([&](std::unique_ptr<std::string>, int,
                                         const std::string&, const std::string&,
                                         int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_TRUE(ran2);
  EXPECT_TRUE(called_back);
}

TEST(FallbackNetFetcher, FallbackOnFailure_Download) {
  bool ran1 = false;
  bool ran2 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForDownload(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 1;
                     })),
                     MakeFakeFetcherForDownload(base::BindLambdaForTesting([&] {
                       ran2 = true;
                       return 0;
                     })))
      .DownloadToFile({}, {}, base::BindRepeating([](int, int64_t) {}),
                      base::BindRepeating([](int64_t) {}),
                      base::BindLambdaForTesting(
                          [&](int, int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_TRUE(ran2);
  EXPECT_TRUE(called_back);
}

TEST(FallbackNetFetcher, NoCrashOnNullptr_Post) {
  bool ran1 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForPost(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 1;
                     })),
                     nullptr)
      .PostRequest(
          {}, {}, {}, {}, base::BindRepeating([](int, int64_t) {}),
          base::BindRepeating([](int64_t) {}),
          base::BindLambdaForTesting([&](std::unique_ptr<std::string>, int,
                                         const std::string&, const std::string&,
                                         int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_TRUE(called_back);
}

TEST(FallbackNetFetcher, NoCrashOnNullptr_Download) {
  bool ran1 = false;
  bool called_back = false;
  FallbackNetFetcher(MakeFakeFetcherForDownload(base::BindLambdaForTesting([&] {
                       ran1 = true;
                       return 1;
                     })),
                     nullptr)
      .DownloadToFile({}, {}, base::BindRepeating([](int, int64_t) {}),
                      base::BindRepeating([](int64_t) {}),
                      base::BindLambdaForTesting(
                          [&](int, int64_t) { called_back = true; }));
  EXPECT_TRUE(ran1);
  EXPECT_TRUE(called_back);
}

}  // namespace updater
