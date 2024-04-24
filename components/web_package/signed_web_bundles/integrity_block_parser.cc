// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/web_bundle_parser.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

namespace {

using SignatureType = mojom::SignatureInfo::Tag;

constexpr auto kPublicKeyAttributeNameToSignatureType =
    base::MakeFixedFlatMap<std::string_view, SignatureType>({
        // clang-format off
        {kEd25519PublicKeyAttributeName, SignatureType::kEd25519},
        {kEcdsaP256PublicKeyAttributeName, SignatureType::kEcdsaP256Sha256},
        // clang-format on
    });

SignatureType GetSignatureType(std::string_view attribute_name) {
  auto* signature_type =
      base::FindOrNull(kPublicKeyAttributeNameToSignatureType, attribute_name);
  return signature_type ? *signature_type : SignatureType::kUnknown;
}

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

  const uint64_t offset_in_stream = input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(&IntegrityBlockParser::ParseSignatureStack,
                     weak_factory_.GetWeakPtr(), offset_in_stream));
}

void IntegrityBlockParser::ParseSignatureStack(
    uint64_t offset_in_stream,
    const std::optional<std::vector<uint8_t>>& data) {
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

  if (*signature_stack_size == 0) {
    RunErrorCallback(
        "The signature stack must contain at least one signature.");
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
    const std::optional<std::vector<uint8_t>>& data) {
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

  base::Extend(signature_stack_entry->complete_entry_cbor,
               base::span(*data).first(input.CurrentOffset()));
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
    const std::optional<std::vector<uint8_t>>& data) {
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
  auto current_entry = base::span(*data).first(input.CurrentOffset());
  base::Extend(signature_stack_entry->complete_entry_cbor, current_entry);
  base::Extend(signature_stack_entry->attributes_cbor, current_entry);

  offset_in_stream += input.CurrentOffset();
  data_source_->get()->Read(
      offset_in_stream, kMaxCBORItemHeaderSize,
      base::BindOnce(
          &IntegrityBlockParser::ParseSignatureStackEntryAttributesKey,
          weak_factory_.GetWeakPtr(), offset_in_stream,
          signature_stack_entries_left, std::move(signature_stack_entry)));
}

void IntegrityBlockParser::ParseSignatureStackEntryAttributesKey(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const std::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's attributes header.");
    return;
  }

  InputReader input(*data);

  const auto attribute_name_size = input.ReadCBORHeader(CBORType::kTextString);
  if (!attribute_name_size.has_value()) {
    RunErrorCallback("The value of the attribute name must be a text string.");
    return;
  }

  auto span = base::span(*data).first(input.CurrentOffset());
  base::Extend(signature_stack_entry->complete_entry_cbor, span);
  base::Extend(signature_stack_entry->attributes_cbor, span);
  offset_in_stream += input.CurrentOffset();

  data_source_->get()->Read(
      offset_in_stream, *attribute_name_size + kMaxCBORItemHeaderSize,
      base::BindOnce(&IntegrityBlockParser::
                         ParseSignatureStackEntryAttributesPublicKeyName,
                     weak_factory_.GetWeakPtr(), offset_in_stream,
                     signature_stack_entries_left,
                     std::move(signature_stack_entry), *attribute_name_size));
}

