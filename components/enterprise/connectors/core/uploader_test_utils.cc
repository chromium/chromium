// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/uploader_test_utils.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

std::string ReadDataPipe(
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer) {
  EXPECT_TRUE(data_pipe_consumer.is_valid());
  std::string body;
  // Write data from `data_pipe_consumer` to `buffer`, and ultimately to `body`.
  while (true) {
    std::string buffer(1024, '\0');
    size_t read_size = 0;
    MojoResult result = data_pipe_consumer->ReadData(
        MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
        read_size);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    if (result != MOJO_RESULT_OK) {
      break;
    }
    body.append(std::string_view(buffer).substr(0, read_size));
  }

  return body;
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
