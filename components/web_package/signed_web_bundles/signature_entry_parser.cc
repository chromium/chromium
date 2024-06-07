// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signature_entry_parser.h"

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/signed_web_bundles/attribute_map_parser.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

namespace {
using SignatureType = mojom::SignatureInfo::Tag;

std::pair<SignatureType, BinaryData> GetSignatureType(
    const AttributesMap& attributes_map) {
  const BinaryData* ed25519_key =
      base::FindOrNull(attributes_map, kEd25519PublicKeyAttributeName);
  const BinaryData* ecdsa_key =
      base::FindOrNull(attributes_map, kEcdsaP256PublicKeyAttributeName);

  if (ed25519_key && ecdsa_key) {
    // The signature type cannot be determined if the attributes map contains
    // both keys.
    return {SignatureType::kUnknown, BinaryData()};
  } else if (ecdsa_key) {
    return {SignatureType::kEcdsaP256Sha256, *ecdsa_key};
  } else if (ed25519_key) {
    return {SignatureType::kEd25519, *ed25519_key};
  } else {
    // The signature type cannot be determined neither key is present in the
    // attributes map.
    return {SignatureType::kUnknown, BinaryData()};
  };
}

}  // namespace

SignatureStackEntryParser::SignatureStackEntryParser(
    mojom::BundleDataSource& data_source,
    SignatureEntryParsedCallback callback)
    : data_source_(data_source), callback_(std::move(callback)) {}

SignatureStackEntryParser::~SignatureStackEntryParser() = default;

void SignatureStackEntryParser::Parse(uint64_t offset_in_stream) {
  offset_in_stream_ = offset_in_stream;

  data_source_->Read(
      offset_in_stream_, kMaxCBORItemHeaderSize,
      base::BindOnce(&SignatureStackEntryParser::ReadSignatureStructure,
                     weak_factory_.GetWeakPtr()));
}

void SignatureStackEntryParser::ReadSignatureStructure(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry.");
    return;
  }

  InputReader input(*data);

  // Each signature stack entry should be an array with two elements:
  // attributes and signature
  const auto array_length = input.ReadCBORHeader(CBORType::kArray);
  if (!array_length) {
    RunErrorCallback("Cannot parse the size of signature stack entry.");
    return;
  }

  if (*array_length != 2) {
    RunErrorCallback(
        "Each signature stack entry must contain exactly two elements.");
    return;
  }

  signature_stack_entry_ =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();

  base::Extend(signature_stack_entry_->complete_entry_cbor,
               base::span(*data).first(input.CurrentOffset()));
  offset_in_stream_ += input.CurrentOffset();

  attribute_map_parser_ = std::make_unique<AttributeMapParser>(
      *data_source_,
      base::BindOnce(&SignatureStackEntryParser::GetAttributesMap,
                     weak_factory_.GetWeakPtr()));

  attribute_map_parser_->Parse(offset_in_stream_);
}

void SignatureStackEntryParser::GetAttributesMap(
    base::expected<std::pair<AttributesMap, uint64_t>, ParserError> result) {
  if (!result.has_value()) {
    RunErrorCallback(std::move(result.error().message),
                     result.error().error_type);
    return;
  }

  auto [attributes_map, offset_to_end_of_map] = std::move(result.value());

  attributes_map_ = attributes_map;
  uint64_t attribute_map_size = offset_to_end_of_map - offset_in_stream_;
  data_source_->Read(
      offset_in_stream_, attribute_map_size,
      base::BindOnce(&SignatureStackEntryParser::ReadAttributesMapBytes,
                     weak_factory_.GetWeakPtr(), attribute_map_size));
}

void SignatureStackEntryParser::ReadAttributesMapBytes(
    uint64_t num_bytes,
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature stack entry.");
    return;
  }

  // Keep track of the raw CBOR bytes of both the complete signature stack entry
  // and its attributes.
  base::Extend(signature_stack_entry_->complete_entry_cbor, *data);
  base::Extend(signature_stack_entry_->attributes_cbor, *data);

  offset_in_stream_ += num_bytes;
  data_source_->Read(
      offset_in_stream_, kMaxCBORItemHeaderSize,
      base::BindOnce(&SignatureStackEntryParser::ReadSignatureHeader,
                     weak_factory_.GetWeakPtr()));
}

void SignatureStackEntryParser::ReadSignatureHeader(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback(
        "Error reading CBOR header of the signature stack entry's "
        "signature.");
    return;
  }

  InputReader input(*data);

  const auto signature_length = input.ReadCBORHeader(CBORType::kByteString);
  if (!signature_length) {
    RunErrorCallback(
        "Cannot parse the size of signature stack entry's signature.");
    return;
  }

  // Keep track of the raw CBOR bytes of the complete signature stack entry.
  base::Extend(signature_stack_entry_->complete_entry_cbor,
               base::span(*data).first(input.CurrentOffset()));

  offset_in_stream_ += input.CurrentOffset();
  data_source_->Read(
      offset_in_stream_, *signature_length,
      base::BindOnce(&SignatureStackEntryParser::ReadSignatureValue,
                     weak_factory_.GetWeakPtr()));
}

void SignatureStackEntryParser::ReadSignatureValue(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading signature-stack entry signature.");
    return;
  }

  base::Extend(signature_stack_entry_->complete_entry_cbor, *data);
  offset_in_stream_ += data->size();

  EvaluateSignatureEntry(*data);
}

void SignatureStackEntryParser::EvaluateSignatureEntry(
    BinaryData signature_bytes) {
  auto [signature_type, public_key_bytes] = GetSignatureType(attributes_map_);
  switch (signature_type) {
    case SignatureType::kEd25519: {
      if (signature_bytes.size() != ED25519_SIGNATURE_LEN) {
        RunErrorCallback(base::StringPrintf(
            "The signature does not have the correct length, "
            "expected %u bytes.",
            ED25519_SIGNATURE_LEN));
        return;
      }

      ASSIGN_OR_RETURN(
          auto public_key, Ed25519PublicKey::Create(public_key_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });

      ASSIGN_OR_RETURN(
          auto signature, Ed25519Signature::Create(signature_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });

      signature_stack_entry_->signature_info = mojom::SignatureInfo::NewEd25519(
          mojom::SignatureInfoEd25519::New(public_key, signature));

    } break;
    case SignatureType::kEcdsaP256Sha256: {
      ASSIGN_OR_RETURN(
          auto public_key, EcdsaP256PublicKey::Create(public_key_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });

      ASSIGN_OR_RETURN(
          auto signature, EcdsaP256SHA256Signature::Create(signature_bytes),
          [&](std::string error) { RunErrorCallback(std::move(error)); });

      signature_stack_entry_->signature_info =
          mojom::SignatureInfo::NewEcdsaP256Sha256(
              mojom::SignatureInfoEcdsaP256SHA256::New(public_key, signature));
    } break;
    case SignatureType::kUnknown:
      // Unknown signature cipher type.
      signature_stack_entry_->signature_info =
          mojom::SignatureInfo::NewUnknown(mojom::SignatureInfoUnknown::New());

      break;
  }

  std::move(callback_).Run(
      std::make_pair(std::move(signature_stack_entry_), offset_in_stream_));
}

void SignatureStackEntryParser::RunErrorCallback(
    const std::string& message,
    mojom::BundleParseErrorType error_type) {
  auto error = ParserError{message, error_type};
  std::move(callback_).Run(base::unexpected{error});
}

}  // namespace web_package
