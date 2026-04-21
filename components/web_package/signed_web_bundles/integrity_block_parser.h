// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_

#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/types.h"
#include "components/web_package/web_bundle_parser.h"

namespace web_package {

class IntegrityBlockParser : public WebBundleParser::WebBundleSectionParser {
 public:
  explicit IntegrityBlockParser(
      mojom::BundleDataSource& data_source,
      WebBundleParser::ParseIntegrityBlockCallback callback);

  IntegrityBlockParser(const IntegrityBlockParser&) = delete;
  IntegrityBlockParser& operator=(const IntegrityBlockParser&) = delete;

  ~IntegrityBlockParser() override;

  void StartParsing(
      WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback callback)
      override;

 private:
  void OnIntegrityBlockRead(const std::optional<BinaryData>& data);

  base::expected<mojom::BundleIntegrityBlockSignatureStackEntryPtr, std::string>
  ParseSignatureInfo(const cbor::Value& attributes_map);

  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError);

  const raw_ref<mojom::BundleDataSource> data_source_;
  WebBundleParser::ParseIntegrityBlockCallback result_callback_;
  WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback
      complete_callback_;

  base::WeakPtrFactory<IntegrityBlockParser> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
