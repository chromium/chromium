// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/types.h"

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

  // Reading the maximal size is safe because:
  //  1. `Read` just return everything when the size of a bundle is shorter
  //  2. CBOR parser (inside) read the CBOR structure fragment by fragment until
  //  reaches its end, remaining part is ignored
  data_source_->Read(0, kMaxIntegrityBlockSize,
                     base::BindOnce(&IntegrityBlockParser::OnIntegrityBlockRead,
                                    weak_factory_.GetWeakPtr()));
}

void IntegrityBlockParser::OnIntegrityBlockRead(
    const std::optional<BinaryData>& data) {
  if (!data) {
    RunErrorCallback("Error reading the integrity block.");
    return;
  }
  // This structure is built during parsing and returned by the final callback
  mojom::BundleIntegrityBlockPtr integrity_block =
      mojom::BundleIntegrityBlock::New();

  size_t consumed_bytes = 0;
  cbor::Reader::DecoderError error;
  cbor::Reader::Config config;
  config.num_bytes_consumed = &consumed_bytes;
  config.error_code_out = &error;

  // When config `bytes_consumed` are assigned the CBOR parser reads only to the
  // end of a structure. It parses only Integrity Block even if Web Bundle part
  // starts just after.
  std::optional<cbor::Value> value = cbor::Reader::Read(*data, config);
  if (!value) {
    RunErrorCallback("Error parsing integrity block as CBOR: " +
                     std::string(cbor::Reader::ErrorCodeToString(error)));
    return;
  }
  integrity_block->size = consumed_bytes;

  if (!value->is_array()) {
    RunErrorCallback("Integrity block is not a CBOR array.");
    return;
  }

  const cbor::Value::ArrayValue& top_array = value->GetArray();
  if (top_array.size() != kIntegrityBlockV2TopLevelArrayLength) {
    RunErrorCallback(base::StringPrintf(
        "Invalid integrity block array length: expected %u, got %zu.",
        kIntegrityBlockV2TopLevelArrayLength, top_array.size()));
    return;
  }

  // 1. Magic bytes
  if (!top_array[0].is_bytestring() ||
      !std::ranges::equal(top_array[0].GetBytestring(),
                          kIntegrityBlockMagicBytes)) {
    RunErrorCallback("Unexpected magic bytes.");
    return;
  }

  // 2. Version
  if (!top_array[1].is_bytestring() ||
      !std::ranges::equal(top_array[1].GetBytestring(),
                          kIntegrityBlockV2VersionBytes)) {
    RunErrorCallback("Unexpected version bytes.",
                     mojom::BundleParseErrorType::kVersionError);
    return;
  }

  // 3. Attributes
  if (!top_array[2].is_map()) {
    RunErrorCallback("Integrity block attributes must be a map.");
    return;
  }
  const cbor::Value::MapValue& attributes_map = top_array[2].GetMap();
  const cbor::Value* web_bundle_id =
      base::FindOrNull(attributes_map, cbor::Value(kWebBundleIdAttributeName));
  if (!web_bundle_id || !web_bundle_id->is_string()) {
    RunErrorCallback(
        "`webBundleId` integrity block attribute is missing or malformed.");
    return;
  }

  // Check if web_bundle_id is correct
  RETURN_IF_ERROR(SignedWebBundleId::Create(web_bundle_id->GetString()),
                  [&](const std::string& error) { RunErrorCallback(error); });

  BinaryData attributes_cbor = *cbor::Writer::Write(top_array[2]);
  integrity_block->attributes = IntegrityBlockAttributes(
      web_bundle_id->GetString(), std::move(attributes_cbor));

  // 4. Signature Stack
  if (!top_array[3].is_array()) {
    RunErrorCallback("Signature stack must be an array.");
    return;
  }
  const cbor::Value::ArrayValue& signatures = top_array[3].GetArray();
  if (signatures.empty()) {
    RunErrorCallback(
        "The signature stack must contain at least one signature.");
    return;
  }

  for (const auto& signature_info_raw : signatures) {
    ASSIGN_OR_RETURN(
        auto parsed_signature_info, ParseSignatureInfo(signature_info_raw),
        [&](const std::string& error) { this->RunErrorCallback(error); });

    if (integrity_block->signature_stack.empty() &&
        parsed_signature_info->signature_info->is_unknown()) {
      RunErrorCallback("Unknown cipher type of the first signature.");
      return;
    }
    integrity_block->signature_stack.push_back(
        std::move(parsed_signature_info));
  }

  std::move(complete_callback_)
      .Run(base::BindOnce(std::move(result_callback_),
                          std::move(integrity_block), nullptr));
}

base::expected<mojom::BundleIntegrityBlockSignatureStackEntryPtr, std::string>
IntegrityBlockParser::ParseSignatureInfo(
    const cbor::Value& signature_info_raw) {
  if (!signature_info_raw.is_array() ||
      signature_info_raw.GetArray().size() != 2) {
    return base::unexpected(
        "Each signature stack entry must contain exactly two elements.");
  }

  const cbor::Value::ArrayValue& signature_info_array =
      signature_info_raw.GetArray();
  if (!signature_info_array[0].is_map() ||
      !signature_info_array[1].is_bytestring()) {
    return base::unexpected("Malformed signature stack entry.");
  }

  mojom::BundleIntegrityBlockSignatureStackEntryPtr parsed_signature_info =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  parsed_signature_info->attributes_cbor =
      *cbor::Writer::Write(signature_info_array[0]);

  const cbor::Value::MapValue& signature_attributes =
      signature_info_array[0].GetMap();
  const BinaryData& signature_bytes = signature_info_array[1].GetBytestring();

  const cbor::Value* ed25519_key = base::FindOrNull(
      signature_attributes, cbor::Value(kEd25519PublicKeyAttributeName));
  const cbor::Value* ecdsa_key = base::FindOrNull(
      signature_attributes, cbor::Value(kEcdsaP256PublicKeyAttributeName));

  if (ed25519_key && ecdsa_key) {
    return base::unexpected("Multiple key types for one signature.");
  } else if (ed25519_key && !ed25519_key->is_bytestring()) {
    return base::unexpected("Invalid ED25519 key.");
  } else if (ecdsa_key && !ecdsa_key->is_bytestring()) {
    return base::unexpected("Invalid ECDSA key.");

  } else if (ed25519_key && ed25519_key->is_bytestring()) {
    ASSIGN_OR_RETURN(auto public_key,
                     Ed25519PublicKey::Create(ed25519_key->GetBytestring()));
    ASSIGN_OR_RETURN(auto signature, Ed25519Signature::Create(signature_bytes));
    parsed_signature_info->signature_info = mojom::SignatureInfo::NewEd25519(
        mojom::SignatureInfoEd25519::New(public_key, signature));

  } else if (ecdsa_key && ecdsa_key->is_bytestring()) {
    ASSIGN_OR_RETURN(auto public_key,
                     EcdsaP256PublicKey::Create(ecdsa_key->GetBytestring()));
    ASSIGN_OR_RETURN(auto signature,
                     EcdsaP256SHA256Signature::Create(signature_bytes));
    parsed_signature_info->signature_info =
        mojom::SignatureInfo::NewEcdsaP256Sha256(
            mojom::SignatureInfoEcdsaP256SHA256::New(public_key, signature));

  } else {
    parsed_signature_info->signature_info =
        mojom::SignatureInfo::NewUnknown(mojom::SignatureInfoUnknown::New());
  }

  return parsed_signature_info;
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
