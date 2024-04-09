// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_

#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"

namespace web_package {

class SignedWebBundleSignatureEd25519 {
 public:
  SignedWebBundleSignatureEd25519(Ed25519PublicKey public_key,
                                  Ed25519Signature signature);

  SignedWebBundleSignatureEd25519(const SignedWebBundleSignatureEd25519&) =
      default;
  SignedWebBundleSignatureEd25519& operator=(
      const SignedWebBundleSignatureEd25519&) = default;

  ~SignedWebBundleSignatureEd25519() = default;

  bool operator==(const SignedWebBundleSignatureEd25519& other) const = default;
  bool operator!=(const SignedWebBundleSignatureEd25519& other) const = default;

  const Ed25519PublicKey& public_key() const { return public_key_; }
  const Ed25519Signature& signature() const { return signature_; }

 private:
  Ed25519PublicKey public_key_;
  Ed25519Signature signature_;
};

struct SignedWebBundleSignatureUnknown {
 public:
  SignedWebBundleSignatureUnknown() = default;

  SignedWebBundleSignatureUnknown(const SignedWebBundleSignatureUnknown&) =
      default;
  SignedWebBundleSignatureUnknown& operator=(
      const SignedWebBundleSignatureUnknown&) = default;

  ~SignedWebBundleSignatureUnknown() = default;

  bool operator==(const SignedWebBundleSignatureUnknown& other) const = default;
};

using SignedWebBundleSignature = absl::variant<SignedWebBundleSignatureUnknown,
                                               SignedWebBundleSignatureEd25519>;

// This class represents an entry on the signature stack of the integrity block
// of a Signed Web Bundle. See the documentation of
// `SignedWebBundleIntegrityBlock` for more details of how this class is used.
class SignedWebBundleSignatureStackEntry {
 public:
  SignedWebBundleSignatureStackEntry(
      const std::vector<uint8_t>& complete_entry_cbor,
      const std::vector<uint8_t>& attributes_cbor,
      const SignedWebBundleSignature signature);

  SignedWebBundleSignatureStackEntry(const SignedWebBundleSignatureStackEntry&);
  SignedWebBundleSignatureStackEntry& operator=(
      const SignedWebBundleSignatureStackEntry&);

  ~SignedWebBundleSignatureStackEntry();

  bool operator==(const SignedWebBundleSignatureStackEntry& other) const;
  bool operator!=(const SignedWebBundleSignatureStackEntry& other) const;

  const std::vector<uint8_t>& complete_entry_cbor() const {
    return complete_entry_cbor_;
  }
  const std::vector<uint8_t>& attributes_cbor() const {
    return attributes_cbor_;
  }

  const SignedWebBundleSignature& signature() const { return signature_; }

 private:
  std::vector<uint8_t> complete_entry_cbor_;
  std::vector<uint8_t> attributes_cbor_;
  SignedWebBundleSignature signature_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
