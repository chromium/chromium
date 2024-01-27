// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace web_package {

// This class is responsible for assigning parsing requests to the particular
// parsers.
class WebBundleParser : public mojom::WebBundleParser {
 public:
  WebBundleParser(mojo::PendingRemote<mojom::BundleDataSource> data_source,
                  GURL base_url);

  WebBundleParser(const WebBundleParser&) = delete;
  WebBundleParser& operator=(const WebBundleParser&) = delete;

  ~WebBundleParser() override;

  // This interface defines the parser of sections of the Web Bundle.
  // Please implement it if you need to create a parse of anything in the
  // Web Bundle.
  class WebBundleSectionParser {
   public:
    // The closure should contain the callback provided by the caller with
    // bound parameters that we should return to the caller.
    using ParsingCompleteCallback = base::OnceCallback<void(base::OnceClosure)>;
    virtual void StartParsing(ParsingCompleteCallback) = 0;
    virtual ~WebBundleSectionParser() = default;
  };

 private:
  class MetadataParser;
  class ResponseParser;

  // mojom::WebBundleParser implementation.
  void ParseIntegrityBlock(ParseIntegrityBlockCallback callback) override;
  void ParseMetadata(std::optional<uint64_t> offset,
                     ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;
  void Close(CloseCallback parser_closed_callback) override;
  void OnDataSourceClosed(CloseCallback parser_closed_callback);

  void ActivateParser(std::unique_ptr<WebBundleSectionParser> parser);
  void OnParsingComplete(WebBundleSectionParser* parser,
                         base::OnceClosure result_callback);
  void OnDisconnect();
  bool CheckIfClosed();

  GURL base_url_;
  base::flat_set<std::unique_ptr<WebBundleSectionParser>,
                 base::UniquePtrComparator>
      active_parsers_;
  mojo::Remote<mojom::BundleDataSource> data_source_;
  bool is_closed_ = false;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_
