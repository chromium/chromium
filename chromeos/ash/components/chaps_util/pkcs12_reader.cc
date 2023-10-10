// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include <nss/pk11pub.h>
#include "chromeos/ash/components/chaps_util/key_helper.h"
#include "chromeos/ash/components/chaps_util/pkcs12_reader.h"
#include "net/cert/x509_util_nss.h"
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

Pkcs12ReaderStatusCode Pkcs12Reader::FindRawCertsWithSubject(
    PK11SlotInfo* slot,
    base::span<const uint8_t> required_subject_name,
    CERTCertificateList** found_certs) const {
  SECItem subject_item;
  subject_item.len = required_subject_name.size();
  subject_item.data = const_cast<uint8_t*>(required_subject_name.data());

  // This is a call to NSS, replace it later with a call to Chaps.
  SECStatus fetch_cert_with_same_subject_status =
      PK11_FindRawCertsWithSubject(slot, &subject_item, found_certs);
  if (fetch_cert_with_same_subject_status != SECSuccess) {
    return Pkcs12ReaderStatusCode::kPkcs12FindCertsWithSubjectFailed;
  }
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

  int alias_len = 0;
  const unsigned char* parsed_alias = X509_alias_get0(cert, &alias_len);
  if (parsed_alias) {
    label = std::string(reinterpret_cast<const char*>(parsed_alias),
                        static_cast<size_t>(alias_len));

    return Pkcs12ReaderStatusCode::kSuccess;
  }

  X509_NAME* subject_name = X509_get_subject_name(cert);
  if (!subject_name) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }

  char temp_label[512];
  int get_common_name = X509_NAME_get_text_by_NID(
      subject_name, NID_commonName, temp_label, sizeof(temp_label));
  if (get_common_name < 0) {
    return Pkcs12ReaderStatusCode::kPkcs12CNExtractionFailed;
  }
  label = temp_label;
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::IsCertWithNicknameInSlots(
    const std::string& nickname,
    bool& is_nickname_present) const {
  if (nickname.empty()) {
    return Pkcs12ReaderStatusCode::kPkcs12MissedNickname;
  }
  CERTCertList* results =
      PK11_FindCertsFromNickname(nickname.c_str(), /*wincx=*/nullptr);
  is_nickname_present = results && results->list.next != NULL;
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode Pkcs12Reader::DoesKeyForCertExist(
    PK11SlotInfo* slot,
    const scoped_refptr<net::X509Certificate>& cert) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }
  if (!slot) {
    return Pkcs12ReaderStatusCode::kMissedSlotInfo;
  }
  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(cert.get());

  SECKEYPrivateKey* private_key =
      PK11_FindPrivateKeyFromCert(slot, nss_cert.get(), nullptr);

  if (private_key) {
    return Pkcs12ReaderStatusCode::kSuccess;
  }
  return Pkcs12ReaderStatusCode::kKeyDataMissed;
}

Pkcs12ReaderStatusCode Pkcs12Reader::DoesKeyForDerCertExist(
    PK11SlotInfo* slot,
    const scoped_refptr<net::X509Certificate>& cert) const {
  if (!cert) {
    return Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;
  }
  if (!slot) {
    return Pkcs12ReaderStatusCode::kMissedSlotInfo;
  }
  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(cert.get());

  SECKEYPrivateKey* private_key =
      PK11_FindKeyByDERCert(slot, nss_cert.get(), nullptr);

  if (private_key) {
    return Pkcs12ReaderStatusCode::kSuccess;
  }
  return Pkcs12ReaderStatusCode::kKeyDataMissed;
}

Pkcs12ReaderStatusCode Pkcs12Reader::EnrichKeyData(KeyData& key_data) const {
  if (!key_data.key) {
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }
  if (EVP_PKEY_base_id(key_data.key.get()) == EVP_PKEY_RSA) {
    const RSA* rsa_key = EVP_PKEY_get0_RSA(key_data.key.get());
    key_data.rsa_key_modulus_bytes = BignumToBytes(RSA_get0_n(rsa_key));
    key_data.cka_id_value =
        SECItemToBytes(MakeIdFromPubKeyNss(key_data.rsa_key_modulus_bytes));
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  return Pkcs12ReaderStatusCode::kPkcs12NotSupportedKeyType;
}

Pkcs12ReaderStatusCode Pkcs12Reader::CheckRelation(const KeyData& key_data,
                                                   X509* cert,
                                                   bool& is_related) const {
  if (!key_data.key) {
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  if (!cert) {
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }

  // Check for RSA key.
  if (!key_data.rsa_key_modulus_bytes.empty()) {
    EVP_PKEY* pub_key_ptr = X509_get_pubkey(cert);
    bssl::UniquePtr<EVP_PKEY> pubkey(pub_key_ptr);
    const RSA* rsa_pub_key = EVP_PKEY_get0_RSA(pubkey.get());
    std::vector<uint8_t> public_modulus_bytes =
        BignumToBytes(RSA_get0_n(rsa_pub_key));

    if (key_data.rsa_key_modulus_bytes != public_modulus_bytes) {
      return Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound;
    }
    is_related = true;
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  return Pkcs12ReaderStatusCode::kPkcs12NotSupportedKeyType;
}

Pkcs12ReaderStatusCode Pkcs12Reader::GetCertFromDerData(
    const unsigned char* der_cert_data,
    int der_cert_len,
    bssl::UniquePtr<X509>& x509) const {
  if (!der_cert_data || !der_cert_len) {
    return Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound;
  };
  X509* cert = d2i_X509(NULL, &der_cert_data, der_cert_len);
  x509 = bssl::UniquePtr<X509>(cert);
  return Pkcs12ReaderStatusCode::kSuccess;
}

}  // namespace chromeos
