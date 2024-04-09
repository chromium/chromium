// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_package {

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const std::vector<uint8_t>& complete_entry_cbor,
    const std::vector<uint8_t>& attributes_cbor,
    const SignedWebBundleSignature signature)
    : complete_entry_cbor_(complete_entry_cbor),
      attributes_cbor_(attributes_cbor),
      signature_(std::move(signature)) {}

bool SignedWebBundleSignatureStackEntry::operator==(
    const SignedWebBundleSignatureStackEntry& other) const {
  return complete_entry_cbor_ == other.complete_entry_cbor_ &&
         attributes_cbor_ == other.attributes_cbor_ &&
         signature_ == other.signature_;
}

bool SignedWebBundleSignatureStackEntry::operator!=(
    const SignedWebBundleSignatureStackEntry& other) const {
  return !operator==(other);
}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry&
SignedWebBundleSignatureStackEntry::operator=(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry::~SignedWebBundleSignatureStackEntry() =
    default;

SignedWebBundleSignatureEd25519::SignedWebBundleSignatureEd25519(
    Ed25519PublicKey public_key,
    Ed25519Signature signature)
    : public_key_(std::move(public_key)), signature_(std::move(signature)) {}

}  // namespace web_package
