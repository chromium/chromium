// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_H_

#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/signed_web_bundles/types.h"

namespace web_package {

class SignedWebBundleIntegrityBlock;

// This class represents the signature stack of the integrity block of a Signed
// Web Bundle. See the documentation of `SignedWebBundleIntegrityBlock` for more
// details of how this class is used.
class SignedWebBundleSignatureStack {
 public:
  static base::expected<SignedWebBundleSignatureStack, std::string> Create(
      base::span<const SignedWebBundleSignatureStackEntry> entries);
  static base::expected<SignedWebBundleSignatureStack, std::string> Create(
      std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>&& entries);

  SignedWebBundleSignatureStack(const SignedWebBundleSignatureStack& other);
  SignedWebBundleSignatureStack& operator=(
      const SignedWebBundleSignatureStack& other);

  ~SignedWebBundleSignatureStack();

  bool operator==(const SignedWebBundleSignatureStack& other) const;
  bool operator!=(const SignedWebBundleSignatureStack& other) const;

  // Returns the signature stack entries. There is guaranteed to be at least one
  // entry. The first entry corresponds to the entry at the bottom of the stack.
  const std::vector<SignedWebBundleSignatureStackEntry>& entries() const {
    CHECK(!entries_.empty());
    return entries_;
  }

  std::vector<PublicKey> public_keys() const;

  // Returns the number of entries in the signature stack. This is guaranteed to
  // be at least 1.
  size_t size() const { return entries().size(); }

 private:
  explicit SignedWebBundleSignatureStack(
      std::vector<SignedWebBundleSignatureStackEntry> entries);

  std::vector<SignedWebBundleSignatureStackEntry> entries_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_STACK_H_
