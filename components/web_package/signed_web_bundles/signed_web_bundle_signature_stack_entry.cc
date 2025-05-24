// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

namespace web_package {

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const std::vector<uint8_t>& attributes_cbor,
    SignedWebBundleSignatureInfo signature_info)
    : attributes_cbor_(attributes_cbor),
      signature_info_(std::move(signature_info)) {}

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry::SignedWebBundleSignatureStackEntry(
    SignedWebBundleSignatureStackEntry&&) = default;

SignedWebBundleSignatureStackEntry&
SignedWebBundleSignatureStackEntry::operator=(
    const SignedWebBundleSignatureStackEntry&) = default;

SignedWebBundleSignatureStackEntry&
SignedWebBundleSignatureStackEntry::operator=(
    SignedWebBundleSignatureStackEntry&&) = default;

SignedWebBundleSignatureStackEntry::~SignedWebBundleSignatureStackEntry() =
    default;

}  // namespace web_package
