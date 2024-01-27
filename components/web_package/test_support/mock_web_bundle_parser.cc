// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/mock_web_bundle_parser.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace web_package {

MockWebBundleParser::MockWebBundleParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    bool simulate_parse_integrity_block_crash,
    bool simulate_parse_metadata_crash,
    bool simulate_parse_response_crash)
    : receiver_(this, std::move(receiver)),
      simulate_parse_integrity_block_crash_(
          simulate_parse_integrity_block_crash),
      simulate_parse_metadata_crash_(simulate_parse_metadata_crash),
      simulate_parse_response_crash_(simulate_parse_response_crash) {}

MockWebBundleParser::MockWebBundleParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    bool simulate_parse_integrity_block_crash,
    bool simulate_parse_metadata_crash,
    bool simulate_parse_response_crash,
    std::unique_ptr<MockWebBundleParser> disconnected_parser)
    : receiver_(this, std::move(receiver)),
      simulate_parse_integrity_block_crash_(
          simulate_parse_integrity_block_crash),
      simulate_parse_metadata_crash_(simulate_parse_metadata_crash),
      simulate_parse_response_crash_(simulate_parse_response_crash),
      wait_parse_integrity_block_callback_(
          std::move(disconnected_parser->wait_parse_integrity_block_callback_)),
      wait_parse_metadata_callback_(
          std::move(disconnected_parser->wait_parse_metadata_callback_)),
      wait_parse_response_callback_(
          std::move(disconnected_parser->wait_parse_response_callback_)) {}

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
  if (integrity_block_callback_.is_null()) {
    wait_parse_integrity_block_callback_ = std::move(closure);
  } else {
    std::move(closure).Run();
  }
}

void MockWebBundleParser::WaitUntilParseMetadataCalled(
    base::OnceCallback<void(std::optional<uint64_t> offset)> callback) {
  if (metadata_callback_.is_null()) {
    wait_parse_metadata_callback_ = std::move(callback);
  } else {
    std::move(callback).Run(parse_metadata_args_);
  }
}

void MockWebBundleParser::WaitUntilParseResponseCalled(
    base::OnceCallback<void(mojom::BundleResponseLocationPtr)> callback) {
  if (response_callback_.is_null()) {
    wait_parse_response_callback_ = std::move(callback);
  } else {
    std::move(callback).Run(std::move(parse_response_args_));
  }
}

void MockWebBundleParser::SetIntegrityBlockParseResult(
    mojom::BundleIntegrityBlockPtr integrity_block,
    mojom::BundleIntegrityBlockParseErrorPtr error) {
  integrity_block_parse_result_ =
      std::make_pair(std::move(integrity_block), std::move(error));
}

void MockWebBundleParser::SetMetadataParseResult(
    mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  metadata_parse_result_ =
      std::make_pair(std::move(metadata), std::move(error));
}

void MockWebBundleParser::SetResponseParseResult(
    mojom::BundleResponsePtr response,
    mojom::BundleResponseParseErrorPtr error) {
  response_parse_result_ =
      std::make_pair(std::move(response), std::move(error));
}

void MockWebBundleParser::ParseIntegrityBlock(
    ParseIntegrityBlockCallback callback) {
  if (simulate_parse_integrity_block_crash_) {
    SimulateDisconnect();
    return;
  }

  if (integrity_block_parse_result_.has_value()) {
    std::move(callback).Run(integrity_block_parse_result_->first.Clone(),
                            integrity_block_parse_result_->second.Clone());
    return;
  }

  integrity_block_callback_ = std::move(callback);
  if (!wait_parse_integrity_block_callback_.is_null()) {
    std::move(wait_parse_integrity_block_callback_).Run();
  }
}

void MockWebBundleParser::ParseMetadata(std::optional<uint64_t> offset,
                                        ParseMetadataCallback callback) {
  if (simulate_parse_metadata_crash_) {
    SimulateDisconnect();
    return;
  }

  if (metadata_parse_result_.has_value()) {
    std::move(callback).Run(metadata_parse_result_->first.Clone(),
                            metadata_parse_result_->second.Clone());
    return;
  }

  metadata_callback_ = std::move(callback);
  parse_metadata_args_ = offset;
  if (!wait_parse_metadata_callback_.is_null()) {
    std::move(wait_parse_metadata_callback_).Run(parse_metadata_args_);
  }
}

void MockWebBundleParser::ParseResponse(uint64_t response_offset,
                                        uint64_t response_length,
                                        ParseResponseCallback callback) {
  if (simulate_parse_response_crash_) {
    SimulateDisconnect();
    return;
  }

  if (response_parse_result_.has_value()) {
    auto response = response_parse_result_->first.Clone();
    if (!response.is_null()) {
      response->payload_offset = response_offset;
      response->payload_length = response_length;
    }
    std::move(callback).Run(std::move(response),
                            response_parse_result_->second.Clone());
    return;
  }

  response_callback_ = std::move(callback);
  parse_response_args_ =
      mojom::BundleResponseLocation::New(response_offset, response_length);
  if (!wait_parse_response_callback_.is_null()) {
    std::move(wait_parse_response_callback_)
        .Run(std::move(parse_response_args_));
  }
}

void MockWebBundleParser::Close(CloseCallback callback) {
  std::move(callback).Run();
}

}  // namespace web_package
