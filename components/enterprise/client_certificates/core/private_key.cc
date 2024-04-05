// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/private_key.h"

#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

PrivateKey::PrivateKey(PrivateKeySource source,
                       scoped_refptr<net::SSLPrivateKey> ssl_private_key)
    : source_(source), ssl_private_key_(std::move(ssl_private_key)) {}

PrivateKey::~PrivateKey() = default;

PrivateKeySource PrivateKey::GetSource() const {
  return source_;
}

scoped_refptr<net::SSLPrivateKey> PrivateKey::GetSSLPrivateKey() {
  return ssl_private_key_;
}

}  // namespace client_certificates
