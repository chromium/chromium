// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_

#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"

namespace web_package {

template <typename PublicKey, typename Signature>
class SignedWebBundleSignatureInfoBase {
 public:
  SignedWebBundleSignatureInfoBase(PublicKey public_key, Signature signature)
      : public_key_(std::move(public_key)), signature_(std::move(signature)) {}

  SignedWebBundleSignatureInfoBase(const SignedWebBundleSignatureInfoBase&) =
      default;
  SignedWebBundleSignatureInfoBase(SignedWebBundleSignatureInfoBase&&) =
      default;
  SignedWebBundleSignatureInfoBase& operator=(
      const SignedWebBundleSignatureInfoBase&) = default;
  SignedWebBundleSignatureInfoBase& operator=(
      SignedWebBundleSignatureInfoBase&&) = default;

  ~SignedWebBundleSignatureInfoBase() = default;

  bool operator==(const SignedWebBundleSignatureInfoBase& other) const =
      default;
  bool operator!=(const SignedWebBundleSignatureInfoBase& other) const =
      default;

  const PublicKey& public_key() const { return public_key_; }
  const Signature& signature() const { return signature_; }

 private:
  PublicKey public_key_;
  Signature signature_;
};

struct SignedWebBundleSignatureInfoUnknown {
 public:
  SignedWebBundleSignatureInfoUnknown() = default;

  SignedWebBundleSignatureInfoUnknown(
      const SignedWebBundleSignatureInfoUnknown&) = default;
  SignedWebBundleSignatureInfoUnknown(SignedWebBundleSignatureInfoUnknown&&) =
      default;
  SignedWebBundleSignatureInfoUnknown& operator=(
      const SignedWebBundleSignatureInfoUnknown&) = default;
  SignedWebBundleSignatureInfoUnknown& operator=(
      SignedWebBundleSignatureInfoUnknown&&) = default;

  ~SignedWebBundleSignatureInfoUnknown() = default;

  bool operator==(const SignedWebBundleSignatureInfoUnknown& other) const =
      default;
};

using SignedWebBundleSignatureInfoEd25519 =
    SignedWebBundleSignatureInfoBase<Ed25519PublicKey, Ed25519Signature>;

using SignedWebBundleSignatureInfoEcdsaP256SHA256 =
    SignedWebBundleSignatureInfoBase<EcdsaP256PublicKey,
                                     EcdsaP256SHA256Signature>;

using SignedWebBundleSignatureInfo =
    absl::variant<SignedWebBundleSignatureInfoUnknown,
                  SignedWebBundleSignatureInfoEd25519,
                  SignedWebBundleSignatureInfoEcdsaP256SHA256>;

// This class represents an entry on the signature stack of the integrity block
// of a Signed Web Bundle. See the documentation of
// `SignedWebBundleIntegrityBlock` for more details of how this class is used.
class SignedWebBundleSignatureStackEntry {
 public:
  SignedWebBundleSignatureStackEntry(
      const std::vector<uint8_t>& attributes_cbor,
      SignedWebBundleSignatureInfo signature_info);

  SignedWebBundleSignatureStackEntry(const SignedWebBundleSignatureStackEntry&);
  SignedWebBundleSignatureStackEntry(SignedWebBundleSignatureStackEntry&&);
  SignedWebBundleSignatureStackEntry& operator=(
      const SignedWebBundleSignatureStackEntry&);
  SignedWebBundleSignatureStackEntry& operator=(
      SignedWebBundleSignatureStackEntry&&);

  ~SignedWebBundleSignatureStackEntry();

  bool operator==(const SignedWebBundleSignatureStackEntry& other) const;
  bool operator!=(const SignedWebBundleSignatureStackEntry& other) const;

  const std::vector<uint8_t>& attributes_cbor() const {
    return attributes_cbor_;
  }
  const SignedWebBundleSignatureInfo& signature_info() const {
    return signature_info_;
  }

 private:
  std::vector<uint8_t> attributes_cbor_;
  SignedWebBundleSignatureInfo signature_info_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_ENTRY_H_
