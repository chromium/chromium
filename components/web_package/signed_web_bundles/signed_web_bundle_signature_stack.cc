// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/overloaded.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

namespace web_package {

namespace {

SignedWebBundleSignatureStackEntry CreateSignatureEntry(
    const mojom::BundleIntegrityBlockSignatureStackEntryPtr& entry) {
  const auto& signature_info = entry->signature_info;
  switch (signature_info->which()) {
    case mojom::SignatureInfo::Tag::kEd25519:
      return SignedWebBundleSignatureStackEntry(
          entry->attributes_cbor,
          SignedWebBundleSignatureInfoEd25519(
              signature_info->get_ed25519()->public_key,
              signature_info->get_ed25519()->signature));
    case mojom::SignatureInfo::Tag::kEcdsaP256Sha256:
      return SignedWebBundleSignatureStackEntry(
          entry->attributes_cbor,
          SignedWebBundleSignatureInfoEcdsaP256SHA256(
              signature_info->get_ecdsa_p256_sha256()->public_key,
              signature_info->get_ecdsa_p256_sha256()->signature));
    case mojom::SignatureInfo::Tag::kUnknown:
      return SignedWebBundleSignatureStackEntry(
          entry->attributes_cbor, SignedWebBundleSignatureInfoUnknown());
  }
}

}  // namespace

// static
base::expected<SignedWebBundleSignatureStack, std::string>
SignedWebBundleSignatureStack::Create(
    base::span<const SignedWebBundleSignatureStackEntry> entries) {
  if (entries.empty()) {
    return base::unexpected("The signature stack needs at least one entry.");
  }

  if (base::ranges::all_of(entries, [](const auto& signature) {
        return absl::holds_alternative<SignedWebBundleSignatureInfoUnknown>(
            signature.signature_info());
      })) {
    return base::unexpected(
        "There must be at least one signature of known type.");
  }

  return SignedWebBundleSignatureStack(
      std::vector(std::begin(entries), std::end(entries)));
}

// static
base::expected<SignedWebBundleSignatureStack, std::string>
SignedWebBundleSignatureStack::Create(
    std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>&&
        raw_entries) {
  return SignedWebBundleSignatureStack::Create(
      base::ToVector(raw_entries, &CreateSignatureEntry));
}

SignedWebBundleSignatureStack::SignedWebBundleSignatureStack(
    std::vector<SignedWebBundleSignatureStackEntry> entries)
    : entries_(std::move(entries)) {
  DCHECK(!entries_.empty());
}

SignedWebBundleSignatureStack::SignedWebBundleSignatureStack(
    const SignedWebBundleSignatureStack& other) = default;

SignedWebBundleSignatureStack& SignedWebBundleSignatureStack::operator=(
    const SignedWebBundleSignatureStack& other) = default;

SignedWebBundleSignatureStack::~SignedWebBundleSignatureStack() = default;

bool SignedWebBundleSignatureStack::operator==(
    const SignedWebBundleSignatureStack& other) const {
  return entries() == other.entries();
}

bool SignedWebBundleSignatureStack::operator!=(
    const SignedWebBundleSignatureStack& other) const {
  return !operator==(other);
}

std::vector<PublicKey> SignedWebBundleSignatureStack::public_keys() const {
  std::vector<PublicKey> public_keys;
  for (const auto& signature : entries()) {
    absl::visit(
        base::Overloaded{[&](const auto& signature_info) {
                           public_keys.push_back(signature_info.public_key());
                         },
                         [](const SignedWebBundleSignatureInfoUnknown&) {
                           // Unknown signatures cannot provide a public key.
                         }},
        signature.signature_info());
  }
  return public_keys;
}

}  // namespace web_package
