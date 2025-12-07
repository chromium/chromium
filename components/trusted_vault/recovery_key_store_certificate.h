// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"

namespace bssl {
class ParsedCertificate;
}  // namespace bssl

namespace trusted_vault {

class SecureBoxPublicKey;

namespace internal {

// Intermediate representation of the data Chrome uses from a cert.xml file.
struct ParsedRecoveryKeyStoreCertXML {
  ParsedRecoveryKeyStoreCertXML(std::vector<std::string> intermediates,
                                std::vector<std::string> endpoints);
  ParsedRecoveryKeyStoreCertXML(const ParsedRecoveryKeyStoreCertXML&) = delete;
  ParsedRecoveryKeyStoreCertXML operator=(
      const ParsedRecoveryKeyStoreCertXML&) = delete;
  ParsedRecoveryKeyStoreCertXML(ParsedRecoveryKeyStoreCertXML&&);
  ParsedRecoveryKeyStoreCertXML& operator=(ParsedRecoveryKeyStoreCertXML&&);
  ~ParsedRecoveryKeyStoreCertXML();

  // The list of intermediate certificates as base64-encoded x509.
  std::vector<std::string> intermediates;

  // The list of endpoint certificates as base64-encoded x509.
  std::vector<std::string> endpoints;
};

// Intermediate representation of the data Chrome uses from a sig.xml file.
struct ParsedRecoveryKeyStoreSigXML {
  ParsedRecoveryKeyStoreSigXML(std::vector<std::string> intermediates,
                               std::string certificate,
                               std::string signature);
  ParsedRecoveryKeyStoreSigXML(const ParsedRecoveryKeyStoreSigXML&) = delete;
  ParsedRecoveryKeyStoreSigXML operator=(const ParsedRecoveryKeyStoreSigXML&) =
      delete;
  ParsedRecoveryKeyStoreSigXML(ParsedRecoveryKeyStoreSigXML&&);
  ParsedRecoveryKeyStoreSigXML& operator=(ParsedRecoveryKeyStoreSigXML&&);
  ~ParsedRecoveryKeyStoreSigXML();

  // The list of intermediate certificates as base64-encoded x509.
  std::vector<std::string> intermediates;

  // The leaf certificate as base64-encoded x509.
  std::string certificate;

  // The signature of the certificate over the cert.xml file.
  std::string signature;
};

// Returns a parsed representation of the cert.xml file. Returns nullopt if
// parsing failed.
std::optional<ParsedRecoveryKeyStoreCertXML> ParseRecoveryKeyStoreCertXML(
    std::string_view cert_xml);

// Returns a parsed representation of the sig.xml file. Returns nullopt if
// parsing failed.
std::optional<ParsedRecoveryKeyStoreSigXML> ParseRecoveryKeyStoreSigXML(
    std::string_view sig_xml);

// Given a base64-encoded x509 certificate, and a list of intermediates,
// attempts to build a chain to the root certificate.
// Returns the leaf certificate if successful, nullptr otherwise.
std::shared_ptr<const bssl::ParsedCertificate> VerifySignatureChain(
    std::string_view certificate_b64,
    base::span<std::string> intermediates_b64,
    base::Time current_time);

// Verifies a signature by `certificate` over `cert_xml`. Returns true if the
// signature matches, false otherwise.
bool VerifySignature(std::shared_ptr<const bssl::ParsedCertificate> certificate,
                     std::string_view cert_xml,
                     std::string_view signature_b64);

// Returns the public keys corresponding to the endpoints listed in `cert_xml`.
// If a certificate is not valid, it's skipped.
std::vector<std::unique_ptr<SecureBoxPublicKey>> ExtractEndpointPublicKeys(
    ParsedRecoveryKeyStoreCertXML cert_xml,
    base::Time current_time);

}  // namespace internal

// Represents a certificate chain from the recovery key store.
class RecoveryKeyStoreCertificate {
 public:
  ~RecoveryKeyStoreCertificate();
  RecoveryKeyStoreCertificate(RecoveryKeyStoreCertificate&& other);
  RecoveryKeyStoreCertificate& operator=(RecoveryKeyStoreCertificate&& other);

  // Parses a certificate from cert.xml and sig.xml. Returns nullopt if parsing
  // failed. This must only be called with values obtained from a trusted
  // source, like https://gstatic.com.
  static std::optional<RecoveryKeyStoreCertificate> Parse(
      std::string_view cert_xml,
      std::string_view sig_xml,
      base::Time current_time);

  const std::vector<std::unique_ptr<SecureBoxPublicKey>>&
  endpoint_public_keys() {
    return endpoint_public_keys_;
  }

 private:
  explicit RecoveryKeyStoreCertificate(
      std::vector<std::unique_ptr<SecureBoxPublicKey>> endpoint_public_keys);

  std::vector<std::unique_ptr<SecureBoxPublicKey>> endpoint_public_keys_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_
