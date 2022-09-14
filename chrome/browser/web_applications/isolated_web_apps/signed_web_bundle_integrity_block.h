// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_app {

// This class represents the integrity block of a Signed Web Bundle. It is
// guaranteed to have a `size_in_bytes` greater than 0, and at least one
// signature stack entry.
class SignedWebBundleIntegrityBlock {
 public:
  // Attempt to convert the provided Mojo integrity block into an instance of
  // this class, returning a string describing the error on failure.
  static base::expected<SignedWebBundleIntegrityBlock, std::string> Create(
      web_package::mojom::BundleIntegrityBlockPtr integrity_block);

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
  const std::vector<web_package::Ed25519PublicKey> GetPublicKeyStack() const;

  const std::vector<SignedWebBundleSignatureStackEntry>& signature_stack()
      const {
    return signature_stack_;
  }

 private:
  explicit SignedWebBundleIntegrityBlock(
      uint64_t size,
      std::vector<SignedWebBundleSignatureStackEntry>&& signature_stack);

  uint64_t size_;
  std::vector<SignedWebBundleSignatureStackEntry> signature_stack_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_INTEGRITY_BLOCK_H_
