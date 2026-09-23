// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/uploader_test_utils.h"

#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

class DataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeReader(mojo::ScopedDataPipeConsumerHandle data_pipe_consumer,
                 base::test::TestFuture<std::string>& future)
      : future_(future), drainer_(this, std::move(data_pipe_consumer)) {}

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    body_.append(base::as_string_view(data));
  }
  void OnDataComplete() override { future_->SetValue(std::move(body_)); }

 private:
  const raw_ref<base::test::TestFuture<std::string>> future_;
  std::string body_;
  mojo::DataPipeDrainer drainer_;
};

std::string ReadDataPipe(
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer) {
  CHECK(data_pipe_consumer.is_valid());

  // Nestable tasks are allowed because callers may read from the pipe within
  // a mock callback while an outer test RunLoop is active.
  base::test::TestFuture<std::string> future;
  DataPipeReader reader(std::move(data_pipe_consumer), future);
  CHECK(future.Wait(base::RunLoop::Type::kNestableTasksAllowed));
  return future.Take();
}

}  // namespace

std::string GetBodyFromFileOrPageRequest(
    ConnectorDataPipeGetter* data_pipe_getter) {
  EXPECT_TRUE(data_pipe_getter);

  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;

  base::RunLoop run_loop;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                 data_pipe_consumer));
  // Read data from `data_pipe_getter` and write it to `data_pipe_producer`.
  data_pipe_getter->Read(
      std::move(data_pipe_producer),
      base::BindLambdaForTesting([&run_loop](int32_t status, uint64_t size) {
        EXPECT_EQ(net::OK, status);
        run_loop.Quit();
      }));
  run_loop.Run();

  return ReadDataPipe(std::move(data_pipe_consumer));
}

std::string GetBodyFromResourceRequestBody(
    const network::ResourceRequestBody& request_body) {
  std::string body;
  for (const auto& element : *request_body.elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        body.append(element.As<network::DataElementBytes>().AsStringView());
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        mojo::Remote<network::mojom::DataPipeGetter> remote(
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter());
        mojo::ScopedDataPipeProducerHandle data_pipe_producer;
        mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
        EXPECT_EQ(MOJO_RESULT_OK,
                  mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                       data_pipe_consumer));
        base::RunLoop run_loop;
        remote->Read(std::move(data_pipe_producer),
                     base::BindLambdaForTesting(
                         [&run_loop](int32_t status, uint64_t size) {
                           EXPECT_EQ(net::OK, status);
                           run_loop.Quit();
                         }));
        run_loop.Run();
        body.append(ReadDataPipe(std::move(data_pipe_consumer)));
        break;
      }
      default:
        // TODO(crbug.com/473031850): Add coverage for other request body
        // element types.
        NOTREACHED();
    }
  }
  return body;
}

}  // namespace enterprise_connectors
