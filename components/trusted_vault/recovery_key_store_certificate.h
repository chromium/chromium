// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trusted_vault {

namespace internal {

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

// Returns the list of endpoints from cert.xml as base64-encoded x509. Returns
// nullopt if parsing failed.
std::optional<std::vector<std::string>> ParseRecoveryKeyStoreCertXML(
    std::string_view cert_xml);

// Returns a parsed representation of the sig.xml file. Returns nullopt if
// parsing failed.
std::optional<ParsedRecoveryKeyStoreSigXML> ParseRecoveryKeyStoreSigXML(
    std::string_view sig_xml);

}  // namespace internal

// Represents a certificate chain from the recovery key store.
class RecoveryKeyStoreCertificate {
 public:
  // Parses a certificate from cert.xml and sig.xml. Returns nullopt if parsing
  // failed. This must only be called with values obtained from a trusted
  // source, like https://gstatic.com.
  static std::optional<RecoveryKeyStoreCertificate> Parse(
      std::string_view cert_xml,
      std::string_view sig_xml);

 private:
  RecoveryKeyStoreCertificate();
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CERTIFICATE_H_
