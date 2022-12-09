// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

namespace web_package {

// This class represents the integrity block of a Signed Web Bundle. It is
// guaranteed to have a `size_in_bytes` greater than 0, and at least one
// signature stack entry. It is constructed from a
// `mojom::BundleIntegrityBlockPtr`, which is the result of
// CBOR-parsing the integrity block of the Signed Web Bundle in a separate data
// decoder process. Given that the Signed Web Bundle is untrusted user input,
// there is a potential for an attacker to compromise the data decoder process
// by providing a malicious bundle and exploiting a memory safety bug.
//
// This class wraps the data received from the data decoder process into
// strongly typed classes, and re-verifies the validity of the data where
// possible (e.g., by checking that public keys have the correct length).
class SignedWebBundleIntegrityBlock {
 public:
  // Attempt to convert the provided Mojo integrity block into an instance of
  // this class, returning a string describing the error on failure.
  static base::expected<SignedWebBundleIntegrityBlock, std::string> Create(
      mojom::BundleIntegrityBlockPtr integrity_block);

  SignedWebBundleIntegrityBlock(const SignedWebBundleIntegrityBlock&) = delete;
  SignedWebBundleIntegrityBlock& operator=(
      const SignedWebBundleIntegrityBlock&) = delete;

  SignedWebBundleIntegrityBlock(SignedWebBundleIntegrityBlock&&);
  SignedWebBundleIntegrityBlock& operator=(SignedWebBundleIntegrityBlock&&);

  ~SignedWebBundleIntegrityBlock();

  // Returns the size of this integrity block in bytes. This is useful for
  // finding out where the actual Web Bundle starts.
  uint64_t size_in_bytes() const { return size_; }

  // Returns the the public keys contained in the signature stack in order.
  // The first public key in the vector is the first key that signed the Web
  // Bundle, the second key is the public key that countersigned the signature
  // of the first key, and so on.
  //
  // TODO(crbug.com/1376076): Remove this method - consumers should instead use
  // `::signature_stack`.
  const std::vector<Ed25519PublicKey> GetPublicKeyStack() const;

  const SignedWebBundleSignatureStack& signature_stack() const {
    return signature_stack_;
  }

 private:
  explicit SignedWebBundleIntegrityBlock(
      uint64_t size,
      SignedWebBundleSignatureStack&& signature_stack);

  uint64_t size_;
  SignedWebBundleSignatureStack signature_stack_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_
