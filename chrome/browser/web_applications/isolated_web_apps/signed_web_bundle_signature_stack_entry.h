// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_

#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/ed25519_signature.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_app {

// This class represents an entry on the signature stack of the integrity block
// of a Signed Web Bundle.
class SignedWebBundleSignatureStackEntry {
 public:
  // Attempt to convert the provided Mojo signature stack entry into an instance
  // of this class, returning a string describing the error on failure.
  static base::expected<SignedWebBundleSignatureStackEntry, std::string> Create(
      const web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
          entry);

  SignedWebBundleSignatureStackEntry(const SignedWebBundleSignatureStackEntry&);

  ~SignedWebBundleSignatureStackEntry();

  [[nodiscard]] bool VerifySignature(base::span<const uint8_t> message) const {
    return signature().Verify(message, public_key());
  }

  const web_package::Ed25519PublicKey& public_key() const {
    return public_key_;
  }
  const Ed25519Signature& signature() const { return signature_; }

  const std::vector<uint8_t>& complete_entry_cbor() const {
    return complete_entry_cbor_;
  }
  const std::vector<uint8_t>& attributes_cbor() const {
    return attributes_cbor_;
  }

 private:
  SignedWebBundleSignatureStackEntry(
      const std::vector<uint8_t>& complete_entry_cbor,
      const std::vector<uint8_t>& attributes_cbor,
      const web_package::Ed25519PublicKey& public_key,
      const Ed25519Signature& signature);

  const std::vector<uint8_t> complete_entry_cbor_;
  const std::vector<uint8_t> attributes_cbor_;

  const web_package::Ed25519PublicKey public_key_;
  const Ed25519Signature signature_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
