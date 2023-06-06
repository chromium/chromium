// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_FEATURES_H_
#define COMPONENTS_QR_CODE_GENERATOR_FEATURES_H_

#include "base/feature_list.h"
#include "components/qr_code_generator/rust_buildflags.h"

// Exposing the feature so that tests can inspect it and turn it on/off,
// but product code should instead use `IsRustyQrCodeGeneratorFeatureEnabled`.
BASE_DECLARE_FEATURE(kRustyQrCodeGeneratorFeature);

// Returns true if Rust should be used for QR code generation:
// 1) the GN-level `build_rust_qr` is true (i.e. the target platform has Rust
//    toolchain and `enable_rust_qr` is true)
// *and*
// 2) the `"RustyQrCodeGenerator"` base::Feature has been enabled.
//
// If Rust is used for QR code generation then:
// 1) //components/qr_code_generator becomes a thin wrapper around a 3rd-party
//    Rust crate that implements QR code generation
// 2) //chrome/services/qrcode_generator stops sandboxing QR code generation in
//    a separate utility process - QR code generation becomes a regular,
//    in-process C++ call (rather than a mojo call).
//
// See https://crbug.com/1431991 for more details about the feature and the
// Rust QR Code Generator project.
inline bool IsRustyQrCodeGeneratorFeatureEnabled() {
#if BUILDFLAG(BUILD_RUST_QR)
  return base::FeatureList::IsEnabled(kRustyQrCodeGeneratorFeature);
#else
  return false;
#endif
}

#endif  // COMPONENTS_QR_CODE_GENERATOR_FEATURES_H_
