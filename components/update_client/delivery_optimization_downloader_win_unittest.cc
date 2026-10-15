// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/delivery_optimization_downloader_win.h"

#include <objbase.h>

#include <windows.h>

#include <deliveryoptimization.h>
#include <stdint.h>
#include <winerror.h>
#include <wrl/client.h>

#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_com_initializer.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "crypto/hash.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {
namespace {
constexpr base::FilePath::CharType kTestDirPrefix[] = FILE_PATH_LITERAL(
    "DeliveryOptimizationDownloaderWinTest_chrome_DO_(test)_");
constexpr base::FilePath::CharType kTestDirMatcher[] = FILE_PATH_LITERAL(
    "DeliveryOptimizationDownloaderWinTest_chrome_DO_(test)_*");
constexpr wchar_t kTestDownloadFilename[] = L"test_file.txt";
constexpr char kTestDownloadContent[] = "Hello, World!";
}  // namespace

class DeliveryOptimizationDownloaderWinTest : public testing::Test {
 protected:
  // Overrides from testing::Test
  void TearDown() override;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<DeliveryOptimizationDownloader> downloader_ =
      base::MakeRefCounted<DeliveryOptimizationDownloader>(
          nullptr,
          "DeliveryOptimizationDownloaderWinTest");
};

void DeliveryOptimizationDownloaderWinTest::TearDown() {
  base::FilePath dir;
  ASSERT_TRUE(base::GetSecureTempDirectory(&dir));
  CleanupDirectoriesOlderThan(dir, kTestDirMatcher, base::Seconds(0));
}

TEST_F(DeliveryOptimizationDownloaderWinTest, CleansStaleDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(base::WriteFile(download_dir_path.Append(kTestDownloadFilename),
                              kTestDownloadContent));

  // Manipulate the creation time of the directory.
  FILETIME creation_filetime =
      (base::Time::NowFromSystemTime() - base::Days(5)).ToFileTime();
  base::File download_dir(download_dir_path,
                          base::File::FLAG_OPEN |
                              base::File::FLAG_WIN_BACKUP_SEMANTICS |
                              base::File::FLAG_WRITE_ATTRIBUTES);
  ASSERT_TRUE(download_dir.IsValid());
  ASSERT_TRUE(::SetFileTime(download_dir.GetPlatformFile(), &creation_filetime,
                            nullptr, nullptr));
  download_dir.Close();
  downloader_->CleanupStaleDownloads();
  EXPECT_FALSE(base::DirectoryExists(download_dir_path));
}

TEST_F(DeliveryOptimizationDownloaderWinTest, RetainsRecentDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(base::WriteFile(download_dir_path.Append(kTestDownloadFilename),
                              kTestDownloadContent));
  downloader_->CleanupStaleDownloads();
  EXPECT_TRUE(base::DirectoryExists(download_dir_path));
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetHttpStatusFromHresult) {
  using Downloader = DeliveryOptimizationDownloader;
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(S_OK), 0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(E_FAIL), 0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_NOT_FOUND), 404);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_SERVER_ERROR),
            500);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_VERSION_NOT_SUP),
            505);

  // Any 5xx code is decoded, not only the standard ones, so that it can be
  // classified as a server error.
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 511)),
            511);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 599)),
            599);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 600)),
            0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 99)),
            0);

  // Other facilities with a plausible low word are not HTTP statuses.
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 404)),
            0);
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetDownloadError) {
  using Downloader = DeliveryOptimizationDownloader;
  DO_DOWNLOAD_STATUS status = {};
  EXPECT_EQ(Downloader::GetDownloadError(status), S_OK);

  status.ExtendedError = HTTP_E_STATUS_SERVER_ERROR;
  EXPECT_EQ(Downloader::GetDownloadError(status), HTTP_E_STATUS_SERVER_ERROR);

  // |Error| takes precedence over |ExtendedError|.
  status.Error = E_ABORT;
  EXPECT_EQ(Downloader::GetDownloadError(status), E_ABORT);

  // Success codes are not errors.
  status.Error = S_FALSE;
  status.ExtendedError = S_FALSE;
  EXPECT_EQ(Downloader::GetDownloadError(status), S_OK);
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetDownloadByteCount) {
  using Downloader = DeliveryOptimizationDownloader;
  int64_t downloaded_bytes = 0;
  int64_t total_bytes = 0;

  // A total of zero means the size is not known yet.
  DO_DOWNLOAD_STATUS status = {};
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, 0);
  EXPECT_EQ(total_bytes, -1);

  status.BytesTransferred = 10;
  status.BytesTotal = 100;
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, 10);
  EXPECT_EQ(total_bytes, 100);

  // Values which do not fit in an int64_t are reported as unknown.
  status.BytesTransferred = std::numeric_limits<uint64_t>::max();
  status.BytesTotal = std::numeric_limits<uint64_t>::max();
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, -1);
  EXPECT_EQ(total_bytes, -1);
}

