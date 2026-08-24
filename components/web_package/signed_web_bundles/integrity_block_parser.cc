// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_parser.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/cbor/reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/rust/signed_web_bundles_rust.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/types.h"

namespace web_package {

namespace {

using SignatureType = signed_web_bundles::rust::SignatureType::Tag;
using signed_web_bundles::rust::parse_integrity_block;

base::expected<mojom::BundleIntegrityBlockSignatureStackEntryPtr, std::string>
CreateSignatureStackEntry(
    const signed_web_bundles::rust::SignatureStackEntry& entry) {
  auto parsed_sig = mojom::BundleIntegrityBlockSignatureStackEntry::New();

  switch (entry.signature_type.tag) {
    case SignatureType::Ed25519: {
      ASSIGN_OR_RETURN(auto public_key,
                       Ed25519PublicKey::Create(entry.public_key.to_span()));
      ASSIGN_OR_RETURN(auto signature,
                       Ed25519Signature::Create(entry.signature.to_span()));
      parsed_sig->signature_info = mojom::SignatureInfo::NewEd25519(
          mojom::SignatureInfoEd25519::New(public_key, signature));
      break;
    }
    case SignatureType::EcdsaP256SHA256: {
      ASSIGN_OR_RETURN(auto public_key,
                       EcdsaP256PublicKey::Create(entry.public_key.to_span()));
      ASSIGN_OR_RETURN(auto signature, EcdsaP256SHA256Signature::Create(
                                           entry.signature.to_span()));
      parsed_sig->signature_info = mojom::SignatureInfo::NewEcdsaP256Sha256(
          mojom::SignatureInfoEcdsaP256SHA256::New(public_key, signature));
      break;
    }
    case SignatureType::Unknown: {
      parsed_sig->signature_info =
          mojom::SignatureInfo::NewUnknown(mojom::SignatureInfoUnknown::New());
      break;
    }
  }

  parsed_sig->attributes_cbor =
      BinaryData(entry.attributes_cbor.begin(), entry.attributes_cbor.end());

  return parsed_sig;
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

  const auto parse_res = parse_integrity_block(*data);
  if (!parse_res.has_value()) {
    const auto& error = parse_res.error();
    RunErrorCallback(std::string(error.message.as_str()),
                     error.is_version_error
                         ? mojom::BundleParseErrorType::kVersionError
                         : mojom::BundleParseErrorType::kFormatError);
    return;
  }

  const auto& parsed_ib = *parse_res;
  auto integrity_block = mojom::BundleIntegrityBlock::New();
  integrity_block->size = parsed_ib.size;

  const std::string_view web_bundle_id_str =
      parsed_ib.web_bundle_id.to_string_view();
  RETURN_IF_ERROR(
      SignedWebBundleId::Create(web_bundle_id_str),
      [&](std::string error) { RunErrorCallback(std::move(error)); });

  integrity_block->attributes =
      IntegrityBlockAttributes(std::string(web_bundle_id_str),
                               BinaryData(parsed_ib.attributes_cbor.begin(),
                                          parsed_ib.attributes_cbor.end()));

  for (const auto& entry : parsed_ib.signature_stack) {
    ASSIGN_OR_RETURN(
        auto parsed_sig, CreateSignatureStackEntry(entry),
        [&](std::string error) { RunErrorCallback(std::move(error)); });
    integrity_block->signature_stack.push_back(std::move(parsed_sig));
  }

  std::move(complete_callback_)
      .Run(base::BindOnce(std::move(result_callback_),
                          std::move(integrity_block), nullptr));
}

void IntegrityBlockParser::RunErrorCallback(
    std::string message,
    mojom::BundleParseErrorType error_type) {
  std::move(complete_callback_)
      .Run(base::BindOnce(std::move(result_callback_), nullptr,
                          mojom::BundleIntegrityBlockParseError::New(
                              error_type, std::move(message))));
}

}  // namespace web_package
