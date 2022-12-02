// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/mojom/web_bundle_parser_mojom_traits.h"

#include "base/containers/span.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"

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

}  // namespace mojo
