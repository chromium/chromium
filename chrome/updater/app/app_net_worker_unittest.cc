// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_net_worker.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/net/fetcher_callback_adapter.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

namespace updater {

namespace {

class TestDrainer : public mojo::DataPipeDrainer::Client {
 public:
  TestDrainer(mojo::ScopedDataPipeConsumerHandle consumer,
              base::OnceClosure on_complete)
      : drainer_(this, std::move(consumer)),
        on_complete_(std::move(on_complete)) {}

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_.append(base::as_string_view(data));
  }
  void OnDataComplete() override { std::move(on_complete_).Run(); }

  const std::string& data() const { return data_; }

 private:
  mojo::DataPipeDrainer drainer_;
  base::OnceClosure on_complete_;
  std::string data_;
};

}  // namespace

class AppNetWorkerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mojo::PlatformChannel channel;
    base::LaunchOptions options;
    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();

    channel.PrepareToPassRemoteEndpoint(&options, &command_line);
    fetcher_process_ = base::SpawnMultiProcessTestChild("NetWorkerChild",
                                                        command_line, options);
    channel.RemoteProcessLaunchAttempted();
    ASSERT_TRUE(fetcher_process_.IsValid());

    mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
        channel.TakeLocalEndpoint(), {}, fetcher_process_.Handle());
    mojo::PendingRemote<mojom::FetchService> pending_remote(
        std::move(pipe), mojom::FetchService::Version_);
    ASSERT_TRUE(pending_remote);
    remote_ = mojo::Remote<mojom::FetchService>(std::move(pending_remote));
  }

  void TearDown() override {
    remote_.reset();
    test::WaitForProcess(fetcher_process_);
    fetcher_process_.Close();
  }

  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
  mojo::Remote<mojom::FetchService> remote_;
  base::Process fetcher_process_;
};

TEST_F(AppNetWorkerTest, PostRequest) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content("hello world!");
        http_response->set_content_type("text/plain");
        http_response->AddCustomHeader(
            update_client::NetworkFetcher::kHeaderEtag, "etag-for-test");
        http_response->AddCustomHeader(
            update_client::NetworkFetcher::kHeaderXCupServerProof,
            "cup-server-proof-xyz");
        http_response->AddCustomHeader(
            update_client::NetworkFetcher::kHeaderSetCookie,
            "cookie-for-testing");
        http_response->AddCustomHeader("SomeOtherHeader", "foo-bar");
        return http_response;
      }));
  ASSERT_TRUE(test_server.Start());

  base::RunLoop run_loop;
  std::vector<mojom::HttpHeaderPtr> headers;
  headers.push_back(mojom::HttpHeader::New("CustomizedHeader", "abc"));
  remote_->PostRequest(
      test_server.GetURL("/"), "", "text/plain", std::move(headers),
      MakePostRequestObserver(
          base::BindRepeating(
              [](int32_t http_status_code, int64_t content_length) {
                EXPECT_EQ(http_status_code, net::HTTP_OK);
                EXPECT_EQ(content_length, 12);  // Length of `hello world!`
              }),
          base::BindRepeating([](int64_t current) { EXPECT_LE(current, 12); }),
          base::BindLambdaForTesting(
              [&](std::optional<std::string> response_body, int32_t net_error,
                  const std::string& header_etag,
                  const std::string& header_x_cup_server_proof,
                  const std::string& header_set_cookie,
                  int64_t xheader_retry_after_sec) {
                EXPECT_EQ(net_error, 0);
                EXPECT_EQ(*response_body, "hello world!");
                EXPECT_EQ(header_etag, "etag-for-test");
                EXPECT_EQ(header_x_cup_server_proof, "cup-server-proof-xyz");
                EXPECT_EQ(header_set_cookie, "cookie-for-testing");
                run_loop.Quit();
              })));
  run_loop.Run();
}

TEST_F(AppNetWorkerTest, DownloadFile) {
  base::FilePath payload_path = updater::test::GetTestFilePath("signed.exe.gz");
  std::optional<int64_t> payload_size = base::GetFileSize(payload_path);
  ASSERT_TRUE(payload_size.has_value());
  std::string payload;
  ASSERT_TRUE(base::ReadFileToString(payload_path, &payload));
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content(payload);
        http_response->set_content_type("application/octet-stream");
        return http_response;
      }));
  ASSERT_TRUE(test_server.Start());

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);

  base::RunLoop run_loop;

  base::RepeatingClosure barrier =
      base::BarrierClosure(2, run_loop.QuitClosure());

  remote_->DownloadToStream(
      test_server.GetURL("/"), std::move(producer),
      MakeFileDownloadObserver(
          base::BindLambdaForTesting(
              [&](int32_t http_status_code, int64_t content_length) {
                EXPECT_EQ(http_status_code, net::HTTP_OK);
                if (content_length > 0) {
                  EXPECT_EQ(content_length, payload_size.value());
                }
              }),
          base::BindLambdaForTesting(
              [&](int32_t net_error, int64_t content_length) {
                EXPECT_EQ(net_error, 0);
                EXPECT_EQ(content_length, payload_size.value());
                barrier.Run();
              })));

  TestDrainer drainer(std::move(consumer), barrier);
  run_loop.Run();

  EXPECT_EQ(drainer.data(), payload);
}