void IntegrityBlockParser::ParseSignatureStackEntryAttributesPublicKeyName(
    uint64_t offset_in_stream,
    const uint64_t signature_stack_entries_left,
    mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
    const uint64_t attribute_name_length,
    const std::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading signature stack entry's public key attribute name.");
    return;
  }

  InputReader input(*data);

  const auto attribute_name = input.ReadString(attribute_name_length);
  if (!attribute_name) {
    RunErrorCallback(
        "Error reading signature stack entry's ed25519PublicKey attribute.");
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
  auto current_entry = base::span(*data).first(input.CurrentOffset());
  base::Extend(signature_stack_entry->complete_entry_cbor, current_entry);
  base::Extend(signature_stack_entry->attributes_cbor, current_entry);

  offset_in_stream += input.CurrentOffset();

  switch (GetSignatureType(*attribute_name)) {
    case SignatureType::kEd25519: {
      signature_stack_entry->signature_info =
          mojom::SignatureInfo::NewEd25519(mojom::SignatureInfoEd25519::New());
    } break;
    case SignatureType::kEcdsaP256Sha256: {
      signature_stack_entry->signature_info =
          mojom::SignatureInfo::NewEcdsaP256Sha256(
              mojom::SignatureInfoEcdsaP256SHA256::New());
    } break;
    case SignatureType::kUnknown: {
      // Unknown signature cipher type.
      if (signature_stack_.size() == 0) {
        RunErrorCallback("Unknown cipher type of the first signature.");
        return;
      }

      signature_stack_entry->signature_info =
          mojom::SignatureInfo::NewUnknown(mojom::SignatureInfoUnknown::New());
    } break;
  }
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
    const std::optional<std::vector<uint8_t>>& public_key_bytes) {
  if (!public_key_bytes) {
    RunErrorCallback("Error reading signature stack entry's public key.");
    return;
  }

  auto& signature_info = signature_stack_entry->signature_info;
  switch (signature_info->which()) {
    case SignatureType::kEd25519: {
      ASSIGN_OR_RETURN(
          signature_stack_entry->signature_info->get_ed25519()->public_key,
          Ed25519PublicKey::Create(*public_key_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });
    } break;
    case SignatureType::kEcdsaP256Sha256: {
      ASSIGN_OR_RETURN(
          signature_info->get_ecdsa_p256_sha256()->public_key,
          EcdsaP256PublicKey::Create(*public_key_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });
    } break;
    case SignatureType::kUnknown:
      break;
  }

  // Keep track of the raw CBOR bytes of both the complete signature stack entry
  // and its attributes.
  base::Extend(signature_stack_entry->complete_entry_cbor, *public_key_bytes);
  base::Extend(signature_stack_entry->attributes_cbor, *public_key_bytes);

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
    const std::optional<std::vector<uint8_t>>& data) {
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

  auto& signature_info = signature_stack_entry->signature_info;
  switch (signature_info->which()) {
    case SignatureType::kEd25519: {
      if (*signature_length != ED25519_SIGNATURE_LEN) {
        RunErrorCallback(base::StringPrintf(
            "The signature does not have the correct length, "
            "expected %u bytes.",
            ED25519_SIGNATURE_LEN));
        return;
      }
    } break;
    // No restrictions on other signature types.
    case SignatureType::kEcdsaP256Sha256:
    case SignatureType::kUnknown:
      break;
  }

  // Keep track of the raw CBOR bytes of the complete signature stack entry.
  auto current_entry = base::span(*data).first(input.CurrentOffset());
  base::Extend(signature_stack_entry->complete_entry_cbor, current_entry);

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
    const std::optional<std::vector<uint8_t>>& signature_bytes) {
  if (!signature_bytes.has_value()) {
    RunErrorCallback("Error reading signature-stack entry signature.");
    return;
  }

  auto& signature_info = signature_stack_entry->signature_info;
  switch (signature_info->which()) {
    case SignatureType::kEd25519: {
      ASSIGN_OR_RETURN(
          signature_stack_entry->signature_info->get_ed25519()->signature,
          Ed25519Signature::Create(*signature_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });
    } break;
    case SignatureType::kEcdsaP256Sha256: {
      ASSIGN_OR_RETURN(
          signature_stack_entry->signature_info->get_ecdsa_p256_sha256()
              ->signature,
          EcdsaP256SHA256Signature::Create(*signature_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });
    } break;
    case SignatureType::kUnknown:
      break;
  }

  // Keep track of the raw CBOR bytes of the complete signature stack entry.
  base::Extend(signature_stack_entry->complete_entry_cbor, *signature_bytes);

  signature_stack_.emplace_back(std::move(signature_stack_entry));

  offset_in_stream += signature_bytes->size();

  ProcessNextSignatureBlock(offset_in_stream, signature_stack_entries_left);
}

void IntegrityBlockParser::ProcessNextSignatureBlock(
    uint64_t offset_in_stream,
    uint64_t signature_stack_entries_left) {
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
