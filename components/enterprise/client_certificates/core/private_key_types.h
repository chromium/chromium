// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_

namespace client_certificates {

// Enum containing private key sources supported across all platforms. Some
// source may only be supported on certain platforms, but that information will
// be determined at construction time (in the factories). Do not reorder as the
// values could be serialized to disk.
enum class PrivateKeySource {
  // Key created via a `crypto::UnexportableKeyProvider`.
  kUnexportableKey = 0,

  // Key created from inside the browser, which is not protected by any hardware
  // mechanism.
  kSoftwareKey = 1,
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_
