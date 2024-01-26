// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_

#include <optional>

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace web_package {

class MockWebBundleParser final : public mojom::WebBundleParser {
 public:
  MockWebBundleParser(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                      bool simulate_parse_integrity_block_crash,
                      bool simulate_parse_metadata_crash,
                      bool simulate_parse_response_crash);

  // Initializes a `MockWebBundleParser` as a replacement for a disconnected
  // `MockWebBundleParser` by moving its `wait_*` callbacks into the new
  // instance.
  MockWebBundleParser(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                      bool simulate_parse_integrity_block_crash,
                      bool simulate_parse_metadata_crash,
                      bool simulate_parse_response_crash,
                      std::unique_ptr<MockWebBundleParser> disconnected_parser);

  MockWebBundleParser(const MockWebBundleParser&) = delete;
  MockWebBundleParser& operator=(const MockWebBundleParser&) = delete;

  ~MockWebBundleParser() override;

  void RunIntegrityBlockCallback(
      mojom::BundleIntegrityBlockPtr integrity_block,
      mojom::BundleIntegrityBlockParseErrorPtr error = nullptr);
  void RunMetadataCallback(
      mojom::BundleMetadataPtr metadata,
      web_package::mojom::BundleMetadataParseErrorPtr error = nullptr);
  void RunResponseCallback(mojom::BundleResponsePtr response,
                           mojom::BundleResponseParseErrorPtr error = nullptr);

  void WaitUntilParseIntegrityBlockCalled(base::OnceClosure closure);
  void WaitUntilParseMetadataCalled(
      base::OnceCallback<void(std::optional<uint64_t> offset)> callback);
  void WaitUntilParseResponseCalled(
      base::OnceCallback<void(mojom::BundleResponseLocationPtr)> callback);

  void SetIntegrityBlockParseResult(
      mojom::BundleIntegrityBlockPtr integrity_block,
      mojom::BundleIntegrityBlockParseErrorPtr error = nullptr);
  void SetMetadataParseResult(
      mojom::BundleMetadataPtr metadata,
      web_package::mojom::BundleMetadataParseErrorPtr error = nullptr);
  void SetResponseParseResult(
      mojom::BundleResponsePtr response,
      mojom::BundleResponseParseErrorPtr error = nullptr);

  void SimulateDisconnect() { receiver_.reset(); }

 private:
  // mojom::WebBundleParser implementation.
  void ParseIntegrityBlock(ParseIntegrityBlockCallback callback) override;
  void ParseMetadata(std::optional<uint64_t> offset,
                     ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;
  void Close(CloseCallback callback) override;

  mojo::Receiver<mojom::WebBundleParser> receiver_;

  const bool simulate_parse_integrity_block_crash_;
  const bool simulate_parse_metadata_crash_;
  const bool simulate_parse_response_crash_;

  ParseIntegrityBlockCallback integrity_block_callback_;
  ParseMetadataCallback metadata_callback_;
  ParseResponseCallback response_callback_;

  std::optional<uint64_t> parse_metadata_args_;
  mojom::BundleResponseLocationPtr parse_response_args_;
  base::OnceClosure wait_parse_integrity_block_callback_;
  base::OnceCallback<void(std::optional<uint64_t> offset)>
      wait_parse_metadata_callback_;
  base::OnceCallback<void(mojom::BundleResponseLocationPtr)>
      wait_parse_response_callback_;

  std::optional<std::pair<mojom::BundleIntegrityBlockPtr,
                          mojom::BundleIntegrityBlockParseErrorPtr>>
      integrity_block_parse_result_;
  std::optional<std::pair<mojom::BundleMetadataPtr,
                          web_package::mojom::BundleMetadataParseErrorPtr>>
      metadata_parse_result_;
  std::optional<
      std::pair<mojom::BundleResponsePtr, mojom::BundleResponseParseErrorPtr>>
      response_parse_result_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_