TEST_F(AppNetWorkerTest, DownloadFileRecoversFromFullPipe) {
  std::string payload(5000, 'A');
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content(payload);
        http_response->set_content_type("application/octet-stream");
        return http_response;
      }));
  ASSERT_TRUE(test_server.Start());

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  constexpr static size_t kPipeCapacity = 1024;
  ASSERT_EQ(mojo::CreateDataPipe(kPipeCapacity, producer, consumer),
            MOJO_RESULT_OK);

  base::RunLoop run_loop;

  base::RepeatingClosure barrier =
      base::BarrierClosure(2, run_loop.QuitClosure());

  base::RunLoop response_wait_loop;
  bool response_started = false;

  remote_->DownloadToStream(
      test_server.GetURL("/"), std::move(producer),
      MakeFileDownloadObserver(
          base::BindLambdaForTesting(
              [&](int32_t http_status_code, int64_t content_length) {
                EXPECT_EQ(http_status_code, net::HTTP_OK);
                EXPECT_EQ(content_length, 5000);
                response_started = true;
                response_wait_loop.Quit();
              }),
          base::BindLambdaForTesting(
              [&](int32_t net_error, int64_t content_length) {
                EXPECT_EQ(net_error, 0);
                EXPECT_EQ(content_length, 5000);
                barrier.Run();
              })));

  response_wait_loop.Run();
  EXPECT_TRUE(response_started);

  // Wait until the writer in the child process has filled the pipe.
  while (true) {
    size_t bytes_available = 0;
    MojoResult result = consumer->ReadData(
        MOJO_READ_DATA_FLAG_QUERY, base::span<uint8_t>(), bytes_available);
    if (result == MOJO_RESULT_OK && bytes_available >= kPipeCapacity) {
      break;
    }
    base::RunLoop loop;
    base::ThreadPool::PostDelayedTask(FROM_HERE, loop.QuitClosure(),
                                      base::Milliseconds(10));
    loop.Run();
  }

  TestDrainer drainer(std::move(consumer), barrier);
  run_loop.Run();

  EXPECT_EQ(drainer.data(), payload);
}

TEST_F(AppNetWorkerTest, DownloadMultiple) {
  base::FilePath payload_path = updater::test::GetTestFilePath("signed.exe.gz");
  std::optional<int64_t> payload_size = base::GetFileSize(payload_path);
  ASSERT_TRUE(payload_size.has_value());
  std::string payload;
  ASSERT_TRUE(base::ReadFileToString(payload_path, &payload));

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        if (request.relative_url == "/payload") {
          http_response->set_content(payload);
        }
        http_response->set_content_type("application/octet-stream");
        return http_response;
      }));
  ASSERT_TRUE(test_server.Start());

  for (int i = 0; i < 2; ++i) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer),
              MOJO_RESULT_OK);

    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());

    remote_->DownloadToStream(
        test_server.GetURL("/payload"), std::move(producer),
        MakeFileDownloadObserver(
            base::BindLambdaForTesting(
                [&](int32_t http_status_code, int64_t content_length) {
                  EXPECT_EQ(http_status_code, net::HTTP_OK);
                  if (content_length > 0) {
                    EXPECT_EQ(content_length, payload_size.value());
                  }
                }),
            base::BindLambdaForTesting(
                [&](int32_t net_error, int64_t content_length) {
                  EXPECT_EQ(net_error, 0);
                  EXPECT_EQ(content_length, payload_size.value());
                  barrier.Run();
                })));

    TestDrainer drainer(std::move(consumer), barrier);
    loop.Run();
    EXPECT_EQ(drainer.data(), payload);
  }
}

TEST_F(AppNetWorkerTest, ServerNotExist) {
  base::RunLoop run_loop;
  remote_->PostRequest(
      GURL("https://host_that.does_not_exist"), "", "text/plain", {},
      MakePostRequestObserver(
          base::DoNothing(), base::DoNothing(),
          base::BindLambdaForTesting(
              [&](std::optional<std::string> response_body, int32_t net_error,
                  const std::string& header_etag,
                  const std::string& header_x_cup_server_proof,
                  const std::string& header_set_cookie,
                  int64_t xheader_retry_after_sec) {
                EXPECT_NE(net_error, 0);
                run_loop.Quit();
              })));
  run_loop.Run();
}

MULTIPROCESS_TEST_MAIN(NetWorkerChild) {
  base::test::TaskEnvironment task_environment;
  ScopedIPCSupportWrapper ipc_support;

  // This is a net-worker process, mark it as such, so that it doesn't do
  // further fallback.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kNetWorkerSwitch);
  return !MakeAppNetWorker()->Run();
}

}  // namespace updater
