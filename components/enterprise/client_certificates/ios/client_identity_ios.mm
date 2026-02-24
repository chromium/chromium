// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/client_certificates/ios/client_identity_ios.h"

#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_apple.h"

namespace client_certificates {

// static
base::expected<ClientIdentityIOS, ClientIdentityIOSError>
ClientIdentityIOS::TryCreate(const ClientIdentity& identity) {
  if (!identity.is_valid()) {
    return base::unexpected(ClientIdentityIOSError::kInvalidBaseIdentity);
  }

  base::apple::ScopedCFTypeRef<SecCertificateRef> certificate(
      net::x509_util::CreateSecCertificateFromX509Certificate(
          identity.certificate.get()));
  if (!certificate) {
    return base::unexpected(
        ClientIdentityIOSError::kCertificateConversionFailed);
  }

  SecKeyRef key_ref = identity.private_key->GetSecKeyRef();
  if (!key_ref) {
    return base::unexpected(
        ClientIdentityIOSError::kPrivateKeyConversionFailed);
  }

  base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref(
      SecIdentityCreate(kCFAllocatorDefault, certificate.get(), key_ref));

  if (!identity_ref) {
    return base::unexpected(ClientIdentityIOSError::kSecIdentityCreateFailed);
  }

  return ClientIdentityIOS(identity, std::move(identity_ref));
}

// static
ClientIdentityIOS ClientIdentityIOS::CreateForTesting(
    const ClientIdentity& identity,
    base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref) {
  return ClientIdentityIOS(identity, std::move(identity_ref));
}

ClientIdentityIOS::ClientIdentityIOS(const ClientIdentity& identity)
    : identity(identity) {}

ClientIdentityIOS::ClientIdentityIOS(
    const ClientIdentity& identity,
    base::apple::ScopedCFTypeRef<SecIdentityRef> identity_ref)
    : identity(identity), identity_ref(std::move(identity_ref)) {}

ClientIdentityIOS::ClientIdentityIOS(const ClientIdentityIOS&) = default;
ClientIdentityIOS& ClientIdentityIOS::operator=(const ClientIdentityIOS&) =
    default;

ClientIdentityIOS::ClientIdentityIOS(ClientIdentityIOS&&) = default;
ClientIdentityIOS& ClientIdentityIOS::operator=(ClientIdentityIOS&&) = default;

ClientIdentityIOS::~ClientIdentityIOS() = default;

const std::string& ClientIdentityIOS::name() const {
  return identity.name;
}

scoped_refptr<PrivateKey> ClientIdentityIOS::private_key() const {
  return identity.private_key;
}

scoped_refptr<net::X509Certificate> ClientIdentityIOS::certificate() const {
  return identity.certificate;
}

bool ClientIdentityIOS::operator==(const ClientIdentityIOS& other) const {
  return identity == other.identity &&
         identity_ref.get() == other.identity_ref.get();
}

bool ClientIdentityIOS::Equals(const ClientIdentity& other) const {
  return identity == other;
}

}  // namespace client_certificates
