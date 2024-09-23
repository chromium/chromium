// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/attribute_map_parser.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signature_entry_parser.h"
#include "components/web_package/signed_web_bundles/types.h"

namespace web_package {

namespace {

std::optional<base::span<const uint8_t>> ReadByteStringWithHeader(
    InputReader& input) {
  if (std::optional<uint64_t> size =
          input.ReadCBORHeader(CBORType::kByteString)) {
    return input.ReadBytes(*size);
  }
  return std::nullopt;
}

}  // namespace

IntegrityBlockParser::IntegrityBlockParser(
    mojom::BundleDataSource& data_source,
    WebBundleParser::ParseIntegrityBlockCallback callback)
    : data_source_(data_source), result_callback_(std::move(callback)) {}

IntegrityBlockParser::~IntegrityBlockParser() {
  if (!complete_callback_.is_null()) {
    RunErrorCallback("Data source disconnected.",
                     mojom::BundleParseErrorType::kParserInternalError);
  }
}

void IntegrityBlockParser::StartParsing(
    WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback callback) {
  complete_callback_ = std::move(callback);
  // First, we will parse the `magic` and `version` bytes.
  constexpr static uint64_t kMagicBytesAndVersionHeaderLength =
      /*array_header=*/kMaxCBORItemHeaderSize +
      /*magic_bytes_header=*/kMaxCBORItemHeaderSize +
      /*magic_bytes=*/kIntegrityBlockMagicBytes.size() +
      /*version_bytes_header=*/kMaxCBORItemHeaderSize +
      /*version_bytes=*/kIntegrityBlockV2VersionBytes.size();
  data_source_->Read(
      0, kMagicBytesAndVersionHeaderLength,
      base::BindOnce(&IntegrityBlockParser::ParseMagicBytesAndVersion,
                     weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::ParseMagicBytesAndVersion(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading the integrity block array structure.");
    return;
  }

  InputReader input(*data);
  std::optional<uint64_t> array_length = input.ReadCBORHeader(CBORType::kArray);
  if (!array_length) {
    RunErrorCallback("Error reading the integrity block array structure.");
    return;
  }

  if (*array_length < 2) {
    RunErrorCallback(
        "Integrity block array size is too short -- expected at least 2 "
        "elements, got " +
        base::ToString(*array_length) + ".");
    return;
  }

  if (ReadByteStringWithHeader(input) != kIntegrityBlockMagicBytes) {
    RunErrorCallback("Unexpected magic bytes.");
    return;
  }

  std::optional<base::span<const uint8_t>> version_bytes =
      ReadByteStringWithHeader(input);
  if (version_bytes == kIntegrityBlockV1VersionBytes) {
    RunErrorCallback(
        "Integrity Block V1 has been deprecated since M129. Please re-sign "
        "your bundle.");
    return;
  }
  if (version_bytes != kIntegrityBlockV2VersionBytes) {
    RunErrorCallback("Unexpected version bytes: expected `2b\\0\\0` (for v2).",
                     mojom::BundleParseErrorType::kVersionError);
    return;
  }
  if (*array_length != kIntegrityBlockV2TopLevelArrayLength) {
    RunErrorCallback("Integrity block array of length " +
                     base::ToString(*array_length) + " - should be " +
                     base::ToString(kIntegrityBlockV2TopLevelArrayLength) +
                     ".");
    return;
  }
  offset_in_stream_ = input.CurrentOffset();
  ReadAttributes();
}

void IntegrityBlockParser::ReadAttributes() {
  attributes_parser_ = std::make_unique<AttributeMapParser>(
      *data_source_, base::BindOnce(&IntegrityBlockParser::ParseAttributes,
                                    weak_factory_.GetWeakPtr()));
  attributes_parser_->Parse(offset_in_stream_);
}

void IntegrityBlockParser::ParseAttributes(
    AttributeMapParser::ParsingResult result) {
  ASSIGN_OR_RETURN(
      (auto [attributes_map, offset_to_end_of_map]), std::move(result),
      [&](std::string error) { RunErrorCallback(std::move(error)); });

  const cbor::Value* web_bundle_id =
      base::FindOrNull(attributes_map, kWebBundleIdAttributeName);
  if (!web_bundle_id || !web_bundle_id->is_string() ||
      web_bundle_id->GetString().empty()) {
    RunErrorCallback(
        "`webBundleId` field in integrity block attributes is missing or "
        "malformed.");
    return;
  }

  uint64_t attribute_map_size = offset_to_end_of_map - offset_in_stream_;
  data_source_->Read(
      offset_in_stream_, attribute_map_size,
      base::BindOnce(&IntegrityBlockParser::ReadAttributesBytes,
                     weak_factory_.GetWeakPtr(), web_bundle_id->GetString()));
}

void IntegrityBlockParser::ReadAttributesBytes(
    std::string web_bundle_id,
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading integrity block attributes.");
    return;
  }

  attributes_ = IntegrityBlockAttributes(std::move(web_bundle_id), *data);
  offset_in_stream_ += data->size();

  ReadSignatureStack();
}

void IntegrityBlockParser::ReadSignatureStack() {
  data_source_->Read(offset_in_stream_, kMaxCBORItemHeaderSize,
                     base::BindOnce(&IntegrityBlockParser::ParseSignatureStack,
                                    weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::ParseSignatureStack(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack.");
    return;
  }

  InputReader input(*data);

  const auto signature_stack_size = input.ReadCBORHeader(CBORType::kArray);
  if (!signature_stack_size) {
    RunErrorCallback("Cannot parse the size of the signature stack.");
    return;
  }

  if (*signature_stack_size == 0) {
    RunErrorCallback(
        "The signature stack must contain at least one signature.");
    return;
  }

  offset_in_stream_ += input.CurrentOffset();
  signature_stack_entries_left_ = *signature_stack_size;
  ReadSignatureStackEntry();
}

void IntegrityBlockParser::ReadSignatureStackEntry() {
  current_signature_stack_entry_parser_ =
      std::make_unique<SignatureStackEntryParser>(
          *data_source_,
          base::BindOnce(&IntegrityBlockParser::NextSignatureStackEntry,
                         weak_factory_.GetWeakPtr()));

  current_signature_stack_entry_parser_->Parse(offset_in_stream_);
}

void IntegrityBlockParser::NextSignatureStackEntry(
    base::expected<
        std::pair<mojom::BundleIntegrityBlockSignatureStackEntryPtr, uint64_t>,
        std::string> result) {
  CHECK_GT(signature_stack_entries_left_, 0u);

  if (!result.has_value()) {
    RunErrorCallback(std::move(result.error()),
                     mojom::BundleParseErrorType::kFormatError);
    return;
  }

  auto [signature_entry, offset] = std::move(result.value());

  if (signature_entry->signature_info->is_unknown() &&
      signature_stack_.size() == 0) {
    RunErrorCallback("Unknown cipher type of the first signature.");
    return;
  }

  offset_in_stream_ = offset;
  signature_stack_.emplace_back(std::move(signature_entry));

  --signature_stack_entries_left_;
  if (signature_stack_entries_left_ > 0) {
    ReadSignatureStackEntry();
  } else {
    RunSuccessCallback();
  }
}

void IntegrityBlockParser::RunSuccessCallback() {
  mojom::BundleIntegrityBlockPtr integrity_block =
      mojom::BundleIntegrityBlock::New();
  integrity_block->size = offset_in_stream_;
  integrity_block->signature_stack = std::move(signature_stack_);
  integrity_block->attributes = std::move(*attributes_);

  std::move(complete_callback_)
      .Run(base::BindOnce(std::move(result_callback_),
                          std::move(integrity_block), nullptr));
}

void IntegrityBlockParser::RunErrorCallback(
    const std::string& message,
    mojom::BundleParseErrorType error_type) {
  std::move(complete_callback_)
      .Run(base::BindOnce(
          std::move(result_callback_), nullptr,
          mojom::BundleIntegrityBlockParseError::New(error_type, message)));
}

}  // namespace web_package
