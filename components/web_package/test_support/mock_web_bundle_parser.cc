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

void MockWebBundleParser::RunMetadataCallback(
    mojom::BundleMetadataPtr metadata) {
  std::move(metadata_callback_).Run(std::move(metadata), nullptr);
}

void MockWebBundleParser::RunResponseCallback(
    mojom::BundleResponsePtr response) {
  std::move(response_callback_).Run(std::move(response), nullptr);
}

void MockWebBundleParser::WaitUntilParseMetadataCalled(
    base::OnceClosure closure) {
  if (metadata_callback_.is_null())
    wait_parse_metadata_callback_ = std::move(closure);
  else
    std::move(closure).Run();
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
  NOTREACHED();
}

void MockWebBundleParser::ParseMetadata(int64_t offset,
                                        ParseMetadataCallback callback) {
  metadata_callback_ = std::move(callback);
  if (!wait_parse_metadata_callback_.is_null())
    std::move(wait_parse_metadata_callback_).Run();
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