// Integration tests which drive a real Delivery Optimization download against
// a local HTTP server, through the public CrxDownloader interface. The tests
// are skipped on hosts where the DO service is not available, since the DO
// COM class is not registered on every Windows build and may be inaccessible
// in some test environments.
class DeliveryOptimizationDownloaderIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    // Surface the downloader's VLOGs in the test output. Level 1 identifies
    // the DO call that failed when a download does not go through; level 2
    // adds the status of the download at every poll.
    vmodule_switches_.InitWithSwitches(
        "delivery_optimization_downloader_win=2");

    // Probe DO availability the same way the downloader does, on the COM MTA.
    base::win::ScopedCOMInitializer com_initializer(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(com_initializer.Succeeded());
    Microsoft::WRL::ComPtr<IDOManager> do_manager;
    const HRESULT hr =
        ::CoCreateInstance(__uuidof(DeliveryOptimization), nullptr,
                           CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&do_manager));
    if (FAILED(hr)) {
      GTEST_SKIP() << "Delivery Optimization is not available on this host: "
                   << std::hex << hr;
    }

    // Serve files from a directory rather than from a canned handler: DO is a
    // range-based downloader which may probe the size with a HEAD request and
    // fetch the file in byte ranges, and the file handler of the test server
    // supports HEAD, Range and Accept-Ranges.
    ASSERT_TRUE(served_dir_.CreateUniqueTempDir());
    test_server_.ServeFilesFromDirectory(served_dir_.GetPath());
    test_server_.RegisterRequestMonitor(base::BindRepeating(
        &DeliveryOptimizationDownloaderIntegrationTest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE((test_server_handle_ = test_server_.StartAndReturnHandle()));
  }

  void TearDown() override {
    // Nothing may run the completion callback after this point, and no
    // request may reach |MonitorRequest| once the server is shut down.
    weak_factory_.InvalidateWeakPtrs();
    downloader_ = nullptr;
    test_server_handle_ = net::test_server::EmbeddedTestServerHandle();
    base::FilePath dir;
    ASSERT_TRUE(base::GetSecureTempDirectory(&dir));
    CleanupDirectoriesOlderThan(dir, kIntegrationTestDirMatcher,
                                base::Seconds(0));
  }

  // Writes `content` to a file served by the test server and returns its url.
  // The path is unique per test run so that downloads from previous runs
  // still queued in DO are not picked up, and so that DO can't have the
  // content cached.
  GURL Serve(const std::string& content) {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string path =
        base::StrCat({"/", test_info->test_suite_name(), "/", test_info->name(),
                      "/", base::UnguessableToken::Create().ToString(), "/",
                      kTestDownloadFilenameAscii});
    const base::FilePath file =
        served_dir_.GetPath().AppendASCII(path.substr(1));
    CHECK(base::CreateDirectory(file.DirName()));
    CHECK(base::WriteFile(file, content));
    {
      base::AutoLock lock(requests_lock_);
      served_path_ = path;
      requests_.clear();
    }
    return test_server_.GetURL(path);
  }

  // Runs a download of `url` to completion, or fails the test if the download
  // does not complete in a reasonable time, and returns its result.
  //
  // DO downloads from the local server complete within the first or second
  // 4 s status poll of the downloader, so `action_max_timeout()` (30 s at
  // base, scaled for slow configurations) leaves a wide margin. If it does
  // fire, the test fails and the download is left to the downloader: the
  // CrxDownloader interface has no cancel, the COM sequence keeps polling
  // until the download completes or the stuck timeout fires, and the
  // download directory removed by TearDown makes the DO download unusable,
  // so the next downloader instance aborts and removes it from the DO queue.
  // The completion callback is bound to a weak pointer so that a late
  // completion can't touch state which no longer exists.
  CrxDownloader::Result Download(const GURL& url, const std::string& hash) {
    downloader_ = base::MakeRefCounted<DeliveryOptimizationDownloader>(
        nullptr, kIntegrationTestProdId);
    result_.reset();
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    downloader_->StartDownloadFromUrl(
        url, hash, base::DoNothing(),
        base::BindOnce(
            &DeliveryOptimizationDownloaderIntegrationTest::OnDownloadComplete,
            weak_factory_.GetWeakPtr()));
    base::OneShotTimer timeout;
    timeout.Start(FROM_HERE, TestTimeouts::action_max_timeout(),
                  base::BindLambdaForTesting([&] {
                    ADD_FAILURE() << "Download did not complete in time.";
                    result_.emplace().error =
                        std::to_underlying(CrxDownloaderError::GENERIC_ERROR);
                    run_loop.Quit();
                  }));
    run_loop.Run();
    weak_factory_.InvalidateWeakPtrs();
    return *result_;
  }

  std::vector<CrxDownloader::DownloadMetrics> download_metrics() const {
    return downloader_->download_metrics();
  }

  // The requests the server has seen for the served file, as "METHOD [Range]"
  // strings, in order.
  std::vector<std::string> requests() const {
    base::AutoLock lock(requests_lock_);
    return requests_;
  }

  // Returns the number of download directories of this test fixture which
  // currently exist, which should be zero after a failed download.
  static int CountDownloadDirectories() {
    base::FilePath dir;
    CHECK(base::GetSecureTempDirectory(&dir));
    int count = 0;
    base::FileEnumerator e(dir, false, base::FileEnumerator::DIRECTORIES,
                           kIntegrationTestDirMatcher);
    for (base::FilePath p = e.Next(); !p.empty(); p = e.Next()) {
      ++count;
    }
    return count;
  }

 private:
  static constexpr char kIntegrationTestProdId[] =
      "DeliveryOptimizationDownloaderIntegrationTest";
  static constexpr base::FilePath::CharType kIntegrationTestDirMatcher[] =
      FILE_PATH_LITERAL(
          "DeliveryOptimizationDownloaderIntegrationTest_chrome_DO_*");
  static constexpr char kTestDownloadFilenameAscii[] = "test_file.txt";

  void OnDownloadComplete(const CrxDownloader::Result& result) {
    result_ = result;
    std::move(quit_closure_).Run();
  }

  // Called on the IO thread of the test server for every request.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_lock_);
    if (request.relative_url != served_path_) {
      return;
    }
    std::string entry = request.method_string;
    const auto range = request.headers.find("Range");
    if (range != request.headers.end()) {
      base::StrAppend(&entry, {" ", range->second});
    }
    requests_.push_back(std::move(entry));
  }

  // Declared first so that it is destroyed last, as required by the class.
  logging::ScopedVmoduleSwitches vmodule_switches_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir served_dir_;

  // The state below is accessed by |MonitorRequest| on the IO thread of the
  // test server, so it is declared before the server and its handle, which
  // stops the IO thread when destroyed, and guarded by a lock.
  mutable base::Lock requests_lock_;
  std::string served_path_ GUARDED_BY(requests_lock_);
  std::vector<std::string> requests_ GUARDED_BY(requests_lock_);
  net::test_server::EmbeddedTestServer test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;

  scoped_refptr<DeliveryOptimizationDownloader> downloader_;
  std::optional<CrxDownloader::Result> result_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<DeliveryOptimizationDownloaderIntegrationTest>
      weak_factory_{this};
};

