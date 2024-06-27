// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

#include <string>
#include <utility>

#include "base/functional/overloaded.h"
#include "base/types/expected_macros.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

SignedWebBundleIntegrityBlock::~SignedWebBundleIntegrityBlock() = default;

base::expected<SignedWebBundleIntegrityBlock, std::string>
SignedWebBundleIntegrityBlock::Create(
    mojom::BundleIntegrityBlockPtr integrity_block) {
  if (integrity_block->size == 0) {
    return base::unexpected("Cannot create integrity block with a size of 0.");
  }

  ASSIGN_OR_RETURN(auto signature_stack,
                   SignedWebBundleSignatureStack::Create(
                       std::move(integrity_block->signature_stack)),
                   [](std::string error) {
                     return "Cannot create an integrity block: " +
                            std::move(error);
                   });

  if (integrity_block->attributes) {
    RETURN_IF_ERROR(SignedWebBundleId::Create(
        integrity_block->attributes->web_bundle_id()));
  }

  return SignedWebBundleIntegrityBlock(integrity_block->size,
                                       std::move(signature_stack),
                                       std::move(integrity_block->attributes));
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    const uint64_t size_in_bytes,
    SignedWebBundleSignatureStack&& signature_stack,
    std::optional<IntegrityBlockAttributes> attributes)
    : size_in_bytes_(size_in_bytes),
      signature_stack_(signature_stack),
      attributes_(std::move(attributes)) {
  CHECK_GT(size_in_bytes_, 0ul);
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    const SignedWebBundleIntegrityBlock&) = default;
SignedWebBundleIntegrityBlock& SignedWebBundleIntegrityBlock::operator=(
    const SignedWebBundleIntegrityBlock&) = default;

bool SignedWebBundleIntegrityBlock::operator==(
    const SignedWebBundleIntegrityBlock& other) const {
  return size_in_bytes_ == other.size_in_bytes_ &&
         signature_stack_ == other.signature_stack_ &&
         attributes_ == other.attributes_;
}

bool SignedWebBundleIntegrityBlock::operator!=(
    const SignedWebBundleIntegrityBlock& other) const {
  return !operator==(other);
}

SignedWebBundleId SignedWebBundleIntegrityBlock::web_bundle_id() const {
  if (attributes_) {
    return *SignedWebBundleId::Create(attributes_->web_bundle_id());
  }
  return absl::visit(
      base::Overloaded{[](const auto& signature_info) {
                         return SignedWebBundleId::CreateForPublicKey(
                             signature_info.public_key());
                       },
                       [](const SignedWebBundleSignatureInfoUnknown&)
                           -> SignedWebBundleId { NOTREACHED_NORETURN(); }},
      signature_stack_.entries()[0].signature_info());
}

}  // namespace web_package
