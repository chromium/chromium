// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/web_bundle_parser.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

namespace {

constexpr char kSignatureAttributesPublicKeyWithCBORHeader[] = {
    0x70,  // UTF-8 string of 16 bytes.
    'e',  'd', '2', '5', '5', '1', '9', 'P',
    'u',  'b', 'l', 'i', 'c', 'K', 'e', 'y',
};

}  // namespace

IntegrityBlockParser::IntegrityBlockParser(
    mojo::Remote<mojom::BundleDataSource>& data_source
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
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
  data_source_->get()->Read(
      0, length,
      base::BindOnce(&IntegrityBlockParser::ParseMagicBytesAndVersion,
                     weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::ParseMagicBytesAndVersion(
    const absl::optional<std::vector<uint8_t>>& data) {
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

  const uint64_t offset_in_stream = input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(&IntegrityBlockParser::ParseSignatureStack,
                     weak_factory_.GetWeakPtr(), offset_in_stream));
}

void IntegrityBlockParser::ParseSignatureStack(
    uint64_t offset_in_stream,
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack.");
    return;
  }

  InputReader input(*data);

  const auto signature_stack_size = input.ReadCBORHeader(CBORType::kArray);
  if (!signature_stack_size.has_value()) {
    RunErrorCallback("Cannot parse the size of the signature stack.");
    return;
  }

  if (*signature_stack_size != 1 && *signature_stack_size != 2) {
    // TODO(cmfcmf): Support more signatures for key rotation.
    RunErrorCallback(
        "The signature stack must contain one or two signatures (developer + "
        "potentially distributor signature).");
    return;
  }

  offset_in_stream += input.CurrentOffset();
  ReadSignatureStackEntry(offset_in_stream, *signature_stack_size);
}

void IntegrityBlockParser::ReadSignatureStackEntry(
    const uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left) {
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(&IntegrityBlockParser::ParseSignatureStackEntry,
                     weak_factory_.GetWeakPtr(), offset_in_stream,
                     signature_stack_entries_left));
}

void IntegrityBlockParser::ParseSignatureStackEntry(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry.");
    return;
  }

  InputReader input(*data);

  // Each signature stack entry should be an array with two elements:
  // attributes and signature
  const auto array_length = input.ReadCBORHeader(CBORType::kArray);
  if (!array_length.has_value()) {
    RunErrorCallback("Cannot parse the size of signature stack entry.");
    return;
  }

  if (*array_length != 2) {
    RunErrorCallback(
        "Each signature stack entry must contain exactly two elements.");
    return;
  }

  mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  // Start to keep track of the complete CBOR bytes of the signature stack
  // entry.
  signature_stack_entry->complete_entry_cbor =
      std::vector(data->begin(), data->begin() + input.CurrentOffset());

  offset_in_stream += input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(
          &IntegrityBlockParser::ParseSignatureStackEntryAttributesHeader,
          weak_factory_.GetWeakPtr(), offset_in_stream,
          signature_stack_entries_left, std::move(signature_stack_entry)));
}

void IntegrityBlockParser::ParseSignatureStackEntryAttributesHeader(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's attributes header.");
    return;
  }

  InputReader input(*data);

  const auto attributes_length = input.ReadCBORHeader(CBORType::kMap);
  if (!attributes_length.has_value()) {
    RunErrorCallback(
        "Cannot parse the size of signature stack entry's attributes.");
    return;
  }

  if (*attributes_length != 1) {
    RunErrorCallback(
        "A signature stack entry's attributes must be a map with one element.");
    return;
  }

  // Keep track of the raw CBOR bytes of both the complete signature stack entry
  // and its attributes.
  signature_stack_entry->complete_entry_cbor.insert(
      signature_stack_entry->complete_entry_cbor.end(), data->begin(),
      data->begin() + input.CurrentOffset());
  signature_stack_entry->attributes_cbor.assign(
      data->begin(), data->begin() + input.CurrentOffset());

  offset_in_stream += input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream,
      sizeof(kSignatureAttributesPublicKeyWithCBORHeader) +
          kMaxCBORItemHeaderSize,
      base::BindOnce(
          &IntegrityBlockParser::ParseSignatureStackEntryAttributesPublicKeyKey,
          weak_factory_.GetWeakPtr(), offset_in_stream,
          signature_stack_entries_left, std::move(signature_stack_entry)));
}