TEST_F(DeliveryOptimizationDownloaderIntegrationTest, DownloadsFile) {
  base::HistogramTester histograms;
  const std::string content(100'000, 'A');
  const GURL url = Serve(content);

  const CrxDownloader::Result result =
      Download(url, base::HexEncode(crypto::hash::Sha256(content)));

  EXPECT_EQ(result.error, 0)
      << "error: 0x" << std::hex << result.error
      << ", requests: " << base::JoinString(requests(), ", ");
  EXPECT_EQ(result.extra_code1, 0)
      << "extra_code1: 0x" << std::hex << result.extra_code1;
  ASSERT_FALSE(result.response.empty());
  std::string downloaded;
  ASSERT_TRUE(base::ReadFileToString(result.response, &downloaded));
  EXPECT_EQ(downloaded, content);

  // The content came from this server and not from a DO cache: the url is
  // unique to this run. DO decides for itself whether to probe with HEAD or
  // fetch in ranges, so only the fact that the file was requested is checked.
  EXPECT_FALSE(requests().empty());

  const std::vector<CrxDownloader::DownloadMetrics> metrics =
      download_metrics();
  ASSERT_EQ(metrics.size(), 1u);
  EXPECT_EQ(metrics[0].url, url);
  EXPECT_EQ(metrics[0].downloader,
            CrxDownloader::DownloadMetrics::kDeliveryOptimization);
  EXPECT_EQ(metrics[0].error, 0);
  EXPECT_EQ(metrics[0].downloaded_bytes,
            base::checked_cast<int64_t>(content.size()));
  EXPECT_EQ(metrics[0].total_bytes,
            base::checked_cast<int64_t>(content.size()));

  histograms.ExpectUniqueSample(
      "UpdateClient.DeliveryOptimization.ManagerCreateResult", S_OK, 1);
  // 4 is FinalDownloadState::kFinalized.
  histograms.ExpectUniqueSample(
      "UpdateClient.DeliveryOptimization.FinalDownloadState", 4, 1);

  // The file is handed over to the caller, who owns it from now on.
  EXPECT_TRUE(base::DeleteFile(result.response));
}

// A download whose content does not match the expected hash is rejected, and
// the downloaded file and its directory are removed.
TEST_F(DeliveryOptimizationDownloaderIntegrationTest, RejectsBadHash) {
  const std::string content = "Hello, World!";
  const GURL url = Serve(content);

  const CrxDownloader::Result result =
      Download(url, base::HexEncode(crypto::hash::Sha256("other")));

  EXPECT_EQ(result.error, std::to_underlying(CrxDownloaderError::BAD_HASH))
      << "error: 0x" << std::hex << result.error << ", extra_code1: 0x"
      << result.extra_code1;
  EXPECT_TRUE(result.response.empty());
  EXPECT_EQ(CountDownloadDirectories(), 0);
}

}  // namespace update_client
