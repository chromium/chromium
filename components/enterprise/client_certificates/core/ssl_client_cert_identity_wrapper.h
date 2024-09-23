// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_CLIENT_CERT_IDENTITY_WRAPPER_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_CLIENT_CERT_IDENTITY_WRAPPER_H_

#include "base/memory/scoped_refptr.h"
#include "net/ssl/client_cert_identity.h"

namespace client_certificates {

class SSLClientCertIdentityWrapper : public net::ClientCertIdentity {
 public:
  SSLClientCertIdentityWrapper(scoped_refptr<net::X509Certificate> cert,
                               scoped_refptr<net::SSLPrivateKey> private_key);
  ~SSLClientCertIdentityWrapper() override;

  // net::ClientCertIdentity:
  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override;

 private:
  scoped_refptr<net::SSLPrivateKey> private_key_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_CLIENT_CERT_IDENTITY_WRAPPER_H_