// Parse the attribute map key of the public key attribute
void IntegrityBlockParser::ParseSignatureStackEntryAttributesPublicKeyKey(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's ed25519PublicKey attribute.");
    return;
  }

  InputReader input(*data);
  const auto attribute_name =
      input.ReadBytes(sizeof(kSignatureAttributesPublicKeyWithCBORHeader));
  if (!attribute_name) {
    RunErrorCallback(
        "Error reading signature stack entry's ed25519PublicKey attribute.");
    return;
  }

  if (!base::ranges::equal(*attribute_name,
                           kSignatureAttributesPublicKeyWithCBORHeader)) {
    RunErrorCallback(
        "The signature stack entry's attribute must have 'ed25519PublicKey' as "
        "its key.");
    return;
  }

  const auto public_key_value_size =
      input.ReadCBORHeader(CBORType::kByteString);
  if (!public_key_value_size.has_value()) {
    RunErrorCallback(
        "The value of the signature stack entry's ed25519PublicKey attribute "
        "must be a byte string.");
    return;
  }

  // Keep track of the raw CBOR bytes of both the complete signature stack entry
  // and its attributes.
  signature_stack_entry->complete_entry_cbor.insert(
      signature_stack_entry->complete_entry_cbor.end(), data->begin(),
      data->begin() + input.CurrentOffset());
  signature_stack_entry->attributes_cbor.insert(
      signature_stack_entry->attributes_cbor.end(), data->begin(),
      data->begin() + input.CurrentOffset());

  offset_in_stream += input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, *public_key_value_size,
      base::BindOnce(&IntegrityBlockParser::
                         ReadSignatureStackEntryAttributesPublicKeyValue,
                     weak_factory_.GetWeakPtr(), offset_in_stream,
                     signature_stack_entries_left,
                     std::move(signature_stack_entry)));
}

void IntegrityBlockParser::ReadSignatureStackEntryAttributesPublicKeyValue(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const absl::optional<std::vector<uint8_t>>& public_key_bytes) {
  if (!public_key_bytes) {
    RunErrorCallback("Error reading signature stack entry's public key.");
    return;
  }
  ASSIGN_OR_RETURN(
      signature_stack_entry->public_key,
      Ed25519PublicKey::Create(*public_key_bytes),
      [&](std::string error) { RunErrorCallback(std::move(error)); });

  // Keep track of the raw CBOR bytes of both the complete signature stack entry
  // and its attributes.
  signature_stack_entry->complete_entry_cbor.insert(
      signature_stack_entry->complete_entry_cbor.end(),
      public_key_bytes->begin(), public_key_bytes->end());
  signature_stack_entry->attributes_cbor.insert(
      signature_stack_entry->attributes_cbor.end(), public_key_bytes->begin(),
      public_key_bytes->end());

  offset_in_stream += public_key_bytes->size();
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(
          &IntegrityBlockParser::ParseSignatureStackEntrySignatureHeader,
          weak_factory_.GetWeakPtr(), offset_in_stream,
          signature_stack_entries_left, std::move(signature_stack_entry)));
}

void IntegrityBlockParser::ParseSignatureStackEntrySignatureHeader(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading CBOR header of the signature stack entry's signature.");
    return;
  }

  InputReader input(*data);

  const auto signature_length = input.ReadCBORHeader(CBORType::kByteString);
  if (!signature_length.has_value()) {
    RunErrorCallback(
        "Cannot parse the size of signature stack entry's signature.");
    return;
  }
  if (*signature_length != ED25519_SIGNATURE_LEN) {
    RunErrorCallback(
        base::StringPrintf("The signature does not have the correct length, "
                           "expected %u bytes.",
                           ED25519_SIGNATURE_LEN));
    return;
  }

  // Keep track of the raw CBOR bytes of the complete signature stack entry.
  signature_stack_entry->complete_entry_cbor.insert(
      signature_stack_entry->complete_entry_cbor.end(), data->begin(),
      data->begin() + input.CurrentOffset());

  offset_in_stream += input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, *signature_length,
      base::BindOnce(&IntegrityBlockParser::ParseSignatureStackEntrySignature,
                     weak_factory_.GetWeakPtr(), offset_in_stream,
                     signature_stack_entries_left,
                     std::move(signature_stack_entry)));
}

void IntegrityBlockParser::ParseSignatureStackEntrySignature(
    uint64_t offset_in_stream,
    uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const absl::optional<std::vector<uint8_t>>& signature_bytes) {
  if (!signature_bytes.has_value()) {
    RunErrorCallback("Error reading signature-stack entry signature.");
    return;
  }

  ASSIGN_OR_RETURN(
      signature_stack_entry->signature,
      Ed25519Signature::Create(*signature_bytes),
      [&](std::string error) { RunErrorCallback(std::move(error)); });

  // Keep track of the raw CBOR bytes of the complete signature stack entry.
  signature_stack_entry->complete_entry_cbor.insert(
      signature_stack_entry->complete_entry_cbor.end(),
      signature_bytes->begin(), signature_bytes->end());

  signature_stack_.emplace_back(std::move(signature_stack_entry));

  offset_in_stream += signature_bytes->size();

  DCHECK(signature_stack_entries_left > 0);
  --signature_stack_entries_left;
  if (signature_stack_entries_left > 0) {
    ReadSignatureStackEntry(offset_in_stream, signature_stack_entries_left);
  } else {
    RunSuccessCallback(offset_in_stream);
  }
}

void IntegrityBlockParser::RunSuccessCallback(const uint64_t offset_in_stream) {
  mojom::BundleIntegrityBlockPtr integrity_block =
      mojom::BundleIntegrityBlock::New();
  integrity_block->size = offset_in_stream;
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
