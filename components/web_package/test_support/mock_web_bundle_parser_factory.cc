// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "components/web_package/test_support/mock_web_bundle_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

MockWebBundleParserFactory::MockWebBundleParserFactory() = default;

MockWebBundleParserFactory::~MockWebBundleParserFactory() = default;

void MockWebBundleParserFactory::AddReceiver(
    mojo::PendingReceiver<mojom::WebBundleParserFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockWebBundleParserFactory::WaitUntilParseIntegrityBlockCalled(
    base::OnceClosure closure) {
  if (parser_)
    parser_->WaitUntilParseIntegrityBlockCalled(std::move(closure));
  else
    wait_parse_integrity_block_callback_ = std::move(closure);
}

void MockWebBundleParserFactory::WaitUntilParseMetadataCalled(
    base::OnceCallback<void(int64_t offset)> callback) {
  if (parser_)
    parser_->WaitUntilParseMetadataCalled(std::move(callback));
  else
    wait_parse_metadata_callback_ = std::move(callback);
}

void MockWebBundleParserFactory::RunIntegrityBlockCallback(
    mojom::BundleIntegrityBlockPtr integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  base::RunLoop run_loop;
  WaitUntilParseIntegrityBlockCalled(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(parser_);
  parser_->RunIntegrityBlockCallback(std::move(integrity_block),
                                     std::move(error));
}

void MockWebBundleParserFactory::RunMetadataCallback(
    int64_t expected_metadata_offset,
    mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  base::test::TestFuture<int64_t> future;
  WaitUntilParseMetadataCalled(future.GetCallback());
  EXPECT_EQ(expected_metadata_offset, future.Get());

  ASSERT_TRUE(parser_);
  parser_->RunMetadataCallback(std::move(metadata), std::move(error));
}

void MockWebBundleParserFactory::RunResponseCallback(
    mojom::BundleResponseLocationPtr expected_parse_args,
    mojom::BundleResponsePtr response,
    mojom::BundleResponseParseErrorPtr error) {
  ASSERT_TRUE(parser_);
  base::test::TestFuture<mojom::BundleResponseLocationPtr> future;
  parser_->WaitUntilParseResponseCalled(future.GetCallback());
  auto parse_args = future.Take();
  EXPECT_EQ(expected_parse_args->offset, parse_args->offset);
  EXPECT_EQ(expected_parse_args->length, parse_args->length);
  parser_->RunResponseCallback(std::move(response), std::move(error));
}

void MockWebBundleParserFactory::GetParserForFile(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    base::File file) {
  parser_ = std::make_unique<MockWebBundleParser>(std::move(receiver));
  if (!wait_parse_integrity_block_callback_.is_null()) {
    parser_->WaitUntilParseIntegrityBlockCalled(
        std::move(wait_parse_integrity_block_callback_));
  }
  if (!wait_parse_metadata_callback_.is_null()) {
    parser_->WaitUntilParseMetadataCalled(
        std::move(wait_parse_metadata_callback_));
  }
}

void MockWebBundleParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  NOTREACHED();
}

}  // namespace web_package
