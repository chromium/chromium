// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "chromeos/ash/components/chaps_util/pkcs12_reader.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

namespace chromeos {

std::vector<uint8_t> Pkcs12Reader::BignumToBytes(const BIGNUM* bignum) const {
  std::vector<uint8_t> result(BN_num_bytes(bignum));
  BN_bn2bin(bignum, result.data());

  return result;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetPkcs12KeyAndCerts(
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    bssl::UniquePtr<EVP_PKEY>& key,
    bssl::UniquePtr<STACK_OF(X509)>& certs) const {
  CBS pkcs12;
  CBS_init(&pkcs12, reinterpret_cast<const uint8_t*>(pkcs12_data.data()),
           pkcs12_data.size());
  if (!pkcs12.data || pkcs12.len <= 0) {
    return Pkcs12ReaderStatusCode::kMissedPkcs12Data;
  }

  EVP_PKEY* key_ptr = nullptr;
  certs = bssl::UniquePtr<STACK_OF(X509)>(sk_X509_new_null());
  const int get_key_and_cert_result = PKCS12_get_key_and_certs(
      &key_ptr, certs.get(), &pkcs12, password.c_str());
  key = bssl::UniquePtr<EVP_PKEY>(key_ptr);
  if (!get_key_and_cert_result || !key_ptr) {
    return Pkcs12ReaderStatusCode::kFailedToParsePkcs12Data;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetDerEncodedCert(
    X509* cert,
    bssl::UniquePtr<uint8_t>& cert_der,
    int& cert_der_size) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertDerMissed;
  }

  uint8_t* cert_der_ptr = nullptr;
  cert_der_size = i2d_X509(cert, &cert_der_ptr);
  cert_der = bssl::UniquePtr<uint8_t>(cert_der_ptr);
  if (cert_der_size <= 0) {
    return Pkcs12ReaderStatusCode::kPkcs12CertDerFailed;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetIssuerNameDer(
    X509* cert,
    base::span<const uint8_t>& issuer_name_data) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }

  X509_NAME* issuer_name = X509_get_issuer_name(cert);
  if (!issuer_name) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }

  const uint8_t* name_der;
  size_t name_der_size;
  if (!X509_NAME_get0_der(issuer_name, &name_der, &name_der_size)) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerDerNameFailed;
  }
  issuer_name_data = {name_der, name_der_size};
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetSubjectNameDer(
    X509* cert,
    base::span<const uint8_t>& subject_name_data) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameMissed;
  }

  X509_NAME* subject_name = X509_get_subject_name(cert);
  if (!subject_name) {
    return Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameMissed;
  }

  const uint8_t* name_der;
  size_t name_der_size;
  if (!X509_NAME_get0_der(subject_name, &name_der, &name_der_size)) {
    return Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameDerFailed;
  }
  subject_name_data = {name_der, name_der_size};
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetSerialNumberDer(
    X509* cert,
    bssl::UniquePtr<uint8_t>& der_serial_number,
    int& der_serial_number_size) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertSerialNumberMissed;
  }

  const ASN1_INTEGER* serial_number = X509_get0_serialNumber(cert);
  uint8_t* der_serial_number_ptr = nullptr;
  der_serial_number_size =
      i2d_ASN1_INTEGER(serial_number, &der_serial_number_ptr);
  der_serial_number = bssl::UniquePtr<uint8_t>(der_serial_number_ptr);
  if (der_serial_number_size < 0) {
    return Pkcs12ReaderStatusCode::kPkcs12CertSerialNumberDerFailed;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetLabel(X509* cert,
                                              std::string& label) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }

  // This is basic implementation which is using common name from the
  // Subject name for the label.
  // TODO(b/284144984): Replace with proper implementation and update tests.
  X509_NAME* subject_name = X509_get_subject_name(cert);
  if (!subject_name) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }

  char temp_label[512] = "";
  int get_label_result = X509_NAME_get_text_by_NID(
      subject_name, NID_commonName, temp_label, sizeof(temp_label));
  if (!get_label_result) {
    return Pkcs12ReaderStatusCode::kPkcs12LabelCreationFailed;
  }

  label = temp_label;
  return Pkcs12ReaderStatusCode::kSuccess;
}

}  // namespace chromeos
