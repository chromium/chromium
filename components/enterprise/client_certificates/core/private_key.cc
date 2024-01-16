// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/private_key.h"

namespace client_certificates {

PrivateKey::PrivateKey(PrivateKeySource source) : source_(source) {}

PrivateKey::~PrivateKey() = default;

PrivateKeySource PrivateKey::GetSource() const {
  return source_;
}

}  // namespace client_certificates
