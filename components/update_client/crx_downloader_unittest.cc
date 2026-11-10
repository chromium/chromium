// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/url_fetcher_downloader.h"
#include "components/update_client/utils.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ContentsEqual;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace update_client {
namespace {

constexpr char kTestFileName[] = "jebgalgnebhfojomionfpkfelancnnkf.crx";

constexpr char hash_jebg[] =
    "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498";

// A successor downloader that fails the test if it is started. It also fails
// the attempt, so that the download completes instead of hanging.
class FailingSuccessorDownloader : public CrxDownloader {
 public:
  FailingSuccessorDownloader() : CrxDownloader(nullptr) {}

 private:
  ~FailingSuccessorDownloader() override = default;

  base::OnceClosure DoStartDownload(const GURL& url) override {
    ADD_FAILURE() << "Successor started after cancellation: " << url;
    DownloadMetrics metrics;
    metrics.url = url;
    metrics.error = std::to_underlying(CrxDownloaderError::GENERIC_ERROR);
    OnDownloadComplete(/*is_handled=*/true, {.error = metrics.error}, metrics);
    return base::DoNothing();
  }
};

// A downloader whose attempts only complete when the test says so. The
// attempts are numbered from 0, in the order they start. Running the
// cancellation callback of an attempt records its number. Then, if
// `completes_on_cancel` is true, the attempt completes with CANCELLED, as with
// UrlFetcherDownloader. Otherwise, the transfer goes on, as with BITS,
// Delivery Optimization and NSURLSession, which can't interrupt it.
class ControlledDownloader : public CrxDownloader {
 public:
  ControlledDownloader(scoped_refptr<CrxDownloader> successor,
                       bool completes_on_cancel)
      : CrxDownloader(std::move(successor)),
        completes_on_cancel_(completes_on_cancel) {}

  // Fails the attempt in progress with `error`. The URL is not handled, so
  // the download may go on with the next URL or the successor downloader.
  void FailAttempt(int error) {
    DownloadMetrics metrics;
    metrics.url = url();
    metrics.error = error;
    OnDownloadComplete(/*is_handled=*/false, {.error = error}, metrics);
  }

  // Completes the attempt in progress with the downloaded file at `path`.
  void SucceedAttempt(const base::FilePath& path) {
    DownloadMetrics metrics;
    metrics.url = url();
    OnDownloadComplete(/*is_handled=*/true, {.response = path}, metrics);
  }

  // Sets a callback to run when the next attempt starts.
  void set_attempt_started_callback(base::OnceClosure callback) {
    attempt_started_callback_ = std::move(callback);
  }

  int num_attempts() const { return num_attempts_; }

  const std::vector<int>& cancelled_attempts() const {
    return cancelled_attempts_;
  }

 private:
  ~ControlledDownloader() override = default;

  base::OnceClosure DoStartDownload(const GURL& url) override {
    const int attempt = num_attempts_++;
    if (attempt_started_callback_) {
      std::move(attempt_started_callback_).Run();
    }
    return base::BindOnce(&ControlledDownloader::CancelAttempt, this, attempt);
  }

  void CancelAttempt(int attempt) {
    cancelled_attempts_.push_back(attempt);
    if (!completes_on_cancel_) {
      return;
    }
    DownloadMetrics metrics;
    metrics.url = url();
    metrics.error = std::to_underlying(CrxDownloaderError::CANCELLED);
    OnDownloadComplete(/*is_handled=*/true, {.error = metrics.error}, metrics);
  }

  const bool completes_on_cancel_;
  int num_attempts_ = 0;
  std::vector<int> cancelled_attempts_;
  base::OnceClosure attempt_started_callback_;
};

// Records how a download completes.
class DownloadRecorder {
 public:
  CrxDownloader::DownloadCallback Callback() {
    return base::BindLambdaForTesting(
        [this](const CrxDownloader::Result& result) {
          ++num_completions_;
          result_ = result;
          run_loop_.Quit();
        });
  }

  // Waits until the download completes, or until Quit() is called.
  void Wait() { run_loop_.Run(); }

  // Ends Wait() without a completion, so that a test fails instead of hanging.
  void Quit() { run_loop_.Quit(); }

  int num_completions() const { return num_completions_; }

  const CrxDownloader::Result& result() const { return result_; }

 private:
  base::RunLoop run_loop_;
  int num_completions_ = 0;
  CrxDownloader::Result result_;
};

}  // namespace

class CrxDownloaderTest : public testing::Test {
 public:
  CrxDownloaderTest();
  ~CrxDownloaderTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  void DownloadComplete(const CrxDownloader::Result& result);

