// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/helpers/kcer_chaps_util.h"

#include <dlfcn.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <pkcs11.h>
#include <pkcs11t.h>
#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "chromeos/ash/components/chaps_util/chaps_util_impl.h"
#include "chromeos/components/kcer/helpers/key_helper.h"
#include "chromeos/components/kcer/helpers/pkcs12_reader.h"
#include "chromeos/components/kcer/helpers/pkcs12_validator.h"
#include "crypto/chaps_support.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"
#include "third_party/boringssl/src/include/openssl/stack.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace kcer::internal {

namespace {

// Chaps sets this for keys that are software-backed.
constexpr char kPkcs12ImportFailed[] = "Chaps util PKCS12 import failed with ";
constexpr char kPkcs12KeyImportFailed[] = "Chaps util key import failed with ";
// Wraps public key and private key PKCS#11 object handles.
struct KeyPairHandles {
  CK_OBJECT_HANDLE public_key;
  CK_OBJECT_HANDLE private_key;
};

using Pkcs11Operation = base::RepeatingCallback<CK_RV()>;

// Performs |operation| and handles return values indicating that the PKCS11
// session has been closed by attempting to re-open the |chaps_session|.
// This is useful because the session could be closed e.g. because NSS could
// have called C_CloseAllSessions.
bool PerformWithRetries(chromeos::ChapsSlotSession* chaps_session,
                        base::StringPiece operation_name,
                        const Pkcs11Operation& operation) {
  const int kMaxAttempts = 5;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    CK_RV result = operation.Run();
    if (result == CKR_OK) {
      return true;
    }
    if (result != CKR_SESSION_HANDLE_INVALID && result != CKR_SESSION_CLOSED) {
      LOG(ERROR) << operation_name << " failed with " << result;
      return false;
    }
    if (!chaps_session->ReopenSession()) {
      return false;
    }
  }
  LOG(ERROR) << operation_name << " failed";
  return false;
}

std::string MakePkcs12KeyImportErrorMessage(Pkcs12ReaderStatusCode error_code) {
  return kPkcs12KeyImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}

std::string MakePkcs12ImportErrorMessage(Pkcs12ReaderStatusCode error_code) {
  return kPkcs12ImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}
