// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ssl_client_cert_identity_wrapper.h"

#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

SSLClientCertIdentityWrapper::SSLClientCertIdentityWrapper(
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> private_key)
    : net::ClientCertIdentity(std::move(cert)),
      private_key_(std::move(private_key)) {}

SSLClientCertIdentityWrapper::~SSLClientCertIdentityWrapper() = default;

void SSLClientCertIdentityWrapper::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback) {
  std::move(private_key_callback).Run(private_key_);
}

}  // namespace client_certificates
