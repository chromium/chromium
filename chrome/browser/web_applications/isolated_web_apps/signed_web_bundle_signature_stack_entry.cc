// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_stack_entry.h"

#include "base/strings/stringprintf.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"

namespace web_app {

// static
base::expected<SignedWebBundleSignatureStackEntry, std::string>
SignedWebBundleSignatureStackEntry::Create(
    const web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        entry) {
  auto public_key = web_package::Ed25519PublicKey::Create(entry->public_key);
  if (!public_key.has_value()) {
    return base::unexpected(base::StringPrintf("Invalid public key: %s",
                                               public_key.error().c_str()));
  }
  auto signature = Ed25519Signature::Create(entry->signature);
  if (!signature.has_value()) {
    return base::unexpected(
        base::StringPrintf("Invalid signature: %s", signature.error().c_str()));
  }

  return SignedWebBundleSignatureStackEntry(entry->complete_entry_cbor,
                                            entry->attributes_cbor, *public_key,
                                            *signature);
}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const std::vector<uint8_t>& complete_entry_cbor,
    const std::vector<uint8_t>& attributes_cbor,
    const web_package::Ed25519PublicKey& public_key,
    const Ed25519Signature& signature)
    : complete_entry_cbor_(complete_entry_cbor),
      attributes_cbor_(attributes_cbor),
      public_key_(public_key),
      signature_(signature) {}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry::~SignedWebBundleSignatureStackEntry() =
    default;

}  // namespace web_app
