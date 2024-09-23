// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_MOJOM_WEB_BUNDLE_PARSER_MOJOM_TRAITS_H_
#define COMPONENTS_WEB_PACKAGE_MOJOM_WEB_BUNDLE_PARSER_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-shared.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"

namespace mojo {

template <>
class StructTraits<web_package::mojom::Ed25519PublicKeyDataView,
                   web_package::Ed25519PublicKey> {
 public:
  static base::span<const uint8_t, web_package::Ed25519PublicKey::kLength>
  bytes(const web_package::Ed25519PublicKey& public_key) {
    return public_key.bytes();
  }

  static bool Read(web_package::mojom::Ed25519PublicKeyDataView data,
                   web_package::Ed25519PublicKey* public_key);
};

template <>
class StructTraits<web_package::mojom::Ed25519SignatureDataView,
                   web_package::Ed25519Signature> {
 public:
  static base::span<const uint8_t, web_package::Ed25519Signature::kLength>
  bytes(const web_package::Ed25519Signature& signature) {
    return signature.bytes();
  }

  static bool Read(web_package::mojom::Ed25519SignatureDataView data,
                   web_package::Ed25519Signature* signature);
};

template <>
class StructTraits<web_package::mojom::EcdsaP256PublicKeyDataView,
                   web_package::EcdsaP256PublicKey> {
 public:
  static base::span<const uint8_t, web_package::EcdsaP256PublicKey::kLength>
  bytes(const web_package::EcdsaP256PublicKey& public_key) {
    return public_key.bytes();
  }

  static bool Read(web_package::mojom::EcdsaP256PublicKeyDataView data,
                   web_package::EcdsaP256PublicKey* public_key);
};

template <>
class StructTraits<web_package::mojom::EcdsaP256SHA256SignatureDataView,
                   web_package::EcdsaP256SHA256Signature> {
 public:
  static base::span<const uint8_t> bytes(
      const web_package::EcdsaP256SHA256Signature& signature) {
    return signature.bytes();
  }

  static bool Read(web_package::mojom::EcdsaP256SHA256SignatureDataView data,
                   web_package::EcdsaP256SHA256Signature* signature);
};

template <>
class StructTraits<web_package::mojom::BundleIntegrityBlockAttributesDataView,
                   web_package::IntegrityBlockAttributes> {
 public:
  static base::span<const uint8_t> cbor(
      const web_package::IntegrityBlockAttributes& attributes) {
    return attributes.cbor();
  }
  static const std::string& web_bundle_id(
      const web_package::IntegrityBlockAttributes& attributes) {
    return attributes.web_bundle_id();
  }

  static bool Read(
      web_package::mojom::BundleIntegrityBlockAttributesDataView data,
      web_package::IntegrityBlockAttributes* attributes);
};

}  // namespace mojo

#endif  // COMPONENTS_WEB_PACKAGE_MOJOM_WEB_BUNDLE_PARSER_MOJOM_TRAITS_H_