  void DownloadProgress(int64_t downloaded_bytes, int64_t total_bytes);

  int GetInterceptorCount() { return interceptor_count_; }

  void AddResponse(const GURL& url,
                   const base::FilePath& file_path,
                   int net_error);

 protected:
  scoped_refptr<CrxDownloader> crx_downloader_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  CrxDownloader::DownloadCallback callback_;
  CrxDownloader::ProgressCallback progress_callback_;

  int num_download_complete_calls_ = 0;
  CrxDownloader::Result download_complete_result_ = {};

  // These members are updated by DownloadProgress.
  int num_progress_calls_ = 0;
  int64_t downloaded_bytes_ = -1;
  int64_t total_bytes_ = -1;

  // Accumulates the number of loads triggered.
  int interceptor_count_ = 0;

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  base::OnceClosure quit_closure_;
};

CrxDownloaderTest::CrxDownloaderTest()
    : callback_(base::BindOnce(&CrxDownloaderTest::DownloadComplete,
                               base::Unretained(this))),
      progress_callback_(
          base::BindRepeating(&CrxDownloaderTest::DownloadProgress,
                              base::Unretained(this))),
      task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

CrxDownloaderTest::~CrxDownloaderTest() = default;

void CrxDownloaderTest::SetUp() {
  // Do not use the background downloader in these tests.
  crx_downloader_ =
      MakeCrxDownloaderFactory(
          base::MakeRefCounted<NetworkFetcherChromiumFactory>(
              test_shared_url_loader_factory_,
              base::BindRepeating([](const GURL& url) { return false; })))
          ->MakeCrxDownloader("CrxDownloaderTest", false);

  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& request) { interceptor_count_++; }));
}

void CrxDownloaderTest::TearDown() {
  crx_downloader_ = nullptr;
}

void CrxDownloaderTest::Quit() {
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

void CrxDownloaderTest::DownloadComplete(const CrxDownloader::Result& result) {
  ++num_download_complete_calls_;
  download_complete_result_ = result;
  Quit();
}

void CrxDownloaderTest::DownloadProgress(int64_t downloaded_bytes,
                                         int64_t total_bytes) {
  if (downloaded_bytes != -1 && total_bytes != -1) {
    EXPECT_LE(downloaded_bytes, total_bytes);
  }
  downloaded_bytes_ = downloaded_bytes;
  total_bytes_ = total_bytes;
  ++num_progress_calls_;
}

void CrxDownloaderTest::AddResponse(const GURL& url,
                                    const base::FilePath& file_path,
                                    int net_error) {
  if (net_error == net::OK) {
    std::string data;
    EXPECT_TRUE(base::ReadFileToString(file_path, &data));
    auto head = network::mojom::URLResponseHead::New();
    head->content_length = data.size();
    network::URLLoaderCompletionStatus status(net_error);
    status.decoded_body_length = base::ByteSize(data.size());
    test_url_loader_factory_.AddResponse(url, std::move(head), data, status);
    return;
  }

  EXPECT_NE(net_error, net::OK);
  test_url_loader_factory_.AddResponse(
      url, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net_error));
}

void CrxDownloaderTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void CrxDownloaderTest::RunThreadsUntilIdle() {
  task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

// Tests that starting a download without a url results in an error.
TEST_F(CrxDownloaderTest, NoUrl) {
  std::vector<GURL> urls;
  crx_downloader_->StartDownload(urls, std::string("abcd"), progress_callback_,
                                 std::move(callback_));
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::NO_URL),
            download_complete_result_.error);
  EXPECT_EQ(download_complete_result_.extra_code1, 0);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that starting a download without providing a hash results in an error.
TEST_F(CrxDownloaderTest, NoHash) {
  std::vector<GURL> urls(1, GURL("http://somehost/somefile"));

  crx_downloader_->StartDownload(urls, std::string(), progress_callback_,
                                 std::move(callback_));
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::NO_HASH),
            download_complete_result_.error);
  EXPECT_EQ(download_complete_result_.extra_code1, 0);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that downloading from one url is successful.
TEST_F(CrxDownloaderTest, OneUrl) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(GetTestFilePath(kTestFileName));
  AddResponse(expected_crx_url, test_file, net::OK);

  crx_downloader_->StartDownloadFromUrl(
      expected_crx_url, std::string(hash_jebg), progress_callback_,
      std::move(callback_));
  RunThreads();

  EXPECT_EQ(1, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(downloaded_bytes_, 1015);
  EXPECT_EQ(total_bytes_, 1015);
}

