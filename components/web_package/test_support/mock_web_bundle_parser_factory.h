// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/test_support/mock_web_bundle_parser.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/gurl.h"

namespace web_package {

// There are two main ways tests can interact with this class. The more verbose
// way, which is most useful in unit tests, involves calling the
// `WaitUntilParse*Called`, followed by `Run*Callback`. This allows the test to
// verify both the arguments that the parser was called with, as well as to set
// the parse results.
// The second way to use this class are the `Set*ParseResult` methods. These
// must be called before the parser is used, and will cause the parser to return
// the values passed to the setters. This is most useful in browser tests, where
// the test does not have fine-grained control over when the the parser is used.
class MockWebBundleParserFactory final : public mojom::WebBundleParserFactory {
 public:
  explicit MockWebBundleParserFactory(
      base::RepeatingCallback<void(std::optional<GURL>)> on_create_parser =
          base::DoNothing());

  MockWebBundleParserFactory(const MockWebBundleParserFactory&) = delete;
  MockWebBundleParserFactory& operator=(const MockWebBundleParserFactory&) =
      delete;

  ~MockWebBundleParserFactory() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::WebBundleParserFactory> receiver);

  void WaitUntilParseIntegrityBlockCalled(base::OnceClosure closure);
  void WaitUntilParseMetadataCalled(
      base::OnceCallback<void(std::optional<uint64_t> offset)> callback);

  void RunIntegrityBlockCallback(
      mojom::BundleIntegrityBlockPtr integrity_block,
      mojom::BundleIntegrityBlockParseErrorPtr error = nullptr);
  void RunMetadataCallback(std::optional<uint64_t> expected_metadata_offset,
                           mojom::BundleMetadataPtr metadata,
                           mojom::BundleMetadataParseErrorPtr error = nullptr);
  void RunResponseCallback(mojom::BundleResponseLocationPtr expected_parse_args,
                           mojom::BundleResponsePtr response,
                           mojom::BundleResponseParseErrorPtr error = nullptr);

  void SetIntegrityBlockParseResult(
      mojom::BundleIntegrityBlockPtr integrity_block,
      mojom::BundleIntegrityBlockParseErrorPtr error = nullptr);
  void SetMetadataParseResult(
      mojom::BundleMetadataPtr metadata,
      web_package::mojom::BundleMetadataParseErrorPtr error = nullptr);
  void SetResponseParseResult(
      mojom::BundleResponsePtr response,
      mojom::BundleResponseParseErrorPtr error = nullptr);

  int GetParserCreationCount() const;

  void SimulateParserDisconnect();
  void SimulateParseIntegrityBlockCrash();
  void SimulateParseMetadataCrash();
  void SimulateParseResponseCrash();

 private:
  void GetParser(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                 const std::optional<GURL>& base_url);

  // mojom::WebBundleParserFactory implementation.
  void BindFileDataSource(
      mojo::PendingReceiver<mojom::BundleDataSource> data_source_receiver,
      base::File file) override;
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::WebBundleParser> receiver,
      const std::optional<GURL>& base_url,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override;

  base::RepeatingCallback<void(std::optional<GURL>)> on_create_parser_;
  std::unique_ptr<MockWebBundleParser> parser_;
  int parser_creation_count_ = 0;
  bool simulate_parse_integrity_block_crash_ = false;
  bool simulate_parse_metadata_crash_ = false;
  bool simulate_parse_response_crash_ = false;

  mojo::ReceiverSet<mojom::WebBundleParserFactory> receivers_;
  base::OnceClosure wait_parse_integrity_block_callback_;
  base::OnceCallback<void(std::optional<uint64_t> offset)>
      wait_parse_metadata_callback_;

  std::optional<std::pair<mojom::BundleIntegrityBlockPtr,
                          mojom::BundleIntegrityBlockParseErrorPtr>>
      integrity_block_parse_result_ = std::nullopt;
  std::optional<std::pair<mojom::BundleMetadataPtr,
                          web_package::mojom::BundleMetadataParseErrorPtr>>
      metadata_parse_result_ = std::nullopt;
  std::optional<
      std::pair<mojom::BundleResponsePtr, mojom::BundleResponseParseErrorPtr>>
      response_parse_result_ = std::nullopt;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_MOCK_WEB_BUNDLE_PARSER_FACTORY_H_
