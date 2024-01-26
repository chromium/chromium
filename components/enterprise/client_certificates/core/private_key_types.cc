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
  }
}

}  // namespace client_certificates
