// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/test_support/mock_web_bundle_parser.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace web_package {

class MockWebBundleParserFactory final : public mojom::WebBundleParserFactory {
 public:
  MockWebBundleParserFactory();

  MockWebBundleParserFactory(const MockWebBundleParserFactory&) = delete;
  MockWebBundleParserFactory& operator=(const MockWebBundleParserFactory&) =
      delete;

  ~MockWebBundleParserFactory() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::WebBundleParserFactory> receiver);

  void WaitUntilParseIntegrityBlockCalled(base::OnceClosure closure);

  void WaitUntilParseMetadataCalled(
      base::OnceCallback<void(int64_t offset)> callback);

  void RunIntegrityBlockCallback(
      mojom::BundleIntegrityBlockPtr integrity_block,
      mojom::BundleIntegrityBlockParseErrorPtr error = nullptr);

  void RunMetadataCallback(int64_t expected_metadata_offset,
                           mojom::BundleMetadataPtr metadata,
                           mojom::BundleMetadataParseErrorPtr error = nullptr);

  void RunResponseCallback(mojom::BundleResponseLocationPtr expected_parse_args,
                           mojom::BundleResponsePtr response,
                           mojom::BundleResponseParseErrorPtr error = nullptr);

 private:
  // mojom::WebBundleParserFactory implementation.
  void GetParserForFile(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                        base::File file) override;
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::WebBundleParser> receiver,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override;

  std::unique_ptr<MockWebBundleParser> parser_;
  mojo::ReceiverSet<mojom::WebBundleParserFactory> receivers_;
  base::OnceClosure wait_parse_integrity_block_callback_;
  base::OnceCallback<void(int64_t offset)> wait_parse_metadata_callback_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_
