// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include "base/strings/stringprintf.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"

namespace web_package {

// static
base::expected<SignedWebBundleSignatureStackEntry, std::string>
SignedWebBundleSignatureStackEntry::Create(
    const mojom::BundleIntegrityBlockSignatureStackEntryPtr entry) {
  return SignedWebBundleSignatureStackEntry(
      entry->complete_entry_cbor, entry->attributes_cbor, entry->public_key,
      entry->signature);
}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const std::vector<uint8_t>& complete_entry_cbor,
    const std::vector<uint8_t>& attributes_cbor,
    const Ed25519PublicKey& public_key,
    const Ed25519Signature& signature)
    : complete_entry_cbor_(complete_entry_cbor),
      attributes_cbor_(attributes_cbor),
      public_key_(public_key),
      signature_(signature) {}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry::~SignedWebBundleSignatureStackEntry() =
    default;

}  // namespace web_package
