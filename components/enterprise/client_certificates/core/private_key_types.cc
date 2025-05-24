// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/private_key_types.h"

namespace client_certificates {

std::optional<PrivateKeySource> ToPrivateKeySource(
    client_certificates_pb::PrivateKey::PrivateKeySource proto_key_source) {
  switch (proto_key_source) {
    case client_certificates_pb::PrivateKey::PRIVATE_UNEXPORTABLE_KEY:
      return PrivateKeySource::kUnexportableKey;
    case client_certificates_pb::PrivateKey::PRIVATE_SOFTWARE_KEY:
      return PrivateKeySource::kSoftwareKey;
    case client_certificates_pb::PrivateKey::PRIVATE_OS_SOFTWARE_KEY:
      return PrivateKeySource::kOsSoftwareKey;
    default:
      return std::nullopt;
  }
}

client_certificates_pb::PrivateKey::PrivateKeySource ToProtoKeySource(
    PrivateKeySource private_key_source) {
  switch (private_key_source) {
    case PrivateKeySource::kUnexportableKey:
      return client_certificates_pb::PrivateKey::PRIVATE_UNEXPORTABLE_KEY;
    case PrivateKeySource::kSoftwareKey:
      return client_certificates_pb::PrivateKey::PRIVATE_SOFTWARE_KEY;
    case PrivateKeySource::kOsSoftwareKey:
      return client_certificates_pb::PrivateKey::PRIVATE_OS_SOFTWARE_KEY;
  }
}

std::optional<PrivateKeySource> ToPrivateKeySource(int pref_key_source) {
  switch (pref_key_source) {
    case 0:
      return PrivateKeySource::kUnexportableKey;
    case 1:
      return PrivateKeySource::kSoftwareKey;
    case 2:
      return PrivateKeySource::kOsSoftwareKey;
    default:
      return std::nullopt;
  }
}

}  // namespace client_certificates
