// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_
#define CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/certificate_model/x509_certificate_model_base.h"

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates.
namespace x509_certificate_model {

struct Extension {
  std::string name;
  std::string value;
};

class X509CertificateModel : public X509CertificateModelBase {
 public:
  // Construct an X509CertificateModel from |cert_data|, which must must not be
  // nullptr.
  explicit X509CertificateModel(bssl::UniquePtr<CRYPTO_BUFFER> cert_data);
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
  //   subject commonName
  //   full subject
  //   dnsName or email address from subjectAltNames
  // If none of those are present, or certificate could not be parsed,
  // the hex SHA256 hash of the certificate data will be returned.
  std::string GetTitle() const;

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if |is_valid()| returns true.

  // Returns lower case hex SHA256 hash of the SPKI.
  std::string HashSpkiSHA256() const;

  std::string GetVersion() const;
  std::string GetSerialNumberHexified() const;

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
