// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

namespace web_package {

namespace {

SignedWebBundleSignatureStackEntry CreateSignatureEntry(
    const mojom::BundleIntegrityBlockSignatureStackEntryPtr& entry) {
  if (entry->signature_info->is_ed25519()) {
    auto const& signature = entry->signature_info->get_ed25519();

    return SignedWebBundleSignatureStackEntry(
        entry->complete_entry_cbor, entry->attributes_cbor,
        SignedWebBundleSignatureEd25519(signature->public_key,
                                        signature->signature));
  } else {
    return SignedWebBundleSignatureStackEntry(
        entry->complete_entry_cbor, entry->attributes_cbor,
        SignedWebBundleSignatureUnknown());
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

}  // namespace web_package
