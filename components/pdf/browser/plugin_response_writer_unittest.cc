// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/plugin_response_writer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/pdf/browser/mock_url_loader_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::HasSubstr;
using ::testing::NiceMock;

class BodyDrainer {
 public:
  explicit BodyDrainer(mojo::ScopedDataPipeConsumerHandle body)
      : drainer_(&drainer_client_, std::move(body)) {}

  const std::string& content() const { return drainer_client_.content; }

  void WaitComplete() {
    if (drainer_client_.is_complete)
      return;

    base::RunLoop run_loop;
    ASSERT_FALSE(drainer_client_.quit_closure);
    drainer_client_.quit_closure = run_loop.QuitClosure();
    run_loop.Run();

    EXPECT_TRUE(drainer_client_.is_complete);
  }

 private:
  struct DrainerClient : public mojo::DataPipeDrainer::Client {
    void OnDataAvailable(const void* data, size_t num_bytes) override {
      content.append(reinterpret_cast<const char*>(data), num_bytes);
    }

    void OnDataComplete() override {
      is_complete = true;
      if (quit_closure)
        std::move(quit_closure).Run();
    }

    std::string content;
    bool is_complete = false;
    base::OnceClosure quit_closure;
  };

  DrainerClient drainer_client_;
  mojo::DataPipeDrainer drainer_;
};

class PluginResponseWriterTest : public testing::Test {
 protected:
  PluginResponseWriterTest() {
    ON_CALL(mock_client_, OnStartLoadingResponseBody)
        .WillByDefault([this](mojo::ScopedDataPipeConsumerHandle body) {
          body_drainer_ = std::make_unique<BodyDrainer>(std::move(body));
        });
  }

  std::unique_ptr<PluginResponseWriter> NewPluginResponseWriter(
      const GURL& source_url,
      const GURL& original_url) {
    return std::make_unique<PluginResponseWriter>(
        source_url, original_url, client_receiver_.BindNewPipeAndPassRemote());
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockURLLoaderClient> mock_client_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{
      &mock_client_};

  std::unique_ptr<BodyDrainer> body_drainer_;
};

}  // namespace

TEST_F(PluginResponseWriterTest, Start) {
  auto response_writer =
      NewPluginResponseWriter(GURL("chrome-extension://id/stream-url"),
                              GURL("https://example.test/fake.pdf"));

  base::RunLoop run_loop;

  {
    // Note that `URLLoaderClient` operations are received on a separate
    // sequence, and only are ordered with respect to each other.
    testing::InSequence in_sequence;

    EXPECT_CALL(mock_client_, OnReceiveResponse)
        .WillOnce([](network::mojom::URLResponseHeadPtr head) {
          EXPECT_EQ(200, head->headers->response_code());
          EXPECT_EQ("text/html", head->mime_type);
        });

    EXPECT_CALL(mock_client_, OnStartLoadingResponseBody);

    EXPECT_CALL(mock_client_, OnComplete)
        .WillOnce(
            [&run_loop](const network::URLLoaderCompletionStatus& status) {
              EXPECT_EQ(net::OK, status.error_code);
              run_loop.Quit();
            });
  }

  // Note that `done_callback` is not ordered with respect to the
  // `URLLoaderClient` operations.
  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run);

  response_writer->Start(done_callback.Get());
  run_loop.Run();

  // Waiting for `URLLoaderClient::OnComplete()` ensures `body_drainer_` is set,
  // but the data pipe may still have unread data.
  ASSERT_TRUE(body_drainer_);
  body_drainer_->WaitComplete();

  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("src=\"chrome-extension://id/stream-url\""));
  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("original-url=\"https://example.test/fake.pdf\""));
  EXPECT_THAT(body_drainer_->content(), HasSubstr("'chrome-extension://id/'"));
}

TEST_F(PluginResponseWriterTest, StartWithUnescapedUrls) {
  auto response_writer =
      NewPluginResponseWriter(GURL("chrome-extension://id/stream-url\""),
                              GURL("https://example.test/\"fake.pdf"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_, OnComplete).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  response_writer->Start(base::DoNothing());
  run_loop.Run();

  // Waiting for `URLLoaderClient::OnComplete()` ensures `body_drainer_` is set,
  // but the data pipe may still have unread data.
  ASSERT_TRUE(body_drainer_);
  body_drainer_->WaitComplete();

  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("src=\"chrome-extension://id/stream-url%22\""));
  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("original-url=\"https://example.test/%22fake.pdf\""));
  EXPECT_THAT(body_drainer_->content(), HasSubstr("'chrome-extension://id/'"));
}

TEST_F(PluginResponseWriterTest, StartForPrintPreview) {
  auto response_writer =
      NewPluginResponseWriter(GURL("chrome://print/1/0/print.pdf"),
                              GURL("chrome://print/1/0/print.pdf"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_, OnComplete).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  response_writer->Start(base::DoNothing());
  run_loop.Run();

  // Waiting for `URLLoaderClient::OnComplete()` ensures `body_drainer_` is set,
  // but the data pipe may still have unread data.
  ASSERT_TRUE(body_drainer_);
  body_drainer_->WaitComplete();

  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("src=\"chrome://print/1/0/print.pdf\""));
  EXPECT_THAT(body_drainer_->content(),
              HasSubstr("original-url=\"chrome://print/1/0/print.pdf\""));
  EXPECT_THAT(body_drainer_->content(), HasSubstr("'chrome://print/'"));
}

}  // namespace pdf
