// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client_errors.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {

namespace {

constexpr char kUrl[] = "http://localhost/download/test.crx";
constexpr char kHash[] =
    "6fc4b93fd11134de1300c2c0bb88c12b644a4ec0fd7c9b12cb7cc067667bde87";

// A network fetcher that never completes on its own and reports completion
// with net::ERR_ABORTED once it is cancelled, as the NetworkFetcher contract
// requires.
class CompletingOnCancelNetworkFetcher : public NetworkFetcher {
 public:
  explicit CompletingOnCancelNetworkFetcher(base::OnceClosure on_start)
      : on_start_(std::move(on_start)) {}
  ~CompletingOnCancelNetworkFetcher() override = default;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    NOTREACHED();
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    complete_callback_ = std::move(download_to_file_complete_callback);
    std::move(on_start_).Run();
    // The fetcher must outlive the fetch: the callback is bound unretained.
    return base::BindOnce(&CompletingOnCancelNetworkFetcher::Cancel,
                          base::Unretained(this));
  }

 private:
  void Cancel() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(complete_callback_),
                                  net::ERR_ABORTED, /*content_size=*/-1));
  }

  base::OnceClosure on_start_;
  DownloadToFileCompleteCallback complete_callback_;
};

class CompletingOnCancelNetworkFetcherFactory : public NetworkFetcherFactory {
 public:
  explicit CompletingOnCancelNetworkFetcherFactory(
      base::RepeatingClosure on_start)
      : on_start_(std::move(on_start)) {}

  std::unique_ptr<NetworkFetcher> Create() const override {
    return std::make_unique<CompletingOnCancelNetworkFetcher>(on_start_);
  }

 private:
  ~CompletingOnCancelNetworkFetcherFactory() override = default;

  base::RepeatingClosure on_start_;
};

}  // namespace

class UrlFetcherDownloaderTest : public testing::Test {
 protected:
  UrlFetcherDownloaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  scoped_refptr<UrlFetcherDownloader> MakeDownloader() {
    return base::MakeRefCounted<UrlFetcherDownloader>(
        /*successor=*/nullptr,
        base::MakeRefCounted<NetworkFetcherChromiumFactory>(
            shared_url_loader_factory_,
            base::BindRepeating([](const GURL& url) { return false; })),
        "UrlFetcherDownloaderTest");
  }

  // Returns a download callback that records the result and quits `run_loop`.
  CrxDownloader::DownloadCallback RecordingCallback(base::RunLoop& run_loop) {
    return base::BindLambdaForTesting([&](const CrxDownloader::Result& result) {
      ++num_complete_calls_;
      result_ = result;
      run_loop.Quit();
    });
  }

  // Drains the pending tasks so that a second completion, if one were posted,
  // would be observed by the count below. There is no condition to wait for
  // here: the expectation is that nothing further happens.
  void ExpectNoFurtherCompletion() {
    const int completions = num_complete_calls_;
    // A completion crosses at most two hops after leaving the thread pool:
    // the network fetcher reports to the downloader, which posts the result
    // to the main thread. Draining twice covers both.
    for (int i = 0; i < 2; ++i) {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      // The sentinel runs after everything queued ahead of it on this
      // sequence, so the loop quits once those tasks have run.
      base::RunLoop drain_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, drain_loop.QuitClosure());
      drain_loop.Run();
    }
    EXPECT_EQ(completions, num_complete_calls_);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  int num_complete_calls_ = 0;
  CrxDownloader::Result result_;
};

// Cancelling before the download directory has been created reports the
// cancellation once, when the directory becomes ready.
TEST_F(UrlFetcherDownloaderTest, CancelBeforeDownloadDirCreated) {
  auto downloader = MakeDownloader();
  base::RunLoop run_loop;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL(kUrl), kHash, RecordingCallback(run_loop));

  std::move(cancel).Run();
  run_loop.Run();

  EXPECT_EQ(1, num_complete_calls_);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED), result_.error);
  // The download directory, created after the cancellation, is not leaked.
  ASSERT_FALSE(downloader->download_dir_.empty());
  // The deletion runs on the thread pool and reports nothing back.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::DirectoryExists(downloader->download_dir_));
}

// Cancelling while the network fetch is in flight reports the cancellation
// once and stops the fetch.
TEST_F(UrlFetcherDownloaderTest, CancelWhileFetchInFlight) {
  auto downloader = MakeDownloader();
  base::RunLoop request_loop;
  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest&) { request_loop.Quit(); }));

  base::RunLoop run_loop;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL(kUrl), kHash, RecordingCallback(run_loop));
  // Wait for the fetch to start; the response is never provided.
  request_loop.Run();
  ASSERT_EQ(0, num_complete_calls_);

  std::move(cancel).Run();
  run_loop.Run();
  ExpectNoFurtherCompletion();

  EXPECT_EQ(1, num_complete_calls_);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED), result_.error);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  // The cancellation is recorded like any other completion.
  ASSERT_EQ(1u, downloader->download_metrics().size());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            downloader->download_metrics()[0].error);
}

// Cancelling after the download has completed does nothing.
TEST_F(UrlFetcherDownloaderTest, CancelAfterCompletionIsNoOp) {
  auto downloader = MakeDownloader();
  test_url_loader_factory_.AddResponse(kUrl, "not a crx");

  base::RunLoop run_loop;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL(kUrl), kHash, RecordingCallback(run_loop));
  run_loop.Run();
  ASSERT_EQ(1, num_complete_calls_);
  // The content does not match the hash, which is the expected outcome here;
  // what matters is that the download completed.
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::BAD_HASH), result_.error);

  std::move(cancel).Run();
  ExpectNoFurtherCompletion();
}

// A network fetcher that reports completion asynchronously after being
// cancelled results in exactly one CANCELLED completion, and the fetcher is
// not destroyed while its fetch is in flight.
TEST_F(UrlFetcherDownloaderTest, CancelWithFetcherCompletingOnCancel) {
  base::RunLoop start_loop;
  auto downloader = base::MakeRefCounted<UrlFetcherDownloader>(
      /*successor=*/nullptr,
      base::MakeRefCounted<CompletingOnCancelNetworkFetcherFactory>(
          start_loop.QuitClosure()),
      "UrlFetcherDownloaderTest");

  base::RunLoop run_loop;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL(kUrl), kHash, RecordingCallback(run_loop));
  start_loop.Run();
  ASSERT_EQ(0, num_complete_calls_);

  std::move(cancel).Run();
  run_loop.Run();
  ExpectNoFurtherCompletion();

  EXPECT_EQ(1, num_complete_calls_);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED), result_.error);
}

// A second cancellation is a no-op.
TEST_F(UrlFetcherDownloaderTest, CancelTwice) {
  auto downloader = MakeDownloader();
  base::RunLoop request_loop;
  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest&) { request_loop.Quit(); }));

  base::RunLoop run_loop;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL(kUrl), kHash, RecordingCallback(run_loop));
  request_loop.Run();

  std::move(cancel).Run();
  downloader->Cancel();
  run_loop.Run();
  ExpectNoFurtherCompletion();

  EXPECT_EQ(1, num_complete_calls_);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED), result_.error);
}

}  // namespace update_client
