// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_

#include "components/web_package/web_bundle_parser.h"

namespace web_package {

class AttributeMapParser;

using BinaryData = std::vector<uint8_t>;
using AttributesMap = base::flat_map<std::string, BinaryData>;

// This class is responsible for parsing a single signature entry from the
// signature stack of the integrity block of a signed web bundle.
class SignatureStackEntryParser {
 public:
  struct ParserError {
    const std::string message;
    mojom::BundleParseErrorType error_type;
  };

  // In case of success the callback returns the signature stack entry and the
  // offset in the stream corresponding to the end of the entry.
  using SignatureEntryParsedCallback = base::OnceCallback<void(
      base::expected<
          std::pair<mojom::BundleIntegrityBlockSignatureStackEntryPtr,
                    uint64_t>,
          ParserError>)>;

  explicit SignatureStackEntryParser(mojom::BundleDataSource& data_source,
                                     SignatureEntryParsedCallback callback);

  ~SignatureStackEntryParser();

  void Parse(uint64_t offset);

 private:
  void ReadSignatureStructure(const std::optional<BinaryData>& data);
  void GetAttributesMap(
      base::expected<std::pair<AttributesMap, uint64_t>, ParserError> result);
  void ReadAttributesMapBytes(uint64_t num_bytes,
                              const std::optional<BinaryData>& data);
  void ReadSignatureHeader(const std::optional<BinaryData>& data);
  void ReadSignatureValue(const std::optional<BinaryData>& data);
  void EvaluateSignatureEntry(BinaryData data);
  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError);

  mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry_;
  AttributesMap attributes_map_;
  std::unique_ptr<AttributeMapParser> attribute_map_parser_;

  uint64_t offset_in_stream_;
  const raw_ref<mojom::BundleDataSource> data_source_;

  SignatureEntryParsedCallback callback_;
  base::WeakPtrFactory<SignatureStackEntryParser> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_