Pkcs12ReaderStatusCode ImportRsaKey(chromeos::ChapsSlotSession* chaps_session,
                                    const KeyData& key_data,
                                    bool is_software_backed,
                                    const Pkcs12Reader* pkcs12_reader) {
  if (!key_data.key) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyDataMissed);
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  // All the data variables must stay alive until `attrs` is sent to Chaps.
  const RSA* rsa_key = EVP_PKEY_get0_RSA(key_data.key.get());
  const std::vector<uint8_t>& cka_id = key_data.cka_id_value;
  std::vector<uint8_t> public_modulus_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_n(rsa_key));
  std::vector<uint8_t> public_exponent_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_e(rsa_key));
  std::vector<uint8_t> private_exponent_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_d(rsa_key));
  std::vector<uint8_t> prime_factor_1 =
      pkcs12_reader->BignumToBytes(RSA_get0_p(rsa_key));
  std::vector<uint8_t> prime_factor_2 =
      pkcs12_reader->BignumToBytes(RSA_get0_q(rsa_key));
  std::vector<uint8_t> exponent_1 =
      pkcs12_reader->BignumToBytes(RSA_get0_dmp1(rsa_key));
  std::vector<uint8_t> exponent_2 =
      pkcs12_reader->BignumToBytes(RSA_get0_dmq1(rsa_key));
  std::vector<uint8_t> coefficient =
      pkcs12_reader->BignumToBytes(RSA_get0_iqmp(rsa_key));

  if (public_modulus_bytes.empty() || cka_id.empty() ||
      public_exponent_bytes.empty() || private_exponent_bytes.empty() ||
      prime_factor_1.empty() || prime_factor_2.empty() || exponent_1.empty() ||
      exponent_2.empty() || coefficient.empty()) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyAttrDataMissing);
    return Pkcs12ReaderStatusCode::kKeyAttrDataMissing;
  }

  CK_BBOOL true_value = CK_TRUE;
  CK_OBJECT_CLASS key_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_BBOOL force_software_attribute = is_software_backed ? CK_TRUE : CK_FALSE;
  CK_OBJECT_HANDLE out_key_handle;
  CK_ATTRIBUTE attrs[] = {
      {CKA_CLASS, &key_class, sizeof(key_class)},
      {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_SENSITIVE, &true_value, sizeof(true_value)},
      {chaps::kForceSoftwareAttribute, &force_software_attribute,
       sizeof(true_value)},
      {CKA_PRIVATE, &true_value, sizeof(true_value)},
      {CKA_UNWRAP, &true_value, sizeof(true_value)},
      {CKA_DECRYPT, &true_value, sizeof(true_value)},
      {CKA_SIGN, &true_value, sizeof(true_value)},
      {CKA_SIGN_RECOVER, &true_value, sizeof(true_value)},
      {CKA_MODULUS, const_cast<uint8_t*>(public_modulus_bytes.data()),
       public_modulus_bytes.size()},
      {CKA_ID, const_cast<uint8_t*>(cka_id.data()), cka_id.size()},
      {CKA_PUBLIC_EXPONENT, public_exponent_bytes.data(),
       public_exponent_bytes.size()},
      {CKA_PRIVATE_EXPONENT, private_exponent_bytes.data(),
       private_exponent_bytes.size()},
      {CKA_PRIME_1, prime_factor_1.data(), prime_factor_1.size()},
      {CKA_PRIME_2, prime_factor_2.data(), prime_factor_2.size()},
      {CKA_EXPONENT_1, exponent_1.data(), exponent_1.size()},
      {CKA_EXPONENT_2, exponent_2.data(), exponent_2.size()},
      {CKA_COEFFICIENT, coefficient.data(), coefficient.size()}};

  if (!PerformWithRetries(
          chaps_session, "CreateObject",
          base::BindRepeating(&chromeos::ChapsSlotSession::CreateObject,
                              base::Unretained(chaps_session), attrs,
                              /*ulCount=*/std::size(attrs), &out_key_handle))) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateKeyFailed);
    return Pkcs12ReaderStatusCode::kCreateKeyFailed;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ImportEcKey(chromeos::ChapsSlotSession* chaps_session,
                                   const KeyData& key_data,
                                   bool is_software_backed,
                                   const Pkcs12Reader* pkcs12_reader) {
  if (!key_data.key) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyDataMissed);
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  // All the data variables must stay alive until `attrs` is sent to Chaps.
  const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key_data.key.get());
  if (!ec_key) {
    return Pkcs12ReaderStatusCode::kEcKeyExtractionFailed;
  }
  const std::vector<uint8_t>& private_value = GetEcPrivateKeyBytes(ec_key);
  const std::vector<uint8_t>& cka_id = key_data.cka_id_value;
  const std::vector<uint8_t>& pub_key = GetEcPublicKeyBytes(ec_key);

  if (private_value.empty() || cka_id.empty() || pub_key.empty()) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyAttrDataMissing);
    return Pkcs12ReaderStatusCode::kKeyAttrDataMissing;
  }

  bssl::ScopedCBB cbb;
  uint8_t* ec_params_der = nullptr;
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  size_t ec_params_der_len = 0;
  if (!CBB_init(cbb.get(), 0) || !EC_KEY_marshal_curve_name(cbb.get(), group) ||
      !CBB_finish(cbb.get(), &ec_params_der, &ec_params_der_len)) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateKeyFailed);
    return Pkcs12ReaderStatusCode::kCreateKeyFailed;
  }
  bssl::UniquePtr<uint8_t> der_deleter(ec_params_der);

  CK_BBOOL true_value = CK_TRUE;
  CK_OBJECT_CLASS key_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_EC;
  CK_BBOOL force_software_attribute = is_software_backed ? CK_TRUE : CK_FALSE;
  CK_OBJECT_HANDLE out_key_handle;

  CK_ATTRIBUTE attrs[] = {
      {CKA_CLASS, &key_class, sizeof(key_class)},
      {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_SENSITIVE, &true_value, sizeof(true_value)},
      {chaps::kForceSoftwareAttribute, &force_software_attribute,
       sizeof(force_software_attribute)},
      {CKA_SIGN, &true_value, sizeof(true_value)},
      {CKA_SIGN_RECOVER, &true_value, sizeof(true_value)},
      {CKA_DERIVE, &true_value, sizeof(true_value)},
      {CKA_ID, const_cast<uint8_t*>(cka_id.data()), cka_id.size()},
      {CKA_VALUE, const_cast<uint8_t*>(private_value.data()),
       private_value.size()},
      {CKA_EC_POINT, const_cast<uint8_t*>(pub_key.data()), pub_key.size()},
      {CKA_PRIVATE, &true_value, sizeof(true_value)},
      {CKA_EC_PARAMS, ec_params_der, ec_params_der_len},
  };

  if (!PerformWithRetries(
          chaps_session, "CreateObject",
          base::BindRepeating(&chromeos::ChapsSlotSession::CreateObject,
                              base::Unretained(chaps_session), attrs,
                              /*ulCount=*/std::size(attrs), &out_key_handle))) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateKeyFailed);
    return Pkcs12ReaderStatusCode::kCreateKeyFailed;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ImportOneCert(chromeos::ChapsSlotSession* chaps_session,
                                     const CertData& cert_data,
                                     const std::vector<uint8_t>& id,
                                     const Pkcs12Reader* pkcs12_helper,
                                     bool is_software_backed) {
  if (!cert_data.x509) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCertificateDataMissed);
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }
  X509* cert = cert_data.x509.get();

  CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
  CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
  CK_BBOOL true_value = CK_TRUE;

  int cert_der_size = 0;
  bssl::UniquePtr<uint8_t> cert_der;
  Pkcs12ReaderStatusCode get_cert_der_result =
      pkcs12_helper->GetDerEncodedCert(cert, cert_der, cert_der_size);

  if (get_cert_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_cert_der_result);
    return get_cert_der_result;
  }

  base::span<const uint8_t> issuer_name_data;
  Pkcs12ReaderStatusCode get_issuer_name_der_result =
      pkcs12_helper->GetIssuerNameDer(cert, issuer_name_data);
  if (get_issuer_name_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_issuer_name_der_result);
    return get_issuer_name_der_result;
  }

  base::span<const uint8_t> subject_name_data;
  Pkcs12ReaderStatusCode get_subject_name_der_result =
      pkcs12_helper->GetSubjectNameDer(cert, subject_name_data);
  if (get_subject_name_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_subject_name_der_result);
    return get_subject_name_der_result;
  }

  int serial_number_der_size = 0;
  bssl::UniquePtr<uint8_t> serial_number_der;
  Pkcs12ReaderStatusCode get_serial_der_result =
      pkcs12_helper->GetSerialNumberDer(cert, serial_number_der,
                                        serial_number_der_size);
  if (get_serial_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_serial_der_result);
    return get_serial_der_result;
  }

  std::string label = cert_data.nickname;

  CK_BBOOL force_software_attribute = is_software_backed ? CK_TRUE : CK_FALSE;

  CK_ATTRIBUTE attrs[] = {
      {CKA_CLASS, &cert_class, sizeof(cert_class)},
      {CKA_CERTIFICATE_TYPE, &cert_type, sizeof(cert_type)},
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {chaps::kForceSoftwareAttribute, &force_software_attribute,
       sizeof(CK_BBOOL)},
      {CKA_ID, const_cast<uint8_t*>(id.data()), id.size()},
      {CKA_LABEL, label.data(), label.size()},
      {CKA_VALUE, cert_der.get(),
       base::saturated_cast<CK_ULONG>(cert_der_size)},
      {CKA_ISSUER, const_cast<uint8_t*>(issuer_name_data.data()),
       issuer_name_data.size()},
      {CKA_SUBJECT, const_cast<uint8_t*>(subject_name_data.data()),
       subject_name_data.size()},
      {CKA_SERIAL_NUMBER, serial_number_der.get(),
       base::saturated_cast<CK_ULONG>(serial_number_der_size)}};

  CK_OBJECT_HANDLE cert_handle;
  if (!PerformWithRetries(
          chaps_session, "CreateObject",
          base::BindRepeating(&chromeos::ChapsSlotSession::CreateObject,
                              base::Unretained(chaps_session), attrs,
                              /*ulCount=*/std::size(attrs), &cert_handle))) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateCertFailed);
    return Pkcs12ReaderStatusCode::kCreateCertFailed;
  }

  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ImportAllCerts(chromeos::ChapsSlotSession* chaps_session,
                                      std::vector<CertData>& certs_data,
                                      const std::vector<uint8_t>& id,
                                      const Pkcs12Reader* pkcs12_helper,
                                      bool is_software_backed) {
  if (certs_data.empty()) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCertificateDataMissed);
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }

  Pkcs12ReaderStatusCode is_every_cert_imported =
      Pkcs12ReaderStatusCode::kSuccess;
  for (CertData& cert_data : certs_data) {
    if (ImportOneCert(chaps_session, cert_data, id, pkcs12_helper,
                      is_software_backed) != Pkcs12ReaderStatusCode::kSuccess) {
      is_every_cert_imported = Pkcs12ReaderStatusCode::kFailureDuringCertImport;
    }
  }
  return is_every_cert_imported;
}

