// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_PARSED_CERTIFICATES_H_
#define CHROMEOS_NETWORK_ONC_ONC_PARSED_CERTIFICATES_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/network/onc/certificate_scope.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class X509Certificate;
}

namespace chromeos {
namespace onc {

// Represents certificates parsed from the ONC Certificates section.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) OncParsedCertificates {
 public:
  // A Server or Authority certificate parsed from ONC. The payload is
  // represented as a net::X509Certificate.
  class ServerOrAuthorityCertificate {
   public:
    enum class Type { kServer, kAuthority };

    ServerOrAuthorityCertificate(
        CertificateScope scope,
        Type type,
        const std::string& guid,
        const scoped_refptr<net::X509Certificate>& certificate,
        bool web_trust_requested);

    ServerOrAuthorityCertificate(const ServerOrAuthorityCertificate& other);
    ServerOrAuthorityCertificate& operator=(
        const ServerOrAuthorityCertificate& other);
    ServerOrAuthorityCertificate(ServerOrAuthorityCertificate&& other);
    ~ServerOrAuthorityCertificate();

    bool operator==(const ServerOrAuthorityCertificate& other) const;
    bool operator!=(const ServerOrAuthorityCertificate& other) const;

    CertificateScope scope() const { return scope_; }

    Type type() const { return type_; }

    const std::string& guid() const { return guid_; }

    const scoped_refptr<net::X509Certificate>& certificate() const {
      return certificate_;
    }
    // Returns true if the certificate definition in ONC had the "Web" TrustBit.
    bool web_trust_requested() const { return web_trust_requested_; }

   private:
    CertificateScope scope_;
    Type type_;
    std::string guid_;
    scoped_refptr<net::X509Certificate> certificate_;
    bool web_trust_requested_;
  };

  // A Client certificate parsed from ONC. The payload is the PKCS12 payload
  // (base64 decoded).
  class ClientCertificate {
   public:
    ClientCertificate(const std::string& guid, const std::string& pkcs12_data);

    ClientCertificate(const ClientCertificate& other);
    ClientCertificate& operator=(const ClientCertificate& other);
    ClientCertificate(ClientCertificate&& other);
    ~ClientCertificate();

    bool operator==(const ClientCertificate& other) const;
    bool operator!=(const ClientCertificate& other) const;

    const std::string& guid() const { return guid_; }

    // The PKCS12 payload of the certificate. Note: Contrary to the PKCS12 field
    // in ONC, this data is not base64-encoded.
    const std::string& pkcs12_data() const { return pkcs12_data_; }

   private:
    std::string guid_;
    std::string pkcs12_data_;
  };

  // Constructs an empty OncParsedCertificates. It will have no error, and the
  // certificate lists will be empty.
  OncParsedCertificates();

  // Parses |onc_certificates|. This must be a Value of type LIST, corresponding
  // to the Certificates part of the ONC specification.
  explicit OncParsedCertificates(const base::Value& onc_certificates);
  ~OncParsedCertificates();

  // Returns all certificates that were successfully parsed and had the type
  // Server or Authoriy.
  const std::vector<ServerOrAuthorityCertificate>&
  server_or_authority_certificates() const {
    return server_or_authority_certificates_;
  }

  // Returns all certificates that were successfully parsed and had the type
  // Client.
  const std::vector<ClientCertificate>& client_certificates() const {
    return client_certificates_;
  }

  // Returns true if any parsing error occured. Note that certificates which had
  // no parsing errors will still be available through
  // server_or_authority_certificates() and client_certificates().
  bool has_error() const { return has_error_; }

 private:
  bool ParseCertificate(const base::Value& onc_certificate);
  bool ParseServerOrCaCertificate(ServerOrAuthorityCertificate::Type type,
                                  const std::string& guid,
                                  const base::Value& onc_certificate);
  bool ParseClientCertificate(const std::string& guid,
                              const base::Value& onc_certificate);

  std::vector<ServerOrAuthorityCertificate> server_or_authority_certificates_;
  std::vector<ClientCertificate> client_certificates_;
  bool has_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(OncParsedCertificates);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_PARSED_CERTIFICATES_H_
