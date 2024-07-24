// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ContentsEqual;

namespace update_client {
namespace {

constexpr char kTestFileName[] = "jebgalgnebhfojomionfpkfelancnnkf.crx";

constexpr char hash_jebg[] =
    "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498";

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
          ->MakeCrxDownloader(false);
  crx_downloader_->set_progress_callback(progress_callback_);

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
    status.decoded_body_length = data.size();
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
  crx_downloader_->StartDownload(urls, std::string("abcd"),
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

  crx_downloader_->StartDownload(urls, std::string(), std::move(callback_));
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
      expected_crx_url, std::string(hash_jebg), std::move(callback_));
  RunThreads();

  EXPECT_EQ(1, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);
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
      std::move(callback_));
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
                                 std::move(callback_));
  RunThreads();

  EXPECT_EQ(2, GetInterceptorCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(0, download_complete_result_.extra_code1);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

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
                                 std::move(callback_));
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

}  // namespace update_client
