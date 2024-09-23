// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/mojom/web_bundle_parser_mojom_traits.h"

#include "base/containers/span.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"

namespace mojo {

// static
bool StructTraits<web_package::mojom::Ed25519PublicKeyDataView,
                  web_package::Ed25519PublicKey>::
    Read(web_package::mojom::Ed25519PublicKeyDataView data,
         web_package::Ed25519PublicKey* public_key) {
  std::array<uint8_t, web_package::Ed25519PublicKey::kLength> bytes;
  if (!data.ReadBytes(&bytes)) {
    return false;
  }

  *public_key = web_package::Ed25519PublicKey::Create(
      base::as_bytes(base::make_span(bytes)));

  return true;
}

// static
bool StructTraits<web_package::mojom::Ed25519SignatureDataView,
                  web_package::Ed25519Signature>::
    Read(web_package::mojom::Ed25519SignatureDataView data,
         web_package::Ed25519Signature* signature) {
  std::array<uint8_t, web_package::Ed25519Signature::kLength> bytes;
  if (!data.ReadBytes(&bytes)) {
    return false;
  }

  *signature = web_package::Ed25519Signature::Create(
      base::as_bytes(base::make_span(bytes)));

  return true;
}

// static
bool StructTraits<web_package::mojom::EcdsaP256PublicKeyDataView,
                  web_package::EcdsaP256PublicKey>::
    Read(web_package::mojom::EcdsaP256PublicKeyDataView data,
         web_package::EcdsaP256PublicKey* public_key) {
  std::array<uint8_t, web_package::EcdsaP256PublicKey::kLength> bytes;
  if (!data.ReadBytes(&bytes)) {
    return false;
  }

  if (auto public_key_or_error = web_package::EcdsaP256PublicKey::Create(bytes);
      public_key_or_error.has_value()) {
    *public_key = std::move(public_key_or_error).value();
    return true;
  }
  return false;
}

// static
bool StructTraits<web_package::mojom::EcdsaP256SHA256SignatureDataView,
                  web_package::EcdsaP256SHA256Signature>::
    Read(web_package::mojom::EcdsaP256SHA256SignatureDataView data,
         web_package::EcdsaP256SHA256Signature* signature) {
  std::vector<uint8_t> bytes;
  if (!data.ReadBytes(&bytes)) {
    return false;
  }

  if (auto signature_or_error =
          web_package::EcdsaP256SHA256Signature::Create(bytes);
      signature_or_error.has_value()) {
    *signature = std::move(signature_or_error).value();
    return true;
  }
  return false;
}

// static
bool StructTraits<web_package::mojom::BundleIntegrityBlockAttributesDataView,
                  web_package::IntegrityBlockAttributes>::
    Read(web_package::mojom::BundleIntegrityBlockAttributesDataView data,
         web_package::IntegrityBlockAttributes* attributes) {
  std::vector<uint8_t> cbor;
  if (!data.ReadCbor(&cbor) || cbor.empty()) {
    return false;
  }
  std::string web_bundle_id;
  if (!data.ReadWebBundleId(&web_bundle_id) || web_bundle_id.empty()) {
    return false;
  }
  *attributes = web_package::IntegrityBlockAttributes(std::move(web_bundle_id),
                                                      std::move(cbor));
  return true;
}

}  // namespace mojo
