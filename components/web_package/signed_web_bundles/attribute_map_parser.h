// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ATTRIBUTE_MAP_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ATTRIBUTE_MAP_PARSER_H_

#include "components/web_package/input_reader.h"
#include "components/web_package/signed_web_bundles/signature_entry_parser.h"
#include "components/web_package/web_bundle_parser.h"

namespace web_package {

// This class is responsible for parsing the attributes map of a signature entry
// contained in the integrity block of a signed web bundle.
class AttributeMapParser {
 public:
  // In case of success the callback returns the attributes map and the offset
  // in the stream corresponding to the end of the attributes map.
  using AttributeMapParsedCallback = base::OnceCallback<void(
      base::expected<std::pair<AttributesMap, uint64_t>,
                     SignatureStackEntryParser::ParserError>)>;

  explicit AttributeMapParser(mojom::BundleDataSource& data_source,
                              AttributeMapParsedCallback callback);
  ~AttributeMapParser();

  void Parse(uint64_t offset_in_stream);

 private:
  void ReadAttributesMapHeader(const std::optional<BinaryData>& data);
  void ReadNextAttributeEntry();

  void ReadAttributeNameCborHeader(const std::optional<BinaryData>& data);
  void ReadAttributeName(uint64_t key_name_length,
                         const std::optional<BinaryData>& data);
  void ReadAttributeValueCborHeader(std::string attribute_key,
                                    const std::optional<BinaryData>& data);
  void ReadAttributeValue(std::string attribute_key,
                          const std::optional<BinaryData>& data);

  void RunSuccessCallback();
  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError);

  uint64_t offset_in_stream_;
  const raw_ref<mojom::BundleDataSource> data_source_;

  AttributesMap attributes_map_;
  uint64_t attributes_entries_left_;

  AttributeMapParsedCallback callback_;
  base::WeakPtrFactory<AttributeMapParser> weak_factory_{this};

  template <class Lambda>
  auto ReadCborData(const BinaryData& data, Lambda lambda) {
    InputReader input(data);

    const auto item = lambda(&input);

    if (item) {
      offset_in_stream_ += input.CurrentOffset();
    }

    return item;
  }
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ATTRIBUTE_MAP_PARSER_H_
