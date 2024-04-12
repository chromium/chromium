// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_

#include <optional>

#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"

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

  kMaxValue = kSoftwareKey
};

// Converts a `proto_key_source` from the proto values to the C++ enum values.
// Returns std::nullopt if `proto_key_source` is unsupported or undefined.
std::optional<PrivateKeySource> ToPrivateKeySource(
    client_certificates_pb::PrivateKey::PrivateKeySource proto_key_source);

// Converts a `private_key_source` from the C++ enum values to the proto values.
client_certificates_pb::PrivateKey::PrivateKeySource ToProtoKeySource(
    PrivateKeySource private_key_source);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_TYPES_H_
