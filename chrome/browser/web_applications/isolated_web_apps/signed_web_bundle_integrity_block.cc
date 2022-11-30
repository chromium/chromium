// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_integrity_block.h"

#include "base/strings/stringprintf.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"

namespace web_app {

SignedWebBundleIntegrityBlock::~SignedWebBundleIntegrityBlock() = default;

base::expected<SignedWebBundleIntegrityBlock, std::string>
SignedWebBundleIntegrityBlock::Create(
    web_package::mojom::BundleIntegrityBlockPtr integrity_block) {
  if (integrity_block->size == 0) {
    return base::unexpected("Cannot create integrity block with a size of 0.");
  }
  if (integrity_block->signature_stack.empty()) {
    return base::unexpected(
        "Cannot create an integrity block without any signatures.");
  }

  std::vector<SignedWebBundleSignatureStackEntry> signature_stack;
  for (const auto& raw_entry : integrity_block->signature_stack) {
    auto entry = SignedWebBundleSignatureStackEntry::Create(raw_entry->Clone());
    if (!entry.has_value()) {
      return base::unexpected(
          base::StringPrintf("Error while parsing signature stack entry: %s",
                             entry.error().c_str()));
    }
    signature_stack.push_back(*entry);
  }

  return SignedWebBundleIntegrityBlock(integrity_block->size,
                                       std::move(signature_stack));
}

const std::vector<web_package::Ed25519PublicKey>
SignedWebBundleIntegrityBlock::GetPublicKeyStack() const {
  std::vector<web_package::Ed25519PublicKey> public_key_stack;
  public_key_stack.reserve(signature_stack_.size());
  base::ranges::transform(signature_stack_,
                          std::back_inserter(public_key_stack),
                          [](const auto& entry) { return entry.public_key(); });
  return public_key_stack;
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    const uint64_t size,
    std::vector<SignedWebBundleSignatureStackEntry>&& signature_stack)
    : size_(size), signature_stack_(std::move(signature_stack)) {
  CHECK_GT(size_, 0ul);
  CHECK(!signature_stack_.empty());
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    SignedWebBundleIntegrityBlock&&) = default;
SignedWebBundleIntegrityBlock& SignedWebBundleIntegrityBlock::operator=(
    SignedWebBundleIntegrityBlock&&) = default;

}  // namespace web_app
