// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_

#include "base/containers/flat_map.h"
#include "components/web_package/signed_web_bundles/attribute_map_parser.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signature_entry_parser.h"
#include "components/web_package/signed_web_bundles/types.h"
#include "components/web_package/web_bundle_parser.h"

namespace web_package {

class SignatureStackEntryParser;

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
  void ParseMagicBytesAndVersion(const std::optional<BinaryData>& data);

  void ReadAttributes();
  void ParseAttributes(AttributeMapParser::ParsingResult result);
  void ReadAttributesBytes(std::string web_bundle_id,
                           const std::optional<BinaryData>& data);

  void ReadSignatureStack();
  void ParseSignatureStack(const std::optional<BinaryData>& data);

  void ReadSignatureStackEntry();

  void NextSignatureStackEntry(
      base::expected<
          std::pair<mojom::BundleIntegrityBlockSignatureStackEntryPtr,
                    uint64_t>,
          std::string> result);

  void RunSuccessCallback();

  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError);

  const raw_ref<mojom::BundleDataSource> data_source_;
  WebBundleParser::ParseIntegrityBlockCallback result_callback_;
  WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback
      complete_callback_;

  std::unique_ptr<SignatureStackEntryParser>
      current_signature_stack_entry_parser_;
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      signature_stack_;
  uint64_t signature_stack_entries_left_;
  uint64_t offset_in_stream_;

  std::unique_ptr<AttributeMapParser> attributes_parser_;
  std::optional<IntegrityBlockAttributes> attributes_;

  base::WeakPtrFactory<IntegrityBlockParser> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
