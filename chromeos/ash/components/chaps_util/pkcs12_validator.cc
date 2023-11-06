// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/pkcs12_validator.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/chaps_util/pkcs12_reader.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

namespace chromeos {
namespace {

constexpr char kPkcs12CertImportFailed[] =
    "Chaps util cert import failed with ";
constexpr int kMaxAttemptUniqueNicknameCreation = 100;
constexpr const char kDefaultNickname[] = "Unknown org";

// Custom CERTCertificateList object allows to avoid calls to PORT_FreeArena()
// after every usage of CERTCertificateList.
struct CERTCertificateListDeleter {
  void operator()(CERTCertificateList* cert_list) {
    CERT_DestroyCertificateList(cert_list);
  }
};
using Pkcs12ScopedCERTCertificateList =
    std::unique_ptr<CERTCertificateList, CERTCertificateListDeleter>;

std::string AddUniqueIndex(std::string old_name, int unique_number) {
  if (unique_number == 0) {
    return old_name;
  }
  return old_name + " " + base::NumberToString(unique_number);
}

Pkcs12ReaderStatusCode MakeNicknameUnique(PK11SlotInfo* slot,
                                          const std::string& nickname,
                                          const Pkcs12Reader& pkcs12_reader,
                                          std::string& unique_nickname) {
  int unique_counter = 0;
  std::string temp_nickname;
  bool is_nickname_used = true;
  while (is_nickname_used &&
         unique_counter < kMaxAttemptUniqueNicknameCreation) {
    temp_nickname = AddUniqueIndex(nickname, unique_counter);
    Pkcs12ReaderStatusCode nickname_search_result =
        pkcs12_reader.IsCertWithNicknameInSlots(temp_nickname,
                                                is_nickname_used);
    if (nickname_search_result != Pkcs12ReaderStatusCode::kSuccess) {
      LOG(ERROR) << MakePkcs12CertImportErrorMessage(nickname_search_result);
      return nickname_search_result;
    }
    unique_counter++;
  }

  if (unique_counter == kMaxAttemptUniqueNicknameCreation) {
    return Pkcs12ReaderStatusCode::kPkcs12ReachedMaxAttemptForUniqueness;
  }
  unique_nickname = temp_nickname;

  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode GetFirstCertNicknameWithSubject(
    PK11SlotInfo* slot,
    const Pkcs12Reader& pkcs12_reader,
    base::span<const uint8_t> required_subject_name,
    std::string& previously_used_nickname) {
  CERTCertificateList* found_certs_ptr = nullptr;
  Pkcs12ReaderStatusCode fetch_cert_status =
      pkcs12_reader.FindRawCertsWithSubject(slot, required_subject_name,
                                            &found_certs_ptr);
  // Wrapping into ScopedCERTCertificateList for proper cleanup.
  Pkcs12ScopedCERTCertificateList found_certs(found_certs_ptr);

  if (fetch_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(fetch_cert_status);
    return fetch_cert_status;
  }

  if (found_certs) {
    for (int certIndex = 0; certIndex < found_certs->len; certIndex++) {
      const unsigned char* der_cert_data = found_certs->certs[certIndex].data;
      int der_cert_len = found_certs->certs[certIndex].len;
      bssl::UniquePtr<X509> x509_cert;

      Pkcs12ReaderStatusCode get_cert_result = pkcs12_reader.GetCertFromDerData(
          der_cert_data, der_cert_len, x509_cert);
      if (get_cert_result != Pkcs12ReaderStatusCode::kSuccess) {
        LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_cert_result);
        continue;
      }

      std::string nickname;
      Pkcs12ReaderStatusCode label_fetch_result =
          pkcs12_reader.GetLabel(x509_cert.get(), nickname);
      if (label_fetch_result != Pkcs12ReaderStatusCode::kSuccess) {
        LOG(ERROR) << MakePkcs12CertImportErrorMessage(label_fetch_result);
        continue;
      }
      if (!nickname.empty()) {
        previously_used_nickname = nickname;
        return Pkcs12ReaderStatusCode::kSuccess;
      }
    }
  }

  return Pkcs12ReaderStatusCode::kPkcs12NoNicknamesWasExtracted;
}

Pkcs12ReaderStatusCode GetScopedCert(
    X509* cert,
    const Pkcs12Reader& pkcs12_reader,
    scoped_refptr<net::X509Certificate>& scoped_cert) {
  int cert_der_size = 0;
  bssl::UniquePtr<uint8_t> cert_der;
  Pkcs12ReaderStatusCode get_cert_der_result =
      pkcs12_reader.GetDerEncodedCert(cert, cert_der, cert_der_size);
  if (get_cert_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_cert_der_result);
    return get_cert_der_result;
  }

  scoped_cert = net::X509Certificate::CreateFromBytes(
      base::make_span(cert_der.get(), static_cast<size_t>(cert_der_size)));

  return Pkcs12ReaderStatusCode::kSuccess;
}

}  // namespace

std::string MakePkcs12CertImportErrorMessage(
    Pkcs12ReaderStatusCode error_code) {
  return kPkcs12CertImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}

