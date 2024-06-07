// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/attribute_map_parser.h"

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/signed_web_bundles/signature_entry_parser.h"

namespace web_package {

AttributeMapParser::AttributeMapParser(mojom::BundleDataSource& data_source,
                                       AttributeMapParsedCallback callback)
    : data_source_(data_source), callback_(std::move(callback)) {}

AttributeMapParser::~AttributeMapParser() = default;

void AttributeMapParser::Parse(uint64_t offset_in_stream) {
  offset_in_stream_ = offset_in_stream;

  data_source_->Read(
      offset_in_stream_, kMaxCBORItemHeaderSize,
      base::BindOnce(&AttributeMapParser::ReadAttributesMapHeader,
                     weak_factory_.GetWeakPtr()));
}

void AttributeMapParser::ReadAttributesMapHeader(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's attributes header.");
    return;
  }

  std::optional<uint64_t> attributes_map_length = ReadCborData(
      *data,
      [](InputReader* input) { return input->ReadCBORHeader(CBORType::kMap); });

  if (!attributes_map_length) {
    RunErrorCallback(
        "Cannot parse the size of signature stack entry's attributes.");
    return;
  };

  attributes_entries_left_ = *attributes_map_length;

  ReadNextAttributeEntry();
}

void AttributeMapParser::ReadNextAttributeEntry() {
  CHECK_GE(attributes_entries_left_, 0ul);

  if (attributes_entries_left_ > 0) {
    --attributes_entries_left_;
    data_source_->Read(
        offset_in_stream_, kMaxCBORItemHeaderSize,
        base::BindOnce(&AttributeMapParser::ReadAttributeNameCborHeader,
                       weak_factory_.GetWeakPtr()));
  } else {
    RunSuccessCallback();
  }
}

void AttributeMapParser::ReadAttributeNameCborHeader(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's attributes header.");
    return;
  }

  std::optional<uint64_t> attribute_name_size =
      ReadCborData(*data, [](InputReader* input) {
        return input->ReadCBORHeader(CBORType::kTextString);
      });
  if (!attribute_name_size) {
    RunErrorCallback("The value of the attribute name must be a text string.");
    return;
  }

  data_source_->Read(
      offset_in_stream_, *attribute_name_size,
      base::BindOnce(&AttributeMapParser::ReadAttributeName,
                     weak_factory_.GetWeakPtr(), *attribute_name_size));
}

void AttributeMapParser::ReadAttributeName(
    uint64_t attribute_name_length,
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry's attribute key.");
    return;
  }

  std::optional<std::string_view> attribute_name =
      ReadCborData(*data, [attribute_name_length](InputReader* input) {
        return input->ReadString(attribute_name_length);
      });

  if (!attribute_name) {
    RunErrorCallback("Error reading signature stack entry's attribute key.");
    return;
  }

  if (base::Contains(attributes_map_, *attribute_name)) {
    RunErrorCallback(
        base::StringPrintf("Found duplicate attribute name <%s> in signature "
                           "stack entry's attributes.",
                           std::string(*attribute_name).c_str()));
    return;
  }

  data_source_->Read(
      offset_in_stream_, kMaxCBORItemHeaderSize,
      base::BindOnce(&AttributeMapParser::ReadAttributeValueCborHeader,
                     weak_factory_.GetWeakPtr(), std::string{*attribute_name}));
}

void AttributeMapParser::ReadAttributeValueCborHeader(
    std::string attribute_name,
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry's attribute key.");
    return;
  }

  std::optional<uint64_t> attribute_value_size =
      ReadCborData(*data, [](InputReader* input) {
        return input->ReadCBORHeader(CBORType::kByteString);
      });
  if (!attribute_value_size) {
    RunErrorCallback(
        "The value of the signature stack entry attribute value must be a byte "
        "string.");
    return;
  }

  data_source_->Read(
      offset_in_stream_, *attribute_value_size,
      base::BindOnce(&AttributeMapParser::ReadAttributeValue,
                     weak_factory_.GetWeakPtr(), std::move(attribute_name)));
}

void AttributeMapParser::ReadAttributeValue(
    std::string attribute_name,
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry's public key.");
    return;
  }

  attributes_map_[std::move(attribute_name)] = *data;

  offset_in_stream_ += data->size();

  ReadNextAttributeEntry();
}

void AttributeMapParser::RunSuccessCallback() {
  std::move(callback_).Run(
      std::make_pair(std::move(attributes_map_), offset_in_stream_));
}

void AttributeMapParser::RunErrorCallback(
    const std::string& message,
    mojom::BundleParseErrorType error_type) {
  auto error = SignatureStackEntryParser::ParserError{message, error_type};
  std::move(callback_).Run(base::unexpected{error});
}

}  // namespace web_package
