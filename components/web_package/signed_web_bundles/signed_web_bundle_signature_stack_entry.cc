// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include "components/web_package/mojom/web_bundle_parser.mojom.h"

namespace web_package {

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const std::vector<uint8_t>& complete_entry_cbor,
    const std::vector<uint8_t>& attributes_cbor,
    const Ed25519PublicKey& public_key,
    const Ed25519Signature& signature)
    : complete_entry_cbor_(complete_entry_cbor),
      attributes_cbor_(attributes_cbor),
      public_key_(public_key),
      signature_(signature) {}

bool SignedWebBundleSignatureStackEntry::operator==(
    const SignedWebBundleSignatureStackEntry& other) const {
  return complete_entry_cbor_ == other.complete_entry_cbor_ &&
         attributes_cbor_ == other.attributes_cbor_ &&
         public_key_ == other.public_key_ && signature_ == other.signature_;
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

}  // namespace web_package
