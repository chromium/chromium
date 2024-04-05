// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/mock_private_key.h"

namespace client_certificates {

MockPrivateKey::MockPrivateKey(
    PrivateKeySource source,
    scoped_refptr<net::SSLPrivateKey> ssl_private_key)
    : PrivateKey(source, std::move(ssl_private_key)) {}

MockPrivateKey::~MockPrivateKey() = default;

}  // namespace client_certificates
