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

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
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
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

namespace updater {

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
    test::WaitForProcess(fetcher_process_);
    fetcher_process_.Close();
    remote_.reset();
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
              [&](std::unique_ptr<std::string> response_body, int32_t net_error,
                  const std::string& header_etag,
                  const std::string& header_x_cup_server_proof,
                  int64_t xheader_retry_after_sec) {
                EXPECT_EQ(net_error, 0);
                EXPECT_EQ(*response_body, "hello world!");
                EXPECT_EQ(header_etag, "etag-for-test");
                EXPECT_EQ(header_x_cup_server_proof, "cup-server-proof-xyz");
                run_loop.Quit();
              })));
  run_loop.Run();
}

TEST_F(AppNetWorkerTest, DownloadFile) {
  base::FilePath payload_path = updater::test::GetTestFilePath("signed.exe.gz");
  int64_t payload_size = {};
  ASSERT_TRUE(base::GetFileSize(payload_path, &payload_size));
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

  base::ScopedTempFile output;
  ASSERT_TRUE(output.Create());
  base::File output_file(output.path(),
                         base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(output_file.IsValid());
  base::RunLoop run_loop;
  remote_->DownloadToFile(
      test_server.GetURL("/"), std::move(output_file),
      MakeFileDownloadObserver(
          base::BindLambdaForTesting(
              [&](int32_t http_status_code, int64_t content_length) {
                EXPECT_EQ(http_status_code, net::HTTP_OK);
                if (content_length > 0) {
                  EXPECT_EQ(content_length, payload_size);
                }
              }),
          base::BindLambdaForTesting(
              [&](int64_t current) { EXPECT_LE(current, payload_size); }),
          base::BindLambdaForTesting(
              [&](int32_t net_error, int64_t content_length) {
                EXPECT_EQ(net_error, 0);
                EXPECT_EQ(content_length, payload_size);
                run_loop.Quit();
              })));
  run_loop.Run();
  EXPECT_TRUE(base::ContentsEqual(output.path(), payload_path));
}

TEST_F(AppNetWorkerTest, ServerNotExist) {
  base::RunLoop run_loop;
  remote_->PostRequest(
      GURL("https://host_that.does_not_exist"), "", "text/plain", {},
      MakePostRequestObserver(
          base::BindRepeating(
              [](int32_t http_status_code, int64_t content_length) {}),
          base::BindRepeating([](int64_t current) {}),
          base::BindLambdaForTesting(
              [&](std::unique_ptr<std::string> response_body, int32_t net_error,
                  const std::string& header_etag,
                  const std::string& header_x_cup_server_proof,
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
