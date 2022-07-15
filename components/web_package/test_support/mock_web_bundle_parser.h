// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace web_package {

class MockWebBundleParser final : public mojom::WebBundleParser {
 public:
  explicit MockWebBundleParser(
      mojo::PendingReceiver<mojom::WebBundleParser> receiver);

  MockWebBundleParser(const MockWebBundleParser&) = delete;
  MockWebBundleParser& operator=(const MockWebBundleParser&) = delete;

  ~MockWebBundleParser() override;

  void RunMetadataCallback(mojom::BundleMetadataPtr metadata);
  void RunResponseCallback(mojom::BundleResponsePtr response);

  void WaitUntilParseMetadataCalled(base::OnceClosure closure);

  void WaitUntilParseResponseCalled(
      base::OnceCallback<void(mojom::BundleResponseLocationPtr)> callback);

 private:
  // mojom::WebBundleParser implementation.
  void ParseIntegrityBlock(ParseIntegrityBlockCallback callback) override;
  void ParseMetadata(int64_t offset, ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;

  mojo::Receiver<mojom::WebBundleParser> receiver_;

  ParseMetadataCallback metadata_callback_;
  ParseResponseCallback response_callback_;
  mojom::BundleResponseLocationPtr parse_response_args_;
  base::OnceClosure wait_parse_metadata_callback_;
  base::OnceCallback<void(mojom::BundleResponseLocationPtr)>
      wait_parse_response_callback_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_H_
