// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/mock_web_bundle_parser.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace web_package {

MockWebBundleParser::MockWebBundleParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver)
    : receiver_(this, std::move(receiver)) {}

MockWebBundleParser::~MockWebBundleParser() = default;

void MockWebBundleParser::RunIntegrityBlockCallback(
    mojom::BundleIntegrityBlockPtr integrity_block,
    mojom::BundleIntegrityBlockParseErrorPtr error) {
  std::move(integrity_block_callback_)
      .Run(std::move(integrity_block), std::move(error));
}

void MockWebBundleParser::RunMetadataCallback(
    mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  std::move(metadata_callback_).Run(std::move(metadata), std::move(error));
}

void MockWebBundleParser::RunResponseCallback(
    mojom::BundleResponsePtr response,
    mojom::BundleResponseParseErrorPtr error) {
  std::move(response_callback_).Run(std::move(response), std::move(error));
}

void MockWebBundleParser::WaitUntilParseIntegrityBlockCalled(
    base::OnceClosure closure) {
  if (integrity_block_callback_.is_null())
    wait_parse_integrity_block_callback_ = std::move(closure);
  else
    std::move(closure).Run();
}

void MockWebBundleParser::WaitUntilParseMetadataCalled(
    base::OnceCallback<void(int64_t offset)> callback) {
  if (metadata_callback_.is_null())
    wait_parse_metadata_callback_ = std::move(callback);
  else
    std::move(callback).Run(parse_metadata_args_);
}

void MockWebBundleParser::WaitUntilParseResponseCalled(
    base::OnceCallback<void(mojom::BundleResponseLocationPtr)> callback) {
  if (response_callback_.is_null())
    wait_parse_response_callback_ = std::move(callback);
  else
    std::move(callback).Run(std::move(parse_response_args_));
}

void MockWebBundleParser::ParseIntegrityBlock(
    ParseIntegrityBlockCallback callback) {
  integrity_block_callback_ = std::move(callback);
  if (!wait_parse_integrity_block_callback_.is_null())
    std::move(wait_parse_integrity_block_callback_).Run();
}

void MockWebBundleParser::ParseMetadata(int64_t offset,
                                        ParseMetadataCallback callback) {
  metadata_callback_ = std::move(callback);
  parse_metadata_args_ = offset;
  if (!wait_parse_metadata_callback_.is_null())
    std::move(wait_parse_metadata_callback_).Run(parse_metadata_args_);
}

void MockWebBundleParser::ParseResponse(uint64_t response_offset,
                                        uint64_t response_length,
                                        ParseResponseCallback callback) {
  response_callback_ = std::move(callback);
  parse_response_args_ =
      mojom::BundleResponseLocation::New(response_offset, response_length);
  if (!wait_parse_response_callback_.is_null()) {
    std::move(wait_parse_response_callback_)
        .Run(std::move(parse_response_args_));
  }
}

}  // namespace web_package
