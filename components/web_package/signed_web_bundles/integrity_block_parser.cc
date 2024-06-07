// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include "base/functional/bind.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/web_bundle_parser.h"

namespace web_package {

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
  const uint64_t length = sizeof(kIntegrityBlockMagicBytes) +
                          sizeof(kIntegrityBlockVersionMagicBytes);
  data_source_->Read(
      0, length,
      base::BindOnce(&IntegrityBlockParser::ParseMagicBytesAndVersion,
                     weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::ParseMagicBytesAndVersion(
    const std::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback("Error reading integrity block magic bytes.",
                     mojom::BundleParseErrorType::kParserInternalError);
    return;
  }

  InputReader input(*data);

  // Check the magic bytes.
  const auto magic = input.ReadBytes(sizeof(kIntegrityBlockMagicBytes));
  if (!magic || !base::ranges::equal(*magic, kIntegrityBlockMagicBytes)) {
    RunErrorCallback("Wrong array size or magic bytes.");
    return;
  }

  // Let version be the result of reading 5 bytes from stream.
  const auto version =
      input.ReadBytes(sizeof(kIntegrityBlockVersionMagicBytes));
  if (!version) {
    RunErrorCallback("Cannot read version bytes.");
    return;
  }

  if (base::ranges::equal(*version, kIntegrityBlockVersionMagicBytes)) {
    signature_stack_ =
        std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>();
  } else {
    RunErrorCallback(
        "Unexpected integrity block version. Currently supported versions are: "
        "'1b\\0\\0'",
        mojom::BundleParseErrorType::kVersionError);
    return;
  }

  offset_in_stream_ = input.CurrentOffset();
  data_source_->Read(offset_in_stream_, kMaxCBORItemHeaderSize,
                     base::BindOnce(&IntegrityBlockParser::ParseSignatureStack,
                                    weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::ParseSignatureStack(
    const std::optional<std::vector<uint8_t>>& data) {
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
        SignatureStackEntryParser::ParserError> result) {
  CHECK_GT(signature_stack_entries_left_, 0u);

  if (!result.has_value()) {
    RunErrorCallback(std::move(result.error().message),
                     result.error().error_type);
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
