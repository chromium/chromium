// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_
#define CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates.
namespace x509_certificate_model {

struct Extension {
  std::string name;
  std::string value;
};

struct NotPresent : absl::monostate {};
struct Error : absl::monostate {};
using OptionalStringOrError = absl::variant<Error, NotPresent, std::string>;

class X509CertificateModel {
 public:
  // Construct an X509CertificateModel from |cert_data|, which must must not be
  // nullptr.  |nickname| may optionally be provided as a platform-specific
  // nickname for the certificate, if available.
  X509CertificateModel(bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
                       std::string nickname);
  X509CertificateModel(X509CertificateModel&& other);
  X509CertificateModel& operator=(X509CertificateModel&& other) = default;
  ~X509CertificateModel();

  // ---------------------------------------------------------------------------
  // These methods are always safe to call even if |cert_data| could not be
  // parsed.

  // Returns lower case hex SHA256 hash of the certificate data.
  std::string HashCertSHA256() const;

  // Get something that can be used as a title for the certificate, using the
  // following priority:
  //   |nickname| passed to constructor
  //   subject commonName
  //   full subject
  //   dnsName or email address from subjectAltNames
  // If none of those are present, or certificate could not be parsed,
  // the hex SHA256 hash of the certificate data will be returned.
  std::string GetTitle() const;

  CRYPTO_BUFFER* cert_buffer() const { return cert_data_.get(); }
  bool is_valid() const { return parsed_successfully_; }

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if |is_valid()| returns true.

  // Returns lower case hex SHA256 hash of the SPKI.
  std::string HashSpkiSHA256() const;

  std::string GetVersion() const;
  std::string GetSerialNumberHexified() const;

  // Get the validity notBefore and notAfter times, returning true on success
  // or false on error in parsing or converting to a base::Time.
  bool GetTimes(base::Time* not_before, base::Time* not_after) const;

  // These methods returns the issuer/subject commonName/orgName/orgUnitName
  // formatted as a string, if present. Returns NotPresent if the attribute
  // type was not present, or Error if there was a parsing error.
  // The Get{Issuer,Subject}CommonName methods return the last (most specific)
  // commonName, while the other methods return the first (most general) value.
  // This matches the NSS behaviour of CERT_GetCommonName, CERT_GetOrgName,
  // CERT_GetOrgUnitName.
  OptionalStringOrError GetIssuerCommonName() const;
  OptionalStringOrError GetIssuerOrgName() const;
  OptionalStringOrError GetIssuerOrgUnitName() const;
  OptionalStringOrError GetSubjectCommonName() const;
  OptionalStringOrError GetSubjectOrgName() const;
  OptionalStringOrError GetSubjectOrgUnitName() const;

  // Get the issuer/subject name as a text block with one line per
  // attribute-value pair. Will process IDN in commonName, showing original and
  // decoded forms. Returns NotPresent if the Name was an empty sequence.
  // (Although note that technically an empty issuer name is invalid.)
  OptionalStringOrError GetIssuerName() const;
  OptionalStringOrError GetSubjectName() const;

  // Returns textual representations of the certificate's extensions, if any.
  // |critical_label| and |non_critical_label| will be used in the returned
  // extension.value fields to describe extensions that are critical or
  // non-critical.
  std::vector<Extension> GetExtensions(
      std::string_view critical_label,
      std::string_view non_critical_label) const;

  std::string ProcessSecAlgorithmSignature() const;
  std::string ProcessSecAlgorithmSubjectPublicKey() const;
  std::string ProcessSecAlgorithmSignatureWrap() const;

  std::string ProcessSubjectPublicKeyInfo() const;

  std::string ProcessRawBitsSignatureWrap() const;

 private:
  bool ParseExtensions(const bssl::der::Input& extensions_tlv);
  std::string ProcessExtension(std::string_view critical_label,
                               std::string_view non_critical_label,
                               const bssl::ParsedExtension& extension) const;
  std::optional<std::string> ProcessExtensionData(
      const bssl::ParsedExtension& extension) const;

  // Externally provided "nickname" for the cert.
  std::string nickname_;

  bool parsed_successfully_ = false;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_data_;
  bssl::der::Input tbs_certificate_tlv_;
  bssl::der::Input signature_algorithm_tlv_;
  bssl::der::BitString signature_value_;
  bssl::ParsedTbsCertificate tbs_;

  bssl::RDNSequence subject_rdns_;
  bssl::RDNSequence issuer_rdns_;
  std::vector<bssl::ParsedExtension> extensions_;

  // Parsed SubjectAltName extension.
  std::unique_ptr<bssl::GeneralNames> subject_alt_names_;
};

// For host values, if they contain IDN Punycode-encoded A-labels, this will
// return a string suitable for display that contains both the original and the
// decoded U-label form.  Otherwise, the string will be returned as is.
std::string ProcessIDN(const std::string& input);

// Parses |public_key_spki_der| as a DER-encoded X.509 SubjectPublicKeyInfo,
// then formats the public key as a string for displaying. Returns an empty
// string on error.
std::string ProcessRawSubjectPublicKeyInfo(base::span<const uint8_t> spki_der);

}  // namespace x509_certificate_model

#endif  // CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_