// Tests that downloading from one url fails if the actual hash of the file
// does not match the expected hash.
TEST_F(CrxDownloaderTest, OneUrlBadHash) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(GetTestFilePath(kTestFileName));
  AddResponse(expected_crx_url, test_file, net::OK);

  crx_downloader_->StartDownloadFromUrl(
      expected_crx_url,
      std::string(
          "813c59747e139a608b3b5fc49633affc6db574373f309f156ea6d27229c0b3f9"),
      progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(1, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::BAD_HASH),
            download_complete_result_.error);
  EXPECT_EQ(download_complete_result_.extra_code1, 0);
  EXPECT_TRUE(download_complete_result_.response.empty());

  EXPECT_LE(1, num_progress_calls_);
}

// Tests that specifying two urls has no side effects. Expect a successful
// download, and only one download request be made.
TEST_F(CrxDownloaderTest, TwoUrls) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(GetTestFilePath(kTestFileName));
  AddResponse(expected_crx_url, test_file, net::OK);

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(1, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);
}

// Tests that the fallback to a valid url is successful.
TEST_F(CrxDownloaderTest, TwoUrls_FirstInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");
  const GURL no_file_url =
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc.crx");

  const base::FilePath test_file(GetTestFilePath(kTestFileName));
  AddResponse(expected_crx_url, test_file, net::OK);
  AddResponse(no_file_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);

  std::vector<GURL> urls;
  urls.push_back(no_file_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(2, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));
  // Assert that progress callbacks are received for the second URL.
  EXPECT_EQ(downloaded_bytes_, 1015);
  EXPECT_EQ(total_bytes_, 1015);

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  // Expect at least some progress reported by the loader.
  EXPECT_LE(1, num_progress_calls_);

  const auto download_metrics = crx_downloader_->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(no_file_url, download_metrics[0].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(-1, download_metrics[0].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[0].total_bytes);
  EXPECT_EQ(expected_crx_url, download_metrics[1].url);
  EXPECT_EQ(0, download_metrics[1].error);
  EXPECT_EQ(1015, download_metrics[1].downloaded_bytes);
  EXPECT_EQ(1015, download_metrics[1].total_bytes);
}

// Tests that the download succeeds if the first url is correct and the
// second bad url does not have a side-effect.
TEST_F(CrxDownloaderTest, TwoUrls_SecondInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");
  const GURL no_file_url =
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc.crx");

  const base::FilePath test_file(GetTestFilePath(kTestFileName));
  AddResponse(expected_crx_url, test_file, net::OK);
  AddResponse(no_file_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(no_file_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(1, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);

  EXPECT_EQ(1u, crx_downloader_->download_metrics().size());
}

// Tests that the download fails if both urls don't serve content.
TEST_F(CrxDownloaderTest, TwoUrls_BothInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  AddResponse(expected_crx_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(2, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_NE(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(download_complete_result_.response.empty());

  const auto download_metrics = crx_downloader_->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(expected_crx_url, download_metrics[0].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(-1, download_metrics[0].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[0].total_bytes);
  EXPECT_EQ(expected_crx_url, download_metrics[1].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[1].error);
  EXPECT_EQ(-1, download_metrics[1].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[1].total_bytes);
}

// A download cancelled while its fetch is in flight reports CANCELLED once
// and is not retried through the successor downloader.
TEST_F(CrxDownloaderTest, CancelDoesNotFallBackToSuccessor) {
  auto downloader = base::MakeRefCounted<UrlFetcherDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      base::MakeRefCounted<NetworkFetcherChromiumFactory>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_),
          base::BindRepeating([](const GURL& url) { return false; })),
      "CrxDownloaderTest");

  // Wait for the request to reach the network; the response is never
  // provided.
  base::RunLoop request_loop;
  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest&) { request_loop.Quit(); }));

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf"),
      hash_jebg,
      /*progress_callback=*/base::DoNothing(), recorder.Callback());
  request_loop.Run();
  ASSERT_EQ(0, recorder.num_completions());

  std::move(cancel).Run();
  recorder.Wait();

  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_TRUE(recorder.result().response.empty());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

// Cancelling a download that is retried from its second URL cancels the
// retry, not the first attempt, and does not fall back to the successor.
TEST_F(CrxDownloaderTest, CancelInterruptsUrlRetry) {
  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      /*completes_on_cancel=*/true);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownload(
      {GURL("http://localhost/download/first.crx"),
       GURL("http://localhost/download/second.crx")},
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());
  ASSERT_EQ(1, downloader->num_attempts());

  base::RunLoop retry_loop;
  downloader->set_attempt_started_callback(retry_loop.QuitClosure());
  downloader->FailAttempt(net::ERR_FILE_NOT_FOUND);
  retry_loop.Run();
  ASSERT_EQ(2, downloader->num_attempts());

  std::move(cancel).Run();
  recorder.Wait();

  EXPECT_THAT(downloader->cancelled_attempts(), ElementsAre(1));
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_TRUE(recorder.result().response.empty());

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            download_metrics[1].error);
}