KcerChapsUtil::FactoryCallback& GetFactoryCallback() {
  static base::NoDestructor<KcerChapsUtil::FactoryCallback> s_callback;
  return *s_callback;
}

}  // namespace

KcerChapsUtil::KcerChapsUtil(std::unique_ptr<chromeos::ChapsSlotSessionFactory>
                                 chaps_slot_session_factory)
    : chaps_slot_session_factory_(std::move(chaps_slot_session_factory)) {}
KcerChapsUtil::~KcerChapsUtil() = default;

// static
std::unique_ptr<KcerChapsUtil> KcerChapsUtil::Create() {
  if (!GetFactoryCallback().is_null()) {
    return GetFactoryCallback().Run();
  }
  return std::make_unique<KcerChapsUtil>(
      std::make_unique<chromeos::ChapsSlotSessionFactoryImpl>());
}

// static
void KcerChapsUtil::SetFactoryForTesting(const FactoryCallback& factory) {
  DCHECK(factory.is_null() || GetFactoryCallback().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetFactoryCallback() = factory;
}

bool KcerChapsUtil::ImportPkcs12Certificate(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    bool is_software_backed) {
  return ImportPkcs12CertificateImpl(slot, pkcs12_data, password,
                                     is_software_backed);
}

bool KcerChapsUtil::ImportPkcs12CertificateImpl(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    const bool is_software_backed,
    const Pkcs12Reader& pkcs12_reader) {
  std::unique_ptr<chromeos::ChapsSlotSession> chaps_session =
      GetChapsSlotSessionForSlot(slot);
  if (!chaps_session) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(
        Pkcs12ReaderStatusCode::kChapsSessionMissed);
    return false;
  }

  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  Pkcs12ReaderStatusCode get_key_and_cert_status =
      pkcs12_reader.GetPkcs12KeyAndCerts(pkcs12_data, password, key_data.key,
                                         certs);
  if (get_key_and_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    uint32_t error = ERR_get_error();
    char ebuf[255];
    LOG(ERROR) << "PKCS#12 import failed with error "
               << ERR_error_string_n(error, ebuf, sizeof(ebuf));
    LOG(ERROR) << MakePkcs12ImportErrorMessage(get_key_and_cert_status);
    return false;
  }

  Pkcs12ReaderStatusCode enrich_key_data_result =
      pkcs12_reader.EnrichKeyData(key_data);
  if (enrich_key_data_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(enrich_key_data_result);
    return false;
  }

  // `certs` are empty after this code block.
  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      slot, pkcs12_reader, std::move(certs), key_data, certs_data);
  if (prepare_certs_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(prepare_certs_status);
    return false;
  }

  bool is_key_installed = false;
  Pkcs12ReaderStatusCode key_installed_result = CanFindInstalledKey(
      slot, certs_data.front(), pkcs12_reader, is_key_installed);
  if (key_installed_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << "Failed to find installed key in slot due to: "
               << MakePkcs12CertImportErrorMessage(key_installed_result);
    return false;
  }

  if (!is_key_installed) {
    Pkcs12ReaderStatusCode import_key_status =
        Pkcs12ReaderStatusCode::kKeyAttrDataMissing;
    if (IsKeyRsaType(key_data.key)) {
      import_key_status = ImportRsaKey(chaps_session.get(), key_data,
                                       is_software_backed, &pkcs12_reader);
    } else if (IsKeyEcType(key_data.key)) {
      import_key_status = ImportEcKey(chaps_session.get(), key_data,
                                      is_software_backed, &pkcs12_reader);
    } else {
      LOG(ERROR) << "Not supported key type";
      return false;
    }

    if (import_key_status != Pkcs12ReaderStatusCode::kSuccess) {
      LOG(ERROR) << MakePkcs12ImportErrorMessage(import_key_status);
      return false;
    }
  }
  // Same id will be used for the key and certs.
  Pkcs12ReaderStatusCode import_certs_status =
      ImportAllCerts(chaps_session.get(), certs_data, key_data.cka_id_value,
                     &pkcs12_reader, is_software_backed);
  if (import_certs_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(import_certs_status);
    return false;
  }

  return true;
}

std::unique_ptr<chromeos::ChapsSlotSession>
KcerChapsUtil::GetChapsSlotSessionForSlot(PK11SlotInfo* slot) {
  if (!slot || (!is_chaps_provided_slot_for_testing_ &&
                !crypto::IsSlotProvidedByChaps(slot))) {
    return nullptr;
  }

  // Note that ChapsSlotSession(Factory) expects something else to have called
  // C_Initialize. It is a safe assumption that NSS has called C_Initialize for
  // chaps if |slot| is actually a chaps-provided slot, which is verified above.
  return chaps_slot_session_factory_->CreateChapsSlotSession(
      PK11_GetSlotID(slot));
}

}  // namespace kcer::internal
