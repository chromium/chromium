// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/client_identity.h"

#include <utility>

#include "base/check.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

ClientIdentity::ClientIdentity(const std::string& name,
                               scoped_refptr<PrivateKey> private_key,
                               scoped_refptr<net::X509Certificate> certificate)
    : name(name),
      private_key(std::move(private_key)),
      certificate(std::move(certificate)) {}

ClientIdentity::ClientIdentity(const ClientIdentity&) = default;
ClientIdentity& ClientIdentity::operator=(const ClientIdentity&) = default;

ClientIdentity::~ClientIdentity() = default;

bool ClientIdentity::operator==(const ClientIdentity& other) const {
  return name == other.name && private_key == other.private_key &&
         certificate == other.certificate;
}

}  // namespace client_certificates