Pkcs12ReaderStatusCode GetNickname(PK11SlotInfo* slot,
                                   X509* cert,
                                   const Pkcs12Reader& pkcs12_reader,
                                   std::string& cert_nickname) {
  base::span<const uint8_t> required_subject;
  Pkcs12ReaderStatusCode get_subject_name_result =
      pkcs12_reader.GetSubjectNameDer(cert, required_subject);
  if (get_subject_name_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_subject_name_result);
    return get_subject_name_result;
  }

  std::string already_used_nickname;
  Pkcs12ReaderStatusCode fetch_certs_result = GetFirstCertNicknameWithSubject(
      slot, pkcs12_reader, required_subject, already_used_nickname);

  bool acceptable_fetch_certs_result =
      fetch_certs_result == Pkcs12ReaderStatusCode::kSuccess ||
      fetch_certs_result ==
          Pkcs12ReaderStatusCode::kPkcs12NoNicknamesWasExtracted;

  if (!acceptable_fetch_certs_result) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(fetch_certs_result);
    return fetch_certs_result;
  }

  if (fetch_certs_result == Pkcs12ReaderStatusCode::kSuccess &&
      !already_used_nickname.empty()) {
    cert_nickname = already_used_nickname;
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  // No certs with the same subject were found in slot,
  // will try to extract nickname from the cert.
  std::string nickname;
  Pkcs12ReaderStatusCode nickname_extraction_result =
      pkcs12_reader.GetLabel(cert, nickname);
  if (nickname_extraction_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(WARNING) << MakePkcs12CertImportErrorMessage(
        nickname_extraction_result);
  }

  if (nickname.empty()) {
    // We did try our best, giving default nickname.
    nickname = kDefaultNickname;
  }

  std::string new_unique_nickname;
  Pkcs12ReaderStatusCode make_nickname_uniq_result =
      MakeNicknameUnique(slot, nickname, pkcs12_reader, new_unique_nickname);
  if (make_nickname_uniq_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(make_nickname_uniq_result);
    return make_nickname_uniq_result;
  }

  cert_nickname = new_unique_nickname;
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode CanFindInstalledKey(PK11SlotInfo* slot,
                                           const CertData& cert,
                                           const Pkcs12Reader& pkcs12_reader,
                                           bool& is_key_installed) {
  scoped_refptr<net::X509Certificate> scoped_cert;
  Pkcs12ReaderStatusCode scoped_cert_result =
      GetScopedCert(cert.x509, pkcs12_reader, scoped_cert);
  if (scoped_cert_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(scoped_cert_result);
    return scoped_cert_result;
  }

  // Searching using X509 cert.
  Pkcs12ReaderStatusCode res = pkcs12_reader.DoesKeyForCertExist(
      slot, Pkcs12ReaderCertSearchType::kPlainType, scoped_cert);
  if (res == Pkcs12ReaderStatusCode::kSuccess) {
    LOG(WARNING) << "Private key is already installed in slot";
    is_key_installed = true;
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  if (res != Pkcs12ReaderStatusCode::kKeyDataMissed) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(res);
    return res;
  }

  // Searching using DER form of X509 cert.
  res = pkcs12_reader.DoesKeyForCertExist(
      slot, Pkcs12ReaderCertSearchType::kDerType, scoped_cert);
  if (res == Pkcs12ReaderStatusCode::kSuccess) {
    LOG(WARNING) << "Private key is already installed in slot";
    is_key_installed = true;
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  if (res != Pkcs12ReaderStatusCode::kKeyDataMissed) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(res);
    return res;
  }

  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ValidateAndPrepareCertData(
    PK11SlotInfo* slot,
    const Pkcs12Reader& pkcs12_reader,
    const bssl::UniquePtr<STACK_OF(X509)>& certs,
    KeyData& key_data,
    std::vector<CertData>& valid_certs_data) {
  if (!slot) {
    return Pkcs12ReaderStatusCode::kMissedSlotInfo;
  }
  if (!certs) {
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }
  if (!key_data.key) {
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  // Normal case if there is one private key and one certificate in pkcs12, but
  // it might be the whole chain included. All certs that are not directly
  // related to the key will be filtered out.
  std::string cert_nickname;
  for (size_t i = 0; i < sk_X509_num(certs.get()); ++i) {
    X509* cert = sk_X509_value(certs.get(), i);
    if (!cert) {
      LOG(WARNING) << MakePkcs12CertImportErrorMessage(
          Pkcs12ReaderStatusCode::kCertificateDataMissed);
      continue;
    }

    bool is_cert_related_to_key = false;
    Pkcs12ReaderStatusCode cert_to_key_check_result =
        pkcs12_reader.CheckRelation(key_data, cert, is_cert_related_to_key);
    if (cert_to_key_check_result != Pkcs12ReaderStatusCode::kSuccess) {
      LOG(ERROR) << MakePkcs12CertImportErrorMessage(cert_to_key_check_result);
      continue;
    }
    if (!is_cert_related_to_key) {
      LOG(WARNING) << "Cert is not directly related to key, skipping";
      continue;
    }

    if (cert_nickname.empty()) {
      Pkcs12ReaderStatusCode get_cert_nickname_result =
          GetNickname(slot, cert, pkcs12_reader, cert_nickname);
      if (get_cert_nickname_result != Pkcs12ReaderStatusCode::kSuccess) {
        LOG(WARNING) << "Can not get nickname for the certificate due to: "
                     << MakePkcs12CertImportErrorMessage(
                            get_cert_nickname_result);
        continue;
      }
    }

    CertData& cert_data = valid_certs_data.emplace_back();
    cert_data.x509 = cert;
    cert_data.nickname = cert_nickname;
  }

  if (valid_certs_data.size() > 0) {
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  return Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound;
}

}  // namespace chromeos
