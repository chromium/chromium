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
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
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

  SignedWebBundleIntegrityBlock(const SignedWebBundleIntegrityBlock&);
  SignedWebBundleIntegrityBlock& operator=(
      const SignedWebBundleIntegrityBlock&);

  ~SignedWebBundleIntegrityBlock();

  bool operator==(const SignedWebBundleIntegrityBlock& other) const;
  bool operator!=(const SignedWebBundleIntegrityBlock& other) const;

  // Returns the size of this integrity block in bytes. This is useful for
  // finding out where the actual Web Bundle starts.
  uint64_t size_in_bytes() const { return size_in_bytes_; }

  const SignedWebBundleSignatureStack& signature_stack() const {
    return signature_stack_;
  }

  // Returns the id of the web bundle specified in integrity block attributes.
  const SignedWebBundleId& web_bundle_id() const { return web_bundle_id_; }

  const std::vector<uint8_t>& attributes_cbor() const {
    return attributes_cbor_;
  }

 private:
  explicit SignedWebBundleIntegrityBlock(
      uint64_t size_in_bytes,
      SignedWebBundleSignatureStack&& signature_stack,
      SignedWebBundleId web_bundle_id,
      std::vector<uint8_t> attributes_cbor);

  uint64_t size_in_bytes_;
  SignedWebBundleSignatureStack signature_stack_;

  SignedWebBundleId web_bundle_id_;
  std::vector<uint8_t> attributes_cbor_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_
