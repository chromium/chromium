// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/hash/sha1.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "crypto/sha2.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/x509_util.h"
#include "net/der/input.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

X509CertificateModel::X509CertificateModel(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
    std::string nickname)
    : nickname_(std::move(nickname)), cert_data_(std::move(cert_data)) {
  DCHECK(cert_data_);

  net::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  net::CertErrors unused_errors;
  if (!net::ParseCertificate(
          net::der::Input(CRYPTO_BUFFER_data(cert_data_.get()),
                          CRYPTO_BUFFER_len(cert_data_.get())),
          &tbs_certificate_tlv_, &signature_algorithm_tlv_, &signature_value_,
          &unused_errors) ||
      !ParseTbsCertificate(tbs_certificate_tlv_, options, &tbs_,
                           &unused_errors) ||
      !net::ParseName(tbs_.subject_tlv, &subject_rdns_) ||
      !net::ParseName(tbs_.issuer_tlv, &issuer_rdns_)) {
    return;
  }
  if (tbs_.has_extensions && !ParseExtensions(tbs_.extensions_tlv)) {
    return;
  }
  parsed_successfully_ = true;
}

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
}

std::string X509CertificateModel::HashCertSHA1() const {
  auto hash =
      base::SHA1HashSpan(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
}

std::string X509CertificateModel::GetVersion() const {
  DCHECK(parsed_successfully_);
  switch (tbs_.version) {
    case net::CertificateVersion::V1:
      return "1";
    case net::CertificateVersion::V2:
      return "2";
    case net::CertificateVersion::V3:
      return "3";
  }
}

std::string X509CertificateModel::GetSerialNumberHexified() const {
  DCHECK(parsed_successfully_);
  return ProcessRawBytesWithSeparators(tbs_.serial_number.UnsafeData(),
                                       tbs_.serial_number.Length(), ':', ':');
}

bool X509CertificateModel::ParseExtensions(
    const net::der::Input& extensions_tlv) {
  net::CertErrors unused_errors;
  net::der::Parser parser(extensions_tlv);

  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  net::der::Parser extensions_parser;
  if (!parser.ReadSequence(&extensions_parser))
    return false;

  // The Extensions SEQUENCE must contains at least 1 element (otherwise it
  // should have been omitted).
  if (!extensions_parser.HasMore())
    return false;

  while (extensions_parser.HasMore()) {
    net::ParsedExtension extension;

    net::der::Input extension_tlv;
    if (!extensions_parser.ReadRawTLV(&extension_tlv))
      return false;

    if (!ParseExtension(extension_tlv, &extension))
      return false;

    extensions_.push_back(extension);

    if (extension.oid == net::der::Input(net::kSubjectAltNameOid)) {
      subject_alt_names_ =
          net::GeneralNames::Create(extension.value, &unused_errors);
      if (!subject_alt_names_)
        return false;
    }
  }

  // By definition the input was a single Extensions sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessIDN(const std::string& input) {
  // Convert the ASCII input to a string16 for ICU.
  std::u16string input16;
  input16.reserve(input.length());
  input16.insert(input16.end(), input.begin(), input.end());

  std::u16string output16 = url_formatter::IDNToUnicode(input);
  if (input16 == output16)
    return input;  // Input did not contain any encoded data.

  // Input contained encoded data, return formatted string showing original and
  // decoded forms.
  return l10n_util::GetStringFUTF8(IDS_CERT_INFO_IDN_VALUE_FORMAT, input16,
                                   output16);
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBytesWithSeparators(const unsigned char* data,
                                          size_t data_length,
                                          char hex_separator,
                                          char line_separator) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters + a space or newline,
  // except for the last byte.
  std::string ret;
  size_t kMin = 0U;

  if (!data_length)
    return std::string();

  ret.reserve(std::max(kMin, data_length * 3 - 1));

  for (size_t i = 0; i < data_length; ++i) {
    unsigned char b = data[i];
    ret.push_back(kHexChars[(b >> 4) & 0xf]);
    ret.push_back(kHexChars[b & 0xf]);
    if (i + 1 < data_length) {
      if ((i + 1) % 16 == 0)
        ret.push_back(line_separator);
      else
        ret.push_back(hex_separator);
    }
  }
  return ret;
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBytes(const unsigned char* data, size_t data_length) {
  return ProcessRawBytesWithSeparators(data, data_length, ' ', '\n');
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBits(const unsigned char* data, size_t data_length) {
  return ProcessRawBytes(data, (data_length + 7) / 8);
}

}  // namespace x509_certificate_model
