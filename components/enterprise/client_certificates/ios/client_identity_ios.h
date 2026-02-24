// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_H_

#include <Security/Security.h>

#include <string_view>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/ios/client_identity_ios_error.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

class PrivateKey;

// iOS-specific extension of ClientIdentity that includes the SecIdentityRef
// required for platform-level operations.
struct ClientIdentityIOS {
  // Attempts to create a `ClientIdentityIOS` by wrapping a `ClientIdentity` and
  // generating a corresponding `SecIdentityRef`.
  //
  // This can fail and return a `ClientIdentityIOSError` if:
  // - The input `identity` is invalid (missing required fields).
  // - Converting the certificate to a `SecCertificateRef` fails.
  // - Obtaining a `SecKeyRef` from the private key fails.
  // - The Security framework cannot link the certificate and key into a
  //   `SecIdentityRef` (e.g., if they don't match).
  static base::expected<ClientIdentityIOS, ClientIdentityIOSError> TryCreate(
      const ClientIdentity& identity);

  static ClientIdentityIOS CreateForTesting(
      const ClientIdentity& identity,
      base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref);

  ClientIdentityIOS(const ClientIdentityIOS&);
  ClientIdentityIOS& operator=(const ClientIdentityIOS&);

  ClientIdentityIOS(ClientIdentityIOS&&);
  ClientIdentityIOS& operator=(ClientIdentityIOS&&);

  ~ClientIdentityIOS();

  bool is_valid() const { return identity.is_valid() && identity_ref.get(); }

  const std::string& name() const;
  scoped_refptr<PrivateKey> private_key() const;
  scoped_refptr<net::X509Certificate> certificate() const;

  ClientIdentity identity;
  base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref{};

  bool operator==(const ClientIdentityIOS& other) const;

  // Compares this iOS identity with a generic ClientIdentity for equivalence
  // based on name, private_key, and certificate.
  bool Equals(const ClientIdentity& other) const;

 private:
  explicit ClientIdentityIOS(const ClientIdentity& identity);
  ClientIdentityIOS(const ClientIdentity& identity,
                    base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref);
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_H_
