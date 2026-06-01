// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_
#define COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/certificate_model/x509_certificate_constants.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/general_names.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"

// This namespace defines a base set of functions for parsing and accessing
// X.509 certificate data.
namespace x509_certificate_model {

struct NotPresent : std::monostate {};
struct Error : std::monostate {};
using OptionalStringOrError = std::variant<Error, NotPresent, std::string>;

class X509CertificateModelBase {
 public:
  X509CertificateModelBase(X509CertificateModelBase&& other);
  X509CertificateModelBase& operator=(X509CertificateModelBase&& other) =
      default;
  ~X509CertificateModelBase();

  X509CertificateModelBase(const X509CertificateModelBase&) = delete;
  X509CertificateModelBase& operator=(const X509CertificateModelBase&) = delete;

  // ---------------------------------------------------------------------------
  // These methods are always safe to call even if `cert_data` could not be
  // parsed.

  // Returns lower case hex SHA256 hash of the certificate data.
  std::string HashCertSHA256() const;

  // Get something that can be used as a title for the certificate, using the
  // following priority:
  //   subject commonName
  //   full subject
  //   dnsName or email address from subjectAltNames
  // If none of those are present, or certificate could not be parsed,
  // the hex SHA256 hash of the certificate data will be returned.
  std::string GetTitle() const;

  CRYPTO_BUFFER* cert_buffer() const { return cert_data_.get(); }
  bool is_valid() const { return parsed_successfully_; }

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if `is_valid()` returns true.

  // Returns lower case hex SHA256 hash of the SPKI.
  std::string HashSpkiSHA256() const;

  std::string GetVersion() const;
  std::string GetSerialNumberHexified() const;

  std::string ProcessSecAlgorithmSignature() const;
  std::string ProcessSecAlgorithmSubjectPublicKey() const;
  std::string ProcessSecAlgorithmSignatureWrap() const;

  // Get the validity notBefore and notAfter times, returning true on success
  // or false on error in parsing or converting to a base::Time.
  bool GetTimes(base::Time* not_before, base::Time* not_after) const;

  // These methods return the issuer/subject commonName/orgName/orgUnitName
  // formatted as a string, if present. Returns NotPresent if the attribute
  // type was not present, or Error if there was a parsing error.
  // The Get{Issuer,Subject}CommonName methods return the last (most specific)
  // commonName, while the other methods return the first (most general) value.
  OptionalStringOrError GetIssuerCommonName() const;
  OptionalStringOrError GetIssuerOrgName() const;
  OptionalStringOrError GetIssuerOrgUnitName() const;
  OptionalStringOrError GetSubjectCommonName() const;
  OptionalStringOrError GetSubjectOrgName() const;
  OptionalStringOrError GetSubjectOrgUnitName() const;

 protected:
  // Parses the certificate data `cert_data` and populates
  // `tbs_certificate_tlv_`, `signature_algorithm_tlv_`, `signature_value_`,
  // `tbs_`, `subject_rdns_`, `issuer_rdns_`, and `extensions_`. If parsing
  // succeeds, `parsed_successfully_` is set to true.
  explicit X509CertificateModelBase(bssl::UniquePtr<CRYPTO_BUFFER> cert_data);

  bssl::der::Input tbs_certificate_tlv_;
  bssl::der::Input signature_algorithm_tlv_;
  bssl::der::BitString signature_value_;
  bssl::ParsedTbsCertificate tbs_;

  bssl::RDNSequence subject_rdns_;
  bssl::RDNSequence issuer_rdns_;

  // Parsed extensions from the certificate.
  std::vector<bssl::ParsedExtension> extensions_;

  // Parsed SubjectAltName extension.
  std::unique_ptr<bssl::GeneralNames> subject_alt_names_;

 private:
  // Parses the Extensions sequence, populating `extensions_` with each parsed
  // extension and `subject_alt_names_` if a SubjectAltName extension is
  // present.
  bool ParseExtensions(const bssl::der::Input& extensions_tlv);

  // Set to true by the class constructor after all parsing succeeds.
  bool parsed_successfully_ = false;

  bssl::UniquePtr<CRYPTO_BUFFER> cert_data_;
};

// Finds an attribute of the given OID type within a RelativeDistinguishedName.
// Returns NotPresent if the attribute was not found, Error if parsing failed,
// or the string value if found.
OptionalStringOrError FindAttributeOfType(
    bssl::der::Input oid,
    const bssl::RelativeDistinguishedName& rdn);

// Returns the value of the most general (first) name of the given OID type.
// Distinguished Names are specified in least to most specific order.
OptionalStringOrError FindFirstNameOfType(bssl::der::Input oid,
                                          const bssl::RDNSequence& rdns);

// Returns the value of the most specific (last) name of the given OID type.
// Distinguished Names are specified in least to most specific order.
OptionalStringOrError FindLastNameOfType(bssl::der::Input oid,
                                         const bssl::RDNSequence& rdns);

// Formats raw bytes as hex with `hex_separator` between bytes on the same
// line and `line_separator` every 16 bytes.
std::string ProcessRawBytesWithSeparators(base::span<const unsigned char> data,
                                          char hex_separator,
                                          char line_separator);

// Returns a string containing the dotted numeric form of `oid` prefixed by
// "OID.", or an empty string on error.
std::string OidToNumericString(bssl::der::Input oid);

// Parse an IA5String from a DER-encoded extension value.
std::optional<std::string> ProcessIA5String(bssl::der::Input extension_data);

// Parse a UserNotice DisplayText.
std::optional<std::string> ProcessUserNoticeDisplayText(CBS_ASN1_TAG tag,
                                                        bssl::der::Input value);

// Look up the l10n string resource ID for a common OID shared between
// platforms. Covers algorithm OIDs, some extended key usage OIDs, and
// certificate policy qualifier OIDs. Returns the IDS_CERT_* resource ID if
// found, or std::nullopt if the OID is not in the shared map.
std::optional<int> GetCommonOidStringId(bssl::der::Input oid);

}  // namespace x509_certificate_model

#endif  // COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_