// Cancelling a download that fell back to the successor downloader cancels
// the attempt of the successor.
TEST_F(CrxDownloaderTest, CancelInterruptsSuccessor) {
  auto successor = base::MakeRefCounted<ControlledDownloader>(
      /*successor=*/nullptr, /*completes_on_cancel=*/true);
  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      successor, /*completes_on_cancel=*/true);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"),
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());

  base::RunLoop fallback_loop;
  successor->set_attempt_started_callback(fallback_loop.QuitClosure());
  downloader->FailAttempt(net::ERR_FILE_NOT_FOUND);
  fallback_loop.Run();
  ASSERT_EQ(1, successor->num_attempts());

  std::move(cancel).Run();
  recorder.Wait();

  EXPECT_THAT(downloader->cancelled_attempts(), IsEmpty());
  EXPECT_THAT(successor->cancelled_attempts(), ElementsAre(0));
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_TRUE(recorder.result().response.empty());

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            download_metrics[1].error);
}

// A cancelled download whose downloader can't interrupt the transfer, and
// that then fails with another error, completes with CANCELLED without trying
// the next URL or the successor downloader.
TEST_F(CrxDownloaderTest, CancelPreventsRetryAfterUninterruptibleFailure) {
  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      /*completes_on_cancel=*/false);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownload(
      {GURL("http://localhost/download/first.crx"),
       GURL("http://localhost/download/second.crx")},
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());
  // Attempt 0 has already started: any other attempt would be a retry after
  // the cancellation.
  downloader->set_attempt_started_callback(base::BindLambdaForTesting([&] {
    ADD_FAILURE() << "Next URL tried after cancellation";
    recorder.Quit();
  }));

  std::move(cancel).Run();
  EXPECT_THAT(downloader->cancelled_attempts(), ElementsAre(0));

  downloader->FailAttempt(net::ERR_ABORTED);
  recorder.Wait();

  EXPECT_EQ(1, downloader->num_attempts());
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_EQ(0, recorder.result().extra_code1);
  EXPECT_TRUE(recorder.result().response.empty());

  // The metrics report the actual outcome of the transfer.
  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(1u, download_metrics.size());
  EXPECT_EQ(net::ERR_ABORTED, download_metrics[0].error);
}

// Cancelling a download after its attempt failed, but before the failure is
// handled, does not run the cancellation callback of the completed attempt,
// and completes with CANCELLED without trying the next URL.
TEST_F(CrxDownloaderTest, CancelAfterFailureDoesNotCancelCompletedAttempt) {
  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      /*completes_on_cancel=*/true);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownload(
      {GURL("http://localhost/download/first.crx"),
       GURL("http://localhost/download/second.crx")},
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());
  // Attempt 0 has already started: any other attempt would be a retry after
  // the cancellation.
  downloader->set_attempt_started_callback(base::BindLambdaForTesting([&] {
    ADD_FAILURE() << "Next URL tried after cancellation";
    recorder.Quit();
  }));

  // The failure is handled by a posted task, which runs after the
  // cancellation.
  downloader->FailAttempt(net::ERR_FILE_NOT_FOUND);
  std::move(cancel).Run();
  recorder.Wait();

  EXPECT_THAT(downloader->cancelled_attempts(), IsEmpty());
  EXPECT_EQ(1, downloader->num_attempts());
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_TRUE(recorder.result().response.empty());

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(1u, download_metrics.size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
}

// Cancelling a download while the file of its successful attempt is being
// verified does not run the cancellation callback of the completed attempt:
// the download completes successfully.
TEST_F(CrxDownloaderTest, CancelDuringHashVerificationCompletesSuccessfully) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file = temp_dir.GetPath().AppendASCII(kTestFileName);
  ASSERT_TRUE(base::CopyFile(GetTestFilePath(kTestFileName), file));

  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      /*completes_on_cancel=*/true);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownloadFromUrl(
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"),
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());

  // The file is verified by a posted task, whose reply runs after the
  // cancellation.
  downloader->SucceedAttempt(file);
  std::move(cancel).Run();
  recorder.Wait();

  EXPECT_THAT(downloader->cancelled_attempts(), IsEmpty());
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(0, recorder.result().error);
  EXPECT_EQ(file, recorder.result().response);
  EXPECT_TRUE(base::PathExists(file));

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(1u, download_metrics.size());
  EXPECT_EQ(0, download_metrics[0].error);
}

