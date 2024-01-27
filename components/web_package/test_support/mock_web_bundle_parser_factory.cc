// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "components/web_package/test_support/mock_web_bundle_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_package {

MockWebBundleParserFactory::MockWebBundleParserFactory(
    base::RepeatingCallback<void(std::optional<GURL>)> on_create_parser)
    : on_create_parser_(std::move(on_create_parser)) {}

MockWebBundleParserFactory::~MockWebBundleParserFactory() = default;

void MockWebBundleParserFactory::AddReceiver(
    mojo::PendingReceiver<mojom::WebBundleParserFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockWebBundleParserFactory::WaitUntilParseIntegrityBlockCalled(
    base::OnceClosure closure) {
  if (parser_) {
    parser_->WaitUntilParseIntegrityBlockCalled(std::move(closure));
  } else {
    wait_parse_integrity_block_callback_ = std::move(closure);
  }
}

void MockWebBundleParserFactory::WaitUntilParseMetadataCalled(
    base::OnceCallback<void(std::optional<uint64_t> offset)> callback) {
  if (parser_) {
    parser_->WaitUntilParseMetadataCalled(std::move(callback));
  } else {
    wait_parse_metadata_callback_ = std::move(callback);
  }
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
    std::optional<uint64_t> expected_metadata_offset,
    mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  base::test::TestFuture<std::optional<uint64_t>> future;
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

void MockWebBundleParserFactory::SetIntegrityBlockParseResult(
    mojom::BundleIntegrityBlockPtr integrity_block,
    mojom::BundleIntegrityBlockParseErrorPtr error) {
  integrity_block_parse_result_ =
      std::make_pair(std::move(integrity_block), std::move(error));
  if (parser_) {
    parser_->SetIntegrityBlockParseResult(
        integrity_block_parse_result_->first.Clone(),
        integrity_block_parse_result_->second.Clone());
  }
}

void MockWebBundleParserFactory::SetMetadataParseResult(
    mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK(!metadata.is_null());
  metadata_parse_result_ =
      std::make_pair(std::move(metadata), std::move(error));
  if (parser_) {
    parser_->SetMetadataParseResult(metadata_parse_result_->first.Clone(),
                                    metadata_parse_result_->second.Clone());
  }
}

void MockWebBundleParserFactory::SetResponseParseResult(
    mojom::BundleResponsePtr response,
    mojom::BundleResponseParseErrorPtr error) {
  response_parse_result_ =
      std::make_pair(std::move(response), std::move(error));
  if (parser_) {
    parser_->SetResponseParseResult(response_parse_result_->first.Clone(),
                                    response_parse_result_->second.Clone());
  }
}

int MockWebBundleParserFactory::GetParserCreationCount() const {
  return parser_creation_count_;
}

void MockWebBundleParserFactory::SimulateParserDisconnect() {
  parser_->SimulateDisconnect();
}

void MockWebBundleParserFactory::SimulateParseIntegrityBlockCrash() {
  simulate_parse_integrity_block_crash_ = true;
}

void MockWebBundleParserFactory::SimulateParseMetadataCrash() {
  simulate_parse_metadata_crash_ = true;
}

void MockWebBundleParserFactory::SimulateParseResponseCrash() {
  simulate_parse_response_crash_ = true;
}

void MockWebBundleParserFactory::GetParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    const std::optional<GURL>& base_url) {
  on_create_parser_.Run(base_url);

  if (parser_) {
    // If a parser existed previously, assume that it has been disconnected, and
    // copy its `wait_` callbacks over to the new instance.
    parser_ = std::make_unique<MockWebBundleParser>(
        std::move(receiver), simulate_parse_integrity_block_crash_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_,
        std::move(parser_));
  } else {
    parser_ = std::make_unique<MockWebBundleParser>(
        std::move(receiver), simulate_parse_integrity_block_crash_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_);
  }

  ++parser_creation_count_;

  if (!wait_parse_integrity_block_callback_.is_null()) {
    parser_->WaitUntilParseIntegrityBlockCalled(
        std::move(wait_parse_integrity_block_callback_));
  }
  if (!wait_parse_metadata_callback_.is_null()) {
    parser_->WaitUntilParseMetadataCalled(
        std::move(wait_parse_metadata_callback_));
  }

  if (integrity_block_parse_result_.has_value()) {
    parser_->SetIntegrityBlockParseResult(
        integrity_block_parse_result_->first.Clone(),
        integrity_block_parse_result_->second.Clone());
  }
  if (metadata_parse_result_.has_value()) {
    parser_->SetMetadataParseResult(metadata_parse_result_->first.Clone(),
                                    metadata_parse_result_->second.Clone());
  }
  if (response_parse_result_.has_value()) {
    parser_->SetResponseParseResult(response_parse_result_->first.Clone(),
                                    response_parse_result_->second.Clone());
  }
}

void MockWebBundleParserFactory::BindFileDataSource(
    mojo::PendingReceiver<mojom::BundleDataSource> data_source_receiver,
    base::File file) {}

void MockWebBundleParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    const std::optional<GURL>& base_url,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  GetParser(std::move(receiver), base_url);
}

}  // namespace web_package
