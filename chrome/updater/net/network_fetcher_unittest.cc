// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/update_client/network.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include <cstddef>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/net/network_file_fetcher.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#endif

namespace updater {
namespace {

using ::testing::_;

using HeaderMap = base::flat_map<std::string, std::string>;

// A mock NetworkFetcher for testing LoggingNetworkFetcher delegation.
class MockNetworkFetcher : public update_client::NetworkFetcher {
 public:
  ~MockNetworkFetcher() override = default;

  MOCK_METHOD(void,
              PostRequest,
              (const GURL& url,
               const std::string& post_data,
               const std::string& content_type,
               const HeaderMap& post_additional_headers,
               ResponseStartedCallback response_started_callback,
               ProgressCallback progress_callback,
               PostRequestCompleteCallback post_request_complete_callback),
              (override));

  MOCK_METHOD(
      base::OnceClosure,
      DownloadToFile,
      (const GURL& url,
       const base::FilePath& file_path,
       ResponseStartedCallback response_started_callback,
       ProgressCallback progress_callback,
       DownloadToFileCompleteCallback download_to_file_complete_callback),
      (override));
};

// Tests for NetworkFetcherFactory.
class NetworkFetcherFactoryTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkFetcherFactoryTest, CreatesNonNullFetcher) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()),
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, NulloptProxyConfig) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      /*policy_service_proxy_configuration=*/std::nullopt,
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, AutoDetectProxyConfig) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = true;
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, ExplicitProxyUrl) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = false;
  proxy_config.proxy_url = "http://proxy.example.com:8080";
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, PacUrl) {
  PolicyServiceProxyConfiguration proxy_config;
  proxy_config.proxy_auto_detect = false;
  proxy_config.proxy_pac_url = "http://proxy.example.com/pac";
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      std::move(proxy_config), /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);
  auto fetcher = factory->Create();
  EXPECT_NE(fetcher, nullptr);
}

TEST_F(NetworkFetcherFactoryTest, CreatesMultipleIndependentFetchers) {
  auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
      /*policy_service_proxy_configuration=*/std::nullopt,
      /*event_logger=*/nullptr);
  ASSERT_NE(factory, nullptr);

  auto fetcher1 = factory->Create();
  auto fetcher2 = factory->Create();
  EXPECT_NE(fetcher1, nullptr);
  EXPECT_NE(fetcher2, nullptr);
  EXPECT_NE(fetcher1.get(), fetcher2.get());
}

// Tests for LoggingNetworkFetcher.
class LoggingNetworkFetcherTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoggingNetworkFetcherTest, NullImplDoesNotCrash) {
  LoggingNetworkFetcher logging_fetcher(nullptr);
}

