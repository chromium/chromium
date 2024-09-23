// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

mojo::ScopedDataPipeConsumerHandle ReadResponseBody(
    uint64_t response_length,
    base::OnceCallback<void(mojo::ScopedDataPipeProducerHandle producer_handle,
                            base::OnceCallback<void(net::Error net_error)>)>
        read_response_body_callback,
    base::OnceCallback<void(net::Error)> on_response_read_callback) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.element_num_bytes = 1;
  options.capacity_num_bytes = response_length + 1;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, producer, consumer));

  std::move(read_response_body_callback)
      .Run(std::move(producer), std::move(on_response_read_callback));
  return consumer;
}

std::string ReadAndFulfillResponseBody(
    SignedWebBundleReader& reader,
    web_package::mojom::BundleResponsePtr response) {
  uint64_t response_length = response->payload_length;
  return ReadAndFulfillResponseBody(
      response_length,
      base::BindOnce(&SignedWebBundleReader::ReadResponseBody,
                     // It is okay to use a bare pointer here, since the
                     // callback is executed synchronously.
                     base::Unretained(&reader), std::move(response)));
}

std::string ReadAndFulfillResponseBody(
    uint64_t response_length,
    base::OnceCallback<void(mojo::ScopedDataPipeProducerHandle producer_handle,
                            base::OnceCallback<void(net::Error net_error)>)>
        read_response_body_callback) {
  base::test::TestFuture<net::Error> error_future;
  mojo::ScopedDataPipeConsumerHandle consumer =
      ReadResponseBody(response_length, std::move(read_response_body_callback),
                       error_future.GetCallback());
  EXPECT_EQ(net::OK, error_future.Get());

  std::string buffer(response_length, '\0');
  size_t bytes_read = 0;
  MojoResult read_result =
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(buffer), bytes_read);
  EXPECT_EQ(MOJO_RESULT_OK, read_result);
  EXPECT_EQ(buffer.size(), bytes_read);
  return buffer.substr(0, bytes_read);
}

}  // namespace web_app
