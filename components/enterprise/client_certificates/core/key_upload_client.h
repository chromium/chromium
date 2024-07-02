// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_KEY_UPLOAD_CLIENT_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_KEY_UPLOAD_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace enterprise_attestation {
class CloudManagementDelegate;
}  // namespace enterprise_attestation

namespace client_certificates {

class PrivateKey;

// Interface to be used for uploading a public key to an attestation server.
class KeyUploadClient {
 public:
  static std::unique_ptr<KeyUploadClient> Create(
      std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
          management_delegate);

  using CreateCertificateCallback =
      base::OnceCallback<void(HttpCodeOrClientError,
                              scoped_refptr<net::X509Certificate>)>;
  using SyncKeyCallback = base::OnceCallback<void(HttpCodeOrClientError)>;

  virtual ~KeyUploadClient() = default;

  // Uploads the SPKI corresponding to `private_key` along with a
  // proof-of-possession with a parameter indicating that a client certificate
  // should be provisioned for this key. `callback` will be invoked with the
  // HTTP status code from the response, and a X509Certificate instance if it
  // was present in the response body.
  virtual void CreateCertificate(scoped_refptr<PrivateKey> private_key,
                                 CreateCertificateCallback callback) = 0;

  // Uploads the SPKI corresponding to `private_key` along with a
  // proof-of-possession. `callback` will be invoked with the HTTP status code
  // from the response.
  virtual void SyncKey(scoped_refptr<PrivateKey> private_key,
                       SyncKeyCallback callback) = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_KEY_UPLOAD_CLIENT_H_