TEST_F(LoggingNetworkFetcherTest, DelegatesPostRequest) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const GURL kUrl("http://example.com/update");
  const std::string kPostData = "test_data";
  const std::string kContentType = "application/json";

  EXPECT_CALL(*mock_ptr, PostRequest(kUrl, kPostData, kContentType, _, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run("response", 0, "etag", "proof", "cookie", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      kUrl, kPostData, kContentType, {}, base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, std::optional<std::string> response_body,
             int net_error, const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            EXPECT_EQ(*response_body, "response");
            EXPECT_EQ(net_error, 0);
            EXPECT_EQ(header_etag, "etag");
            EXPECT_EQ(header_x_cup_server_proof, "proof");
            EXPECT_EQ(header_set_cookie, "cookie");
            EXPECT_EQ(xheader_retry_after_sec, -1);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, DelegatesDownloadToFile) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const GURL kUrl("http://example.com/download");
  const base::FilePath kPath(FILE_PATH_LITERAL("file.bin"));

  EXPECT_CALL(*mock_ptr, DownloadToFile(kUrl, kPath, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       callback) {
        std::move(callback).Run(0, 1024);
        return base::DoNothing();
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  base::OnceClosure cancel = logging_fetcher.DownloadToFile(
      kUrl, kPath, base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, int net_error, int64_t content_size) {
            EXPECT_EQ(net_error, 0);
            EXPECT_EQ(content_size, 1024);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, PropagatesPostRequestError) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  EXPECT_CALL(*mock_ptr, PostRequest(_, _, _, _, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run(std::nullopt, -1, "", "", "", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      GURL("http://example.com"), "data", "text/plain", {}, base::DoNothing(),
      base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, std::optional<std::string> response_body,
             int net_error, const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            EXPECT_FALSE(response_body.has_value());
            EXPECT_EQ(net_error, -1);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, ForwardsAdditionalHeaders) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  const HeaderMap kHeaders = {
      {"Authorization", "Bearer token123"},
      {"X-Custom", "value"},
  };

  EXPECT_CALL(*mock_ptr, PostRequest(_, _, _, kHeaders, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const HeaderMap&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       callback) {
        std::move(callback).Run("ok", 0, "", "", "", -1);
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.PostRequest(
      GURL("http://example.com"), "data", "text/plain", kHeaders,
      base::DoNothing(), base::DoNothing(),
      base::BindOnce([](base::OnceClosure quit, std::optional<std::string>, int,
                        const std::string&, const std::string&,
                        const std::string&, int64_t) { std::move(quit).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(LoggingNetworkFetcherTest, PropagatesDownloadError) {
  auto mock = std::make_unique<MockNetworkFetcher>();
  MockNetworkFetcher* mock_ptr = mock.get();

  EXPECT_CALL(*mock_ptr, DownloadToFile(_, _, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       callback) {
        std::move(callback).Run(-1, 0);
        return base::DoNothing();
      });

  LoggingNetworkFetcher logging_fetcher(std::move(mock));
  base::RunLoop run_loop;
  logging_fetcher.DownloadToFile(
      GURL("http://example.com/file"), base::FilePath(FILE_PATH_LITERAL("f")),
      base::DoNothing(), base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure quit, int net_error, int64_t content_size) {
            EXPECT_EQ(net_error, -1);
            EXPECT_EQ(content_size, 0);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
class FakeFetchService : public mojom::FetchService {
 public:
  using DownloadHandler =
      base::OnceCallback<void(mojo::ScopedDataPipeProducerHandle,
                              mojo::Remote<mojom::FileDownloadObserver>)>;

  explicit FakeFetchService(DownloadHandler on_download)
      : on_download_(std::move(on_download)) {}
  ~FakeFetchService() override = default;

  mojo::PendingRemote<mojom::FetchService> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void PostRequest(const GURL& url,
                   const std::string& post_data,
                   const std::string& content_type,
                   std::vector<mojom::HttpHeaderPtr> additional_headers,
                   PostRequestCallback callback) override {
    NOTREACHED();
  }

  void DownloadToStream(const GURL& url,
                        mojo::ScopedDataPipeProducerHandle response_stream,
                        DownloadToStreamCallback callback) override {
    mojo::Remote<mojom::FileDownloadObserver> observer;
    std::move(callback).Run(observer.BindNewPipeAndPassReceiver());
    std::move(on_download_)
        .Run(std::move(response_stream), std::move(observer));
  }

 private:
  DownloadHandler on_download_;
  mojo::Receiver<mojom::FetchService> receiver_{this};
};

class OutOfProcessNetworkFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::test::TaskEnvironment task_environment_;
  ScopedIPCSupportWrapper ipc_support_;
  base::ScopedTempDir temp_dir_;
};

// `OnDownloadComplete` over the Mojo pipe may arrive before `DataPipeDrainer`
// finishes reading and sees the data pipe close on its background sequence.
TEST_F(OutOfProcessNetworkFetcherTest,
       DownloadToFile_MojoCompleteBeforeDataComplete) {
  static constexpr std::string_view kPayload = "test download payload";
  const base::FilePath dest_path =
      temp_dir_.GetPath().AppendASCII("downloaded.bin");

  FakeFetchService service(base::BindLambdaForTesting(
      [&](mojo::ScopedDataPipeProducerHandle response_stream,
          mojo::Remote<mojom::FileDownloadObserver> observer) {
        observer->OnResponseStarted(200, kPayload.size());
        observer->OnDownloadComplete(0, kPayload.size());
        observer.FlushForTesting();

        size_t actually_written = 0;
        ASSERT_EQ(response_stream->WriteData(base::as_byte_span(kPayload),
                                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                             actually_written),
                  MOJO_RESULT_OK);
        EXPECT_EQ(actually_written, kPayload.size());
        response_stream.reset();
      }));

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      CreateOutOfProcessNetworkFetcherForTesting(
          service.BindNewPipeAndPassRemote());

  base::RunLoop run_loop;
  int response_code = 0;
  fetcher->DownloadToFile(
      GURL("http://example.com/payload.bin"), dest_path,
      base::BindLambdaForTesting(
          [&](int code, int64_t /*content_length*/) { response_code = code; }),
      base::DoNothing(),
      base::BindLambdaForTesting([&](int net_error, int64_t content_size) {
        EXPECT_EQ(net_error, 0);
        EXPECT_EQ(content_size, static_cast<int64_t>(kPayload.size()));
        fetcher.reset();
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(response_code, 200);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_path, &contents));
  EXPECT_EQ(contents, kPayload);
}

TEST_F(OutOfProcessNetworkFetcherTest,
       DownloadToFile_DataCompleteBeforeMojoComplete) {
  static constexpr std::string_view kPayload = "payload drained first";
  const base::FilePath dest_path =
      temp_dir_.GetPath().AppendASCII("downloaded.bin");

  mojo::Remote<mojom::FileDownloadObserver> saved_observer;
  FakeFetchService service(base::BindLambdaForTesting(
      [&](mojo::ScopedDataPipeProducerHandle response_stream,
          mojo::Remote<mojom::FileDownloadObserver> observer) {
        observer->OnResponseStarted(200, kPayload.size());
        size_t actually_written = 0;
        ASSERT_EQ(response_stream->WriteData(base::as_byte_span(kPayload),
                                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                             actually_written),
                  MOJO_RESULT_OK);
        EXPECT_EQ(actually_written, kPayload.size());
        response_stream.reset();
        saved_observer = std::move(observer);
      }));

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      CreateOutOfProcessNetworkFetcherForTesting(
          service.BindNewPipeAndPassRemote());

  base::RunLoop progress_loop;
  base::RunLoop complete_loop;
  fetcher->DownloadToFile(
      GURL("http://example.com/payload.bin"), dest_path, base::DoNothing(),
      base::BindLambdaForTesting([&](int64_t current) {
        if (current == static_cast<int64_t>(kPayload.size())) {
          progress_loop.Quit();
        }
      }),
      base::BindLambdaForTesting([&](int net_error, int64_t content_size) {
        EXPECT_EQ(net_error, 0);
        EXPECT_EQ(content_size, static_cast<int64_t>(kPayload.size()));
        fetcher.reset();
        complete_loop.Quit();
      }));

  progress_loop.Run();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  ASSERT_TRUE(saved_observer.is_bound());
  saved_observer->OnDownloadComplete(0, kPayload.size());
  complete_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_path, &contents));
  EXPECT_EQ(contents, kPayload);
}

TEST_F(OutOfProcessNetworkFetcherTest, DownloadToFile_FileOpenError) {
  static constexpr std::string_view kPayload = "payload for failed open";
  const base::FilePath dest_path =
      temp_dir_.GetPath().AppendASCII("missing_dir").AppendASCII("file.bin");

  FakeFetchService service(base::BindLambdaForTesting(
      [&](mojo::ScopedDataPipeProducerHandle response_stream,
          mojo::Remote<mojom::FileDownloadObserver> observer) {
        observer->OnResponseStarted(200, kPayload.size());
        size_t actually_written = 0;
        const MojoResult rv = response_stream->WriteData(
            base::as_byte_span(kPayload), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
            actually_written);
        EXPECT_TRUE(rv == MOJO_RESULT_OK ||
                    rv == MOJO_RESULT_FAILED_PRECONDITION);
        response_stream.reset();
        observer->OnDownloadComplete(0, kPayload.size());
      }));

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      CreateOutOfProcessNetworkFetcherForTesting(
          service.BindNewPipeAndPassRemote());

  base::RunLoop run_loop;
  fetcher->DownloadToFile(
      GURL("http://example.com/payload.bin"), dest_path, base::DoNothing(),
      base::DoNothing(),
      base::BindLambdaForTesting([&](int net_error, int64_t content_size) {
        EXPECT_EQ(net_error, kErrorFailedToWriteFile);
        EXPECT_EQ(content_size, -1);
        fetcher.reset();
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(OutOfProcessNetworkFetcherTest,
       DownloadToFile_DisconnectBeforeComplete) {
  const base::FilePath dest_path =
      temp_dir_.GetPath().AppendASCII("downloaded.bin");

  FakeFetchService service(base::BindLambdaForTesting(
      [](mojo::ScopedDataPipeProducerHandle response_stream,
         mojo::Remote<mojom::FileDownloadObserver> observer) {
        response_stream.reset();
        observer.reset();
      }));

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      CreateOutOfProcessNetworkFetcherForTesting(
          service.BindNewPipeAndPassRemote());

  base::RunLoop run_loop;
  fetcher->DownloadToFile(
      GURL("http://example.com/payload.bin"), dest_path, base::DoNothing(),
      base::DoNothing(),
      base::BindLambdaForTesting([&](int net_error, int64_t /*content_size*/) {
        EXPECT_EQ(net_error, kErrorIpcDisconnect);
        fetcher.reset();
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Destroying the fetcher cancels the download: the completion callback must
// not run, even if the fetch service completes the download afterwards.
TEST_F(OutOfProcessNetworkFetcherTest,
       DownloadToFile_FetcherDestroyedMidDownload) {
  static constexpr std::string_view kPayload = "payload after destruction";
  const base::FilePath dest_path =
      temp_dir_.GetPath().AppendASCII("downloaded.bin");

  mojo::ScopedDataPipeProducerHandle saved_stream;
  mojo::Remote<mojom::FileDownloadObserver> saved_observer;
  FakeFetchService service(base::BindLambdaForTesting(
      [&](mojo::ScopedDataPipeProducerHandle response_stream,
          mojo::Remote<mojom::FileDownloadObserver> observer) {
        observer->OnResponseStarted(200, kPayload.size());
        saved_stream = std::move(response_stream);
        saved_observer = std::move(observer);
      }));

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      CreateOutOfProcessNetworkFetcherForTesting(
          service.BindNewPipeAndPassRemote());

  base::RunLoop response_started_loop;
  bool download_completed = false;
  fetcher->DownloadToFile(
      GURL("http://example.com/payload.bin"), dest_path,
      base::BindLambdaForTesting(
          [&](int /*response_code*/, int64_t /*content_length*/) {
            response_started_loop.Quit();
          }),
      base::DoNothing(),
      base::BindLambdaForTesting(
          [&](int /*net_error*/, int64_t /*content_size*/) {
            download_completed = true;
          }));
  response_started_loop.Run();
  fetcher.reset();

  // Complete the download after the fetcher is gone. The write fails if the
  // reader has already closed its end of the pipe.
  size_t actually_written = 0;
  const MojoResult rv = saved_stream->WriteData(
      base::as_byte_span(kPayload), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
      actually_written);
  EXPECT_TRUE(rv == MOJO_RESULT_OK || rv == MOJO_RESULT_FAILED_PRECONDITION);
  saved_stream.reset();
  saved_observer->OnDownloadComplete(0, kPayload.size());
  saved_observer.FlushForTesting();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop main_thread_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, main_thread_loop.QuitClosure());
  main_thread_loop.Run();

  EXPECT_FALSE(download_completed);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace
}  // namespace updater