// A cancelled download routed to a successor that can't interrupt the
// transfer, and whose transfer then fails, completes with CANCELLED without
// trying the next URL in the successor or the successor of the successor.
TEST_F(CrxDownloaderTest, CancelUninterruptibleSuccessorPreventsFallback) {
  auto successor = base::MakeRefCounted<ControlledDownloader>(
      base::MakeRefCounted<FailingSuccessorDownloader>(),
      /*completes_on_cancel=*/false);
  auto downloader = base::MakeRefCounted<ControlledDownloader>(
      successor, /*completes_on_cancel=*/true);

  DownloadRecorder recorder;
  base::OnceClosure cancel = downloader->StartDownload(
      {GURL("http://localhost/download/first.crx"),
       GURL("http://localhost/download/second.crx")},
      hash_jebg, /*progress_callback=*/base::DoNothing(), recorder.Callback());

  // Both URLs fail in the first downloader, which falls back to the successor.
  base::RunLoop retry_loop;
  downloader->set_attempt_started_callback(retry_loop.QuitClosure());
  downloader->FailAttempt(net::ERR_FILE_NOT_FOUND);
  retry_loop.Run();
  base::RunLoop fallback_loop;
  successor->set_attempt_started_callback(fallback_loop.QuitClosure());
  downloader->FailAttempt(net::ERR_FILE_NOT_FOUND);
  fallback_loop.Run();
  ASSERT_EQ(1, successor->num_attempts());

  // Attempt 0 of the successor has already started: any other attempt would
  // be a retry after the cancellation.
  successor->set_attempt_started_callback(base::BindLambdaForTesting([&] {
    ADD_FAILURE() << "Next URL tried after cancellation";
    recorder.Quit();
  }));

  std::move(cancel).Run();
  EXPECT_THAT(successor->cancelled_attempts(), ElementsAre(0));

  successor->FailAttempt(net::ERR_ABORTED);
  recorder.Wait();

  EXPECT_THAT(downloader->cancelled_attempts(), IsEmpty());
  EXPECT_EQ(1, successor->num_attempts());
  EXPECT_EQ(1, recorder.num_completions());
  EXPECT_EQ(std::to_underlying(CrxDownloaderError::CANCELLED),
            recorder.result().error);
  EXPECT_TRUE(recorder.result().response.empty());

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(3u, download_metrics.size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[1].error);
  EXPECT_EQ(net::ERR_ABORTED, download_metrics[2].error);
}

// Tests that falling back to a successor downloader succeeds and posts progress
// callbacks.
TEST_F(CrxDownloaderTest, FallbackToSuccessor) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");
  const base::FilePath test_file(GetTestFilePath(kTestFileName));

  auto network_fetcher_factory =
      base::MakeRefCounted<NetworkFetcherChromiumFactory>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_),
          base::BindRepeating([](const GURL& url) { return false; }));
  auto downloader = base::MakeRefCounted<UrlFetcherDownloader>(
      base::MakeRefCounted<UrlFetcherDownloader>(
          nullptr, network_fetcher_factory, "CrxDownloaderTest"),
      network_fetcher_factory, "CrxDownloaderTest");

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        interceptor_count_++;
        if (interceptor_count_ == 1) {
          AddResponse(expected_crx_url, base::FilePath(),
                      net::ERR_FILE_NOT_FOUND);
        } else {
          AddResponse(expected_crx_url, test_file, net::OK);
        }
      }));

  downloader->StartDownloadFromUrl(expected_crx_url, std::string(hash_jebg),
                                   progress_callback_, std::move(callback_));
  RunThreads();

  EXPECT_EQ(2, GetInterceptorCount());
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));
  // Assert that progress callbacks are received for the successor
  // CrxDownloader.
  EXPECT_EQ(downloaded_bytes_, 1015);
  EXPECT_EQ(total_bytes_, 1015);

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);

  const auto download_metrics = downloader->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(expected_crx_url, download_metrics[0].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(-1, download_metrics[0].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[0].total_bytes);
  EXPECT_EQ(expected_crx_url, download_metrics[1].url);
  EXPECT_EQ(0, download_metrics[1].error);
  EXPECT_EQ(1015, download_metrics[1].downloaded_bytes);
  EXPECT_EQ(1015, download_metrics[1].total_bytes);
}

}  // namespace update_client
