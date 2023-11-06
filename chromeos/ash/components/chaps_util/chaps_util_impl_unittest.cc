// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/chaps_util_impl.h"

#include "base/base64.h"
#include "base/files/file_util.h"

#include <pkcs11t.h>
#include <secmodt.h>

#include <map>
#include <utility>
#include <vector>

#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "chromeos/ash/components/chaps_util/pkcs12_reader.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace chromeos {
namespace {

const size_t kKeySizeBits = 2048;

// TODO(b/202374261): Move these into a shared header.
// Signals to chaps that a generated key should be software-backed.
constexpr CK_ATTRIBUTE_TYPE kForceSoftwareAttribute = CKA_VENDOR_DEFINED + 4;
// Chaps sets this for keys that are software-backed.
constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;

enum AttrValueType { kNotDefined, kCkBool, kCkUlong, kCkBytes };
const char kPkcs12FilePassword[] = "12345";
const absl::optional<std::vector<CK_BYTE>> default_encoded_cert_label =
    base::Base64Decode("dGVzdHVzZXJjZXJ0");
// python print(base64.b64encode("default nickname".encode('utf-8'))).
const absl::optional<std::vector<CK_BYTE>> default_encoded_label =
    base::Base64Decode("VW5rbm93biBvcmc=");

// Class helper to keep relations between all possible attribute's types,
// attribute's names and attribute's value types.
class AttributesParsingOptions {
 public:
  AttributesParsingOptions() = default;
  ~AttributesParsingOptions() = default;

  static std::string GetName(const CK_ATTRIBUTE& attribute) {
    if (!GetPkcs12ObjectAttrMap().contains(attribute.type)) {
      ADD_FAILURE() << "Attribute value's type is unknown hex:" << std::hex
                    << attribute.type;
      return "";
    }
    return std::get<std::string>(GetPkcs12ObjectAttrMap().at(attribute.type));
  }

  static AttrValueType GetValueType(const CK_ATTRIBUTE& attribute) {
    if (!GetPkcs12ObjectAttrMap().contains(attribute.type)) {
      ADD_FAILURE() << "Attribute value's type is unknown hex:" << std::hex
                    << attribute.type;
      return AttrValueType::kNotDefined;
    }
    return std::get<AttrValueType>(GetPkcs12ObjectAttrMap().at(attribute.type));
  }

 private:
  static const std::map<CK_ATTRIBUTE_TYPE,
                        std::pair<AttrValueType, std::string>>&
  GetPkcs12ObjectAttrMap() {
    // Map which keeps relation between PKCS12 object attribute type, attribute
    // name and attribute value's type.
    static std::map<CK_ATTRIBUTE_TYPE, std::pair<AttrValueType, std::string>>
        attr_map;
    if (attr_map.empty()) {
      attr_map[CKA_TOKEN] = {kCkBool, "CKA_TOKEN"};
      attr_map[CKA_PRIVATE] = {kCkBool, "CKA_PRIVATE"};
      attr_map[CKA_VERIFY] = {kCkBool, "CKA_VERIFY"};
      attr_map[CKA_MODULUS_BITS] = {kCkUlong, "CKA_MODULUS_BITS"};
      attr_map[CKA_PUBLIC_EXPONENT] = {kCkBytes, "CKA_PUBLIC_EXPONENT"};
      attr_map[CKA_SENSITIVE] = {kCkBool, "CKA_SENSITIVE"};
      attr_map[CKA_EXTRACTABLE] = {kCkBool, "CKA_EXTRACTABLE"};
      attr_map[CKA_SIGN] = {kCkBool, "CKA_SIGN"};
      attr_map[kForceSoftwareAttribute] = {kCkBool, "kForceSoftwareAttribute"};
      attr_map[CKA_CLASS] = {kCkUlong, "CKA_CLASS"};
      attr_map[CKA_KEY_TYPE] = {kCkUlong, "CKA_KEY_TYPE"};
      attr_map[CKA_UNWRAP] = {kCkBool, "CKA_UNWRAP"};
      attr_map[CKA_DECRYPT] = {kCkBool, "CKA_DECRYPT"};
      attr_map[CKA_MODULUS] = {kCkBytes, "CKA_MODULUS"};
      attr_map[CKA_SIGN_RECOVER] = {kCkBool, "CKA_SIGN_RECOVER"};
      attr_map[CKA_ID] = {kCkBytes, "CKA_ID"};
      attr_map[CKA_PUBLIC_EXPONENT] = {kCkBytes, "CKA_PUBLIC_EXPONENT"};
      attr_map[CKA_PRIVATE_EXPONENT] = {kCkBytes, "CKA_PRIVATE_EXPONENT"};
      attr_map[CKA_PRIME_1] = {kCkBytes, "CKA_PRIME_1"};
      attr_map[CKA_PRIME_2] = {kCkBytes, "CKA_PRIME_2"};
      attr_map[CKA_EXPONENT_1] = {kCkBytes, "CKA_EXPONENT_1"};
      attr_map[CKA_EXPONENT_2] = {kCkBytes, "CKA_EXPONENT_2"};
      attr_map[CKA_COEFFICIENT] = {kCkBytes, "CKA_COEFFICIENT"};
      attr_map[CKA_LABEL] = {kCkBytes, "CKA_LABEL"};
      attr_map[CKA_VALUE] = {kCkBytes, "CKA_VALUE"};
      attr_map[CKA_ISSUER] = {kCkBytes, "CKA_ISSUER"};
      attr_map[CKA_SUBJECT] = {kCkBytes, "CKA_SUBJECT"};
      attr_map[CKA_SERIAL_NUMBER] = {kCkBytes, "CKA_SERIAL_NUMBER"};
      attr_map[CKA_NSS_EMAIL] = {kCkBytes, "CKA_NSS_EMAIL"};
      attr_map[CKA_CERTIFICATE_TYPE] = {kCkBytes, "CKA_CERTIFICATE_TYPE"};
    }
    return attr_map;
  }
};

// Generic holder for single parsed attribute with all parsing methods.
class AttributeData {
 public:
  AttributeData() = default;
  explicit AttributeData(const CK_ATTRIBUTE& attribute) {
    AttrValueType attr_value_type =
        AttributesParsingOptions::GetValueType(attribute);
    name_ = AttributesParsingOptions::GetName(attribute);
    switch (attr_value_type) {
      case kCkBool:
        ck_bool_value_ = ParseCkBBool(attribute, name_);
        break;
      case kCkUlong:
        ck_ulong_value_ = ParseCkULong(attribute, name_);
        break;
      case kCkBytes:
        ck_bytes_value_ = ParseCkBytes(attribute);
        break;
      case kNotDefined:
        ADD_FAILURE() << "Parser is not defined for attribute type:" << std::hex
                      << attribute.type;
        break;
    }
  }
  ~AttributeData() = default;

  absl::optional<CK_BBOOL> CkBool() { return ck_bool_value_; }

  absl::optional<CK_ULONG> CkULong() { return ck_ulong_value_; }

  absl::optional<std::vector<CK_BYTE>> CkByte() { return ck_bytes_value_; }

 private:
  std::string name_;
  absl::optional<CK_BBOOL> ck_bool_value_;
  absl::optional<CK_ULONG> ck_ulong_value_;
  absl::optional<std::vector<CK_BYTE>> ck_bytes_value_;

  static absl::optional<CK_BBOOL> ParseCkBBool(
      const CK_ATTRIBUTE& attribute,
      const std::string& attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_BBOOL)) {
      ADD_FAILURE() << "Size to small for CK_BBOOL for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return absl::nullopt;
    }
    CK_BBOOL value;
    memcpy(&value, attribute.pValue, sizeof(CK_BBOOL));
    return value;
  }

  static absl::optional<CK_ULONG> ParseCkULong(
      const CK_ATTRIBUTE& attribute,
      const std::string& attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_ULONG)) {
      ADD_FAILURE() << "Size to small for CK_ULONG for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return absl::nullopt;
    }
    CK_ULONG value;
    memcpy(&value, attribute.pValue, sizeof(CK_ULONG));
    return value;
  }

  static absl::optional<std::vector<CK_BYTE>> ParseCkBytes(
      const CK_ATTRIBUTE& attribute) {
    std::vector<CK_BYTE> result(attribute.ulValueLen);
    memcpy(result.data(), attribute.pValue, result.size());
    return result;
  }
};

// Holds PKCS#11 attributes passed by the code under test.
struct ObjectAttributes {
  ObjectAttributes() = default;
  ~ObjectAttributes() = default;

  static ObjectAttributes ParseFrom(CK_ATTRIBUTE_PTR attributes,
                                    CK_ULONG attributes_count) {
    ObjectAttributes result;
    for (CK_ULONG i = 0; i < attributes_count; ++i) {
      const CK_ATTRIBUTE& attr = attributes[i];
      if (result.parsed_attributes_map.contains(attr.type)) {
        ADD_FAILURE() << "Already stored attribute type:" << attr.type;
      }
      result.parsed_attributes_map[attr.type] = AttributeData(attr);
    }
    return result;
  }

  absl::optional<CK_BBOOL> GetCkBool(const CK_ATTRIBUTE_TYPE attribute_type) {
    return parsed_attributes_map[attribute_type].CkBool();
  }

  absl::optional<CK_ULONG> GetCkULong(const CK_ATTRIBUTE_TYPE attribute_type) {
    return parsed_attributes_map[attribute_type].CkULong();
  }

  absl::optional<std::vector<CK_BYTE>> GetCkByte(
      const CK_ATTRIBUTE_TYPE attribute_type) {
    return parsed_attributes_map[attribute_type].CkByte();
  }

  int Size() { return parsed_attributes_map.size(); }

  std::map<CK_ATTRIBUTE_TYPE, AttributeData> parsed_attributes_map;
};

// Holds
// - flags triggering how FakeChapsSlotSession should behave.
// - data passed by the code under test to FakeChapsSlotSessionFactory and
// FakeChapsSlotSession.
struct PassedData {
  // Controls whether ChapsSlotSessionFactory::CreateChapsSlotSession succeeds.
  bool factory_success = true;

  // Assigns results to operations. The key is the operation index, i.e. the
  // sequence number of an operation performed on the ChapsSlotSession.
  // The value is the operation result. CKR_INVALID_SESSION_HANDLE and
  // CKR_SESSION_CLOSED have special meaning.
  std::map<int, CK_RV> operation_results;

  // If set to false, calls to ChapsSlotSession::ReopenSession will fail.
  bool reopen_session_success = true;

  // Counts how often the code under test called
  // ChapsSlotSession::ReopenSession.
  int reopen_session_call_count = 0;

  // The slot_id passed into FakeChapsSlotSessionFactory.
  absl::optional<CK_SLOT_ID> slot_id;

  // Attributes passed for the secret key template to GenerateKey.
  ObjectAttributes secret_key_gen_attributes;

  // Attributes passed for the public key template to GenerateKeyPair.
  ObjectAttributes public_key_gen_attributes;

  // Attributes passed for the private key template to GenerateKeyPair.
  ObjectAttributes private_key_gen_attributes;

  // The data passed into FakeChapsSlotSession::SetAttributeValue for the
  // CKA_ID attribute of the public key. Empty if SetAttributeValue was never
  // called for that attribute.
  std::vector<uint8_t> public_key_cka_id;

  // The data passed into FakeChapsSlotSession::SetAttributeValue for the
  // CKA_ID attribute of the private key. Empty if SetAttributeValue was never
  // called for that attribute.
  std::vector<uint8_t> private_key_cka_id;

  // The data passed into FakeChapsSlotSession::SetAttributeValue for creation
  // of key object from PKCS12 container.
  ObjectAttributes pkcs12_key_attributes;

  // The data passed into FakeChapsSlotSession::SetAttributeValue for creation
  // of certificates objects from PKCS12 container. PKCS12 container can hold
  // multiple certificates.
  std::vector<ObjectAttributes> pkcs12_cert_attributes;
};

// FakeChapsSlotSession actually generates a key pair on a NSS slot. This is
// useful so it's possible to test whether the CKA_ID that the code under test
// would assign matches the CKA_ID that NSS computed for the key.
class FakeChapsSlotSession : public ChapsSlotSession {
 public:
  explicit FakeChapsSlotSession(PK11SlotInfo* slot, PassedData* passed_data)
      : slot_(slot), passed_data_(passed_data) {}
  ~FakeChapsSlotSession() override = default;

  bool ReopenSession() override {
    ++passed_data_->reopen_session_call_count;

    // The code under test should only call this if it was given an indication
    // that the session handle it uses is not valid anymore.
    EXPECT_FALSE(session_ok_);
    if (passed_data_->reopen_session_success) {
      session_ok_ = true;
      return true;
    }
    return false;
  }

  CK_RV CreateObject(CK_ATTRIBUTE_PTR pTemplate,
                     CK_ULONG ulCount,
                     CK_OBJECT_HANDLE_PTR phObject) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK) {
      return configured_result;
    }

    ObjectAttributes parsing_result =
        ObjectAttributes::ParseFrom(pTemplate, ulCount);

    AttributeData parsed_object_type =
        parsing_result.parsed_attributes_map[CKA_CLASS];
    if (parsed_object_type.CkULong() == CKO_PRIVATE_KEY) {
      passed_data_->pkcs12_key_attributes = parsing_result;
    }
    if (parsed_object_type.CkULong() == CKO_CERTIFICATE) {
      passed_data_->pkcs12_cert_attributes.push_back(parsing_result);
    }

    return CKR_OK;
  }

  CK_RV GenerateKey(CK_MECHANISM_PTR pMechanism,
                    CK_ATTRIBUTE_PTR pTemplate,
                    CK_ULONG ulCount,
                    CK_OBJECT_HANDLE_PTR phKey) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK) {
      return configured_result;
    }

    passed_data_->secret_key_gen_attributes =
        ObjectAttributes::ParseFrom(pTemplate, ulCount);

    // TODO(b/288880151): Finish fake implementation of `GenerateKey()`, when it
    // becomes necessary for testing `ChapsUtilImpl`.

    return CKR_OK;
  }

  CK_RV GenerateKeyPair(CK_MECHANISM_PTR pMechanism,
                        CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                        CK_ULONG ulPublicKeyAttributeCount,
                        CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                        CK_ULONG ulPrivateKeyAttributeCount,
                        CK_OBJECT_HANDLE_PTR phPublicKey,
                        CK_OBJECT_HANDLE_PTR phPrivateKey) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK) {
      return configured_result;
    }

    passed_data_->public_key_gen_attributes = ObjectAttributes::ParseFrom(
        pPublicKeyTemplate, ulPublicKeyAttributeCount);
    passed_data_->private_key_gen_attributes = ObjectAttributes::ParseFrom(
        pPrivateKeyTemplate, ulPrivateKeyAttributeCount);

    crypto::ScopedSECKEYPublicKey public_key;
    crypto::ScopedSECKEYPrivateKey private_key;
    EXPECT_TRUE(crypto::GenerateRSAKeyPairNSS(slot_, kKeySizeBits,
                                              /*permanent=*/true, &public_key,
                                              &private_key));
    *phPublicKey = public_key->pkcs11ID;
    public_key_handle_ = public_key->pkcs11ID;
    *phPrivateKey = private_key->pkcs11ID;
    private_key_handle_ = private_key->pkcs11ID;

    // Remember the modulus.
    SECItem* modulus = &(public_key->u.rsa.modulus);
    public_key_modulus_.assign(modulus->data, modulus->data + modulus->len);
    return CKR_OK;
  }

  CK_RV GetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK) {
      return configured_result;
    }

    if (hObject == public_key_handle_) {
      const size_t kModulusBytes = kKeySizeBits / 8;
      if (ulCount != 1 || pTemplate[0].type != CKA_MODULUS) {
        return CKR_ATTRIBUTE_TYPE_INVALID;
      }
      if (pTemplate[0].ulValueLen < kModulusBytes) {
        return CKR_BUFFER_TOO_SMALL;
      }
      memcpy(pTemplate[0].pValue, public_key_modulus_.data(), kModulusBytes);
      return CKR_OK;
    }
    if (hObject == private_key_handle_) {
      if (ulCount != 1 || pTemplate[0].type != kKeyInSoftware) {
        return CKR_ATTRIBUTE_TYPE_INVALID;
      }
      const CK_BBOOL key_in_software_value = true;
      if (pTemplate[0].ulValueLen < sizeof(key_in_software_value)) {
        return CKR_BUFFER_TOO_SMALL;
      }
      memcpy(pTemplate[0].pValue, &key_in_software_value,
             sizeof(key_in_software_value));
      return CKR_OK;
    }
    return CKR_OBJECT_HANDLE_INVALID;
  }

  CK_RV SetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK) {
      return configured_result;
    }

    if (ulCount != 1 || pTemplate[0].type != CKA_ID) {
      return CKR_ATTRIBUTE_TYPE_INVALID;
    }

    uint8_t* data = reinterpret_cast<uint8_t*>(pTemplate[0].pValue);
    size_t length = pTemplate[0].ulValueLen;
    if (hObject == public_key_handle_) {
      passed_data_->public_key_cka_id.assign(data, data + length);
      return CKR_OK;
    } else if (hObject == private_key_handle_) {
      passed_data_->private_key_cka_id.assign(data, data + length);
      return CKR_OK;
    }
    return CKR_OBJECT_HANDLE_INVALID;
  }

 private:
  // Applies a result configured for the current operation, if any.
  CK_RV ApplyConfiguredResult() {
    int cur_operation = operation_count_;
    ++operation_count_;

    auto operation_result = passed_data_->operation_results.find(cur_operation);
    if (operation_result == passed_data_->operation_results.end()) {
      return CKR_OK;
    }
    CK_RV result = operation_result->second;
    // CKR_SESSION_HANDLE_INVALID and CKR_SESSION_CLOSED have a special meaning
    // - also flag that the session handle is not usable (until the next call to
    // ReopenSession).
    if (result == CKR_SESSION_HANDLE_INVALID || result == CKR_SESSION_CLOSED) {
      session_ok_ = false;
    }
    return result;
  }

  // Keeps track of how many operations were already performed. Used to keep
  // track of the operation sequence number for PassedData::operation_results.
  int operation_count_ = 0;
  // If false, the session is not usable until ReopenSession has been called.
  bool session_ok_ = true;

  // Unowned.
  const raw_ptr<PK11SlotInfo, ExperimentalAsh> slot_;
  // Unowned.
  const raw_ptr<PassedData, ExperimentalAsh> passed_data_;

  // Cached modulus of the generated public key so GetAttributeValue with
  // CKA_MODULUS is supported.
  std::vector<uint8_t> public_key_modulus_;
  CK_OBJECT_HANDLE public_key_handle_ = CKR_OBJECT_HANDLE_INVALID;
  CK_OBJECT_HANDLE private_key_handle_ = CKR_OBJECT_HANDLE_INVALID;
};

class FakeChapsSlotSessionFactory : public ChapsSlotSessionFactory {
 public:
  FakeChapsSlotSessionFactory(PK11SlotInfo* slot, PassedData* passed_data)
      : slot_(slot), passed_data_(passed_data) {}
  ~FakeChapsSlotSessionFactory() override = default;

  std::unique_ptr<ChapsSlotSession> CreateChapsSlotSession(
      CK_SLOT_ID slot_id) override {
    passed_data_->slot_id = slot_id;
    if (!passed_data_->factory_success) {
      return nullptr;
    }
    return std::make_unique<FakeChapsSlotSession>(slot_, passed_data_);
  }

 private:
  // Unowned.
  const raw_ptr<PK11SlotInfo, ExperimentalAsh> slot_;
  // Unowned.
  const raw_ptr<PassedData, ExperimentalAsh> passed_data_;
};

// FakePkcs12Reader helper, by default it will call methods for the original
// object.
class FakePkcs12Reader : public Pkcs12Reader {
 public:
  FakePkcs12Reader() = default;
  ~FakePkcs12Reader() override = default;

  Pkcs12ReaderStatusCode GetPkcs12KeyAndCerts(
      const std::vector<uint8_t>& pkcs12_data,
      const std::string& password,
      bssl::UniquePtr<EVP_PKEY>& key,
      bssl::UniquePtr<STACK_OF(X509)>& certs) const override {
    get_pkcs12_key_and_cert_called_++;
    if (fake_certs_.get()) {
      certs = std::move(fake_certs_);
      return Pkcs12ReaderStatusCode::kSuccess;
    }

    if (get_pkcs12_key_and_cert_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_pkcs12_key_and_cert_status_;
    }
    return Pkcs12Reader::GetPkcs12KeyAndCerts(pkcs12_data, password, key,
                                              certs);
  }

  Pkcs12ReaderStatusCode GetDerEncodedCert(X509* cert,
                                           bssl::UniquePtr<uint8_t>& cert_der,
                                           int& cert_der_size) const override {
    get_der_encode_cert_called_++;
    if (get_der_encoded_cert_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_der_encoded_cert_status_;
    }
    return Pkcs12Reader::GetDerEncodedCert(cert, cert_der, cert_der_size);
  }

  Pkcs12ReaderStatusCode GetIssuerNameDer(
      X509* cert,
      base::span<const uint8_t>& issuer_name_data) const override {
    if (get_issuer_name_der_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_issuer_name_der_status_;
    }
    return Pkcs12Reader::GetIssuerNameDer(cert, issuer_name_data);
  }

  Pkcs12ReaderStatusCode GetSubjectNameDer(
      X509* cert,
      base::span<const uint8_t>& subject_name_data) const override {
    get_subject_name_der_called_++;
    if (get_subject_name_der_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_subject_name_der_status_;
    }
    return Pkcs12Reader::GetSubjectNameDer(cert, subject_name_data);
  }

  Pkcs12ReaderStatusCode GetSerialNumberDer(
      X509* cert,
      bssl::UniquePtr<uint8_t>& serial_number_der,
      int& serial_number_der_size) const override {
    if (get_serial_number_der_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_serial_number_der_status_;
    }
    return Pkcs12Reader::GetSerialNumberDer(cert, serial_number_der,
                                            serial_number_der_size);
  }

  Pkcs12ReaderStatusCode EnrichKeyData(KeyData& key_data) const override {
    get_key_data_called_++;
    if (get_key_data_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_key_data_status_;
    }
    return Pkcs12Reader::EnrichKeyData(key_data);
  }

  Pkcs12ReaderStatusCode CheckRelation(const KeyData& key_data,
                                       X509* cert,
                                       bool& is_related) const override {
    check_relation_data_called_++;
    if (check_relation_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return check_relation_status_;
    }
    return Pkcs12Reader::CheckRelation(key_data, cert, is_related);
  }

  Pkcs12ReaderStatusCode FindRawCertsWithSubject(
      PK11SlotInfo* slot,
      base::span<const uint8_t> required_subject_name,
      CERTCertificateList** found_certs) const override {
    find_raw_certs_with_subject_called_++;

    if (fake_some_certs_in_slot_) {
      // Some multi steps action here to mock returned CERTCertificateList with
      // one cert. This all should go away when PK11_FindRawCertsWithSubject is
      // replaced with a new function.
      scoped_refptr<net::X509Certificate> x509_cert =
          net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
      net::ScopedCERTCertificate nss_cert =
          net::x509_util::CreateCERTCertificateFromX509Certificate(
              x509_cert.get());
      *found_certs = CERT_CertListFromCert(nss_cert.get());

      return Pkcs12ReaderStatusCode::kSuccess;
    }

    if (find_raw_certs_with_subject_ != Pkcs12ReaderStatusCode::kSuccess) {
      return find_raw_certs_with_subject_;
    }
    return Pkcs12Reader::FindRawCertsWithSubject(slot, required_subject_name,
                                                 found_certs);
  }

  Pkcs12ReaderStatusCode GetLabel(X509* cert,
                                  std::string& label) const override {
    get_label_called_++;
    if (get_label_override_) {
      label = std::string();
      return Pkcs12ReaderStatusCode::kSuccess;
    }
    if (get_label_status_ != Pkcs12ReaderStatusCode::kSuccess) {
      return get_label_status_;
    }
    return Pkcs12Reader::GetLabel(cert, label);
  }

  Pkcs12ReaderStatusCode IsCertWithNicknameInSlots(
      const std::string& nickname_in,
      bool& is_nickname_present) const override {
    is_certs_with_nickname_in_slots_called_++;

    // Override allows to return first N request to isCertsWithNicknamesInSlot
    // with True and then return False, so can verify behaviour when nicknames
    // are found in slot.
    if (is_certs_with_nickname_in_slots_override_ > 0 &&
        is_certs_with_nickname_in_slots_override_ <
            is_certs_with_nickname_in_slots_called_) {
      is_nickname_present = false;
      return Pkcs12ReaderStatusCode::kSuccess;
    }

    // By default nothing is returned from PK11_FindCertsFromNickname() and
    // is_nickname_present will be false, this will override is_nickname_present
    // to true for tests.
    if (is_certs_nickname_used_) {
      is_nickname_present = is_certs_nickname_used_;
      return Pkcs12ReaderStatusCode::kSuccess;
    }

    if (is_certs_with_nickname_in_slot_status_ !=
        Pkcs12ReaderStatusCode::kSuccess) {
      return is_certs_with_nickname_in_slot_status_;
    }
    return Pkcs12Reader::IsCertWithNicknameInSlots(nickname_in,
                                                   is_nickname_present);
  }

  Pkcs12ReaderStatusCode DoesKeyForCertExist(
      PK11SlotInfo* slot,
      const Pkcs12ReaderCertSearchType cert_type,
      const scoped_refptr<net::X509Certificate>& cert) const override {
    find_key_by_cert_called_++;
    if (cert_type == Pkcs12ReaderCertSearchType::kPlainType &&
        find_key_by_cert_status_.has_value()) {
      return find_key_by_cert_status_.value();
    }
    if (cert_type == Pkcs12ReaderCertSearchType::kDerType &&
        find_key_by_der_cert_status_.has_value()) {
      return find_key_by_der_cert_status_.value();
    }
    return Pkcs12Reader::DoesKeyForCertExist(slot, cert_type, cert);
  }

  Pkcs12ReaderStatusCode GetCertFromDerData(
      const unsigned char* der_cert_data,
      int der_cert_len,
      bssl::UniquePtr<X509>& x509) const override {
    return Pkcs12Reader::GetCertFromDerData(der_cert_data, der_cert_len, x509);
  }

  std::vector<uint8_t> BignumToBytes(const BIGNUM* bignum) const override {
    if (bignum_to_bytes_value_) {
      return bignum_to_bytes_value_.value();
    }
    return Pkcs12Reader::BignumToBytes(bignum);
  }

  mutable int get_pkcs12_key_and_cert_called_ = 0;
  mutable bssl::UniquePtr<STACK_OF(X509)> fake_certs_;
  Pkcs12ReaderStatusCode get_pkcs12_key_and_cert_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int get_der_encode_cert_called_ = 0;
  Pkcs12ReaderStatusCode get_der_encoded_cert_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int get_issuer_name_der_called_ = 0;
  Pkcs12ReaderStatusCode get_issuer_name_der_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int get_subject_name_der_called_ = 0;
  Pkcs12ReaderStatusCode get_subject_name_der_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int get_serial_number_der_called_ = 0;
  Pkcs12ReaderStatusCode get_serial_number_der_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int find_raw_certs_with_subject_called_ = 0;
  Pkcs12ReaderStatusCode find_raw_certs_with_subject_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable bssl::UniquePtr<STACK_OF(X509)> certs_with_same_DN_override_;
  mutable bool fake_some_certs_in_slot_ = false;
  mutable int get_label_called_ = 0;
  mutable int get_label_override_ = false;
  Pkcs12ReaderStatusCode get_label_status_ = Pkcs12ReaderStatusCode::kSuccess;
  mutable int is_certs_with_nickname_in_slots_override_ = 0;
  mutable int is_certs_with_nickname_in_slots_called_ = 0;
  Pkcs12ReaderStatusCode is_certs_with_nickname_in_slot_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  bool is_certs_nickname_used_ = false;
  mutable int get_key_data_called_ = 0;
  Pkcs12ReaderStatusCode get_key_data_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int check_relation_data_called_ = 0;
  Pkcs12ReaderStatusCode check_relation_status_ =
      Pkcs12ReaderStatusCode::kSuccess;
  mutable int find_key_by_cert_called_ = 0;
  absl::optional<Pkcs12ReaderStatusCode> find_key_by_cert_status_;
  absl::optional<Pkcs12ReaderStatusCode> find_key_by_der_cert_status_;

  absl::optional<std::vector<uint8_t>> bignum_to_bytes_value_ = absl::nullopt;
};

class ChapsUtilImplTest : public ::testing::Test {
 public:
  ChapsUtilImplTest() {
    auto chaps_slot_session_factory =
        std::make_unique<FakeChapsSlotSessionFactory>(nss_test_db_.slot(),
                                                      &passed_data_);
    chaps_util_impl_ =
        std::make_unique<ChapsUtilImpl>(std::move(chaps_slot_session_factory));
    chaps_util_impl_->SetIsChapsProvidedSlotForTesting(true);
  }
  ChapsUtilImplTest(const ChapsUtilImplTest&) = delete;
  ChapsUtilImplTest& operator=(const ChapsUtilImplTest&) = delete;
  ~ChapsUtilImplTest() override = default;

 protected:
  static std::vector<uint8_t> ReadTestFile(const std::string& file_name) {
    base::FilePath file_path =
        net::GetTestCertsDirectory().AppendASCII(file_name);
    absl::optional<std::vector<uint8_t>> file_data = ReadFileToBytes(file_path);
    EXPECT_TRUE(file_data.has_value());
    if (!file_data.has_value()) {
      return {};
    }
    return file_data.value();
  }

  static std::vector<uint8_t>& GetPkcs12Data() {
    static std::vector<uint8_t> pkcs12_data_;
    if (pkcs12_data_.empty()) {
      pkcs12_data_ = ReadTestFile("client.p12");
    }
    return pkcs12_data_;
  }

  bool KeyImportNeverDone() const {
    ObjectAttributes data = passed_data_.pkcs12_key_attributes;
    return data.Size() == 0;
  }

  bool CertImportNeverDone() const {
    return passed_data_.pkcs12_cert_attributes.empty();
  }

  bool KeyImportDone() const {
    ObjectAttributes data = passed_data_.pkcs12_key_attributes;
    return data.Size() == 19;  // valid only for ReadTestFile("client.p12").
  }

  bool CertImportDone() const {
    ObjectAttributes data = passed_data_.pkcs12_cert_attributes[0];
    return data.Size() == 10;  // valid only for ReadTestFile("client.p12").
  }

  crypto::ScopedTestNSSDB nss_test_db_;
  PassedData passed_data_;

  std::unique_ptr<ChapsUtilImpl> chaps_util_impl_;
};

// Returns the CKA_ID of |private_key|. This is the CKA_ID that NSS assigned to
// the private key, and thus it should be the same CKA_ID that ChapsUtil
// attempts to assign to the private and public key.
std::vector<uint8_t> GetExpectedCkaId(SECKEYPrivateKey* private_key) {
  crypto::ScopedSECItem cka_id_secitem(
      PK11_GetLowLevelKeyIDForPrivateKey(private_key));
  uint8_t* cka_id_data = reinterpret_cast<uint8_t*>(cka_id_secitem->data);
  return {cka_id_data, cka_id_data + cka_id_secitem->len};
}

// Successfully generates a software-backed key pair. Also verifies CKA_ID
// assignment.
TEST_F(ChapsUtilImplTest, GenerateSoftwareKeyPairSuccess) {
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  ASSERT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));

  // Verify that ChapsUtil passed the expected attributes.
  // Check attributes for public key.
  ObjectAttributes public_key_data = passed_data_.public_key_gen_attributes;
  const int expected_public_key_attributes = 5;
  EXPECT_EQ(public_key_data.Size(), expected_public_key_attributes);
  EXPECT_EQ(public_key_data.GetCkBool(CKA_TOKEN), CK_TRUE);
  EXPECT_EQ(public_key_data.GetCkBool(CKA_PRIVATE), CK_FALSE);
  EXPECT_EQ(public_key_data.GetCkBool(CKA_VERIFY), CK_TRUE);
  EXPECT_EQ(public_key_data.GetCkULong(CKA_MODULUS_BITS), (CK_ULONG)2048);
  EXPECT_EQ(public_key_data.GetCkByte(CKA_PUBLIC_EXPONENT),
            (std::vector<CK_BYTE>{0x01, 0x00, 0x01}));

  // Check attributes for private key.
  ObjectAttributes private_key_data = passed_data_.private_key_gen_attributes;
  const int expected_private_key_attributes = 6;
  EXPECT_EQ(private_key_data.Size(), expected_private_key_attributes);
  EXPECT_EQ(private_key_data.GetCkBool(CKA_TOKEN), CK_TRUE);
  EXPECT_EQ(private_key_data.GetCkBool(CKA_PRIVATE), CK_TRUE);
  EXPECT_EQ(private_key_data.GetCkBool(CKA_SENSITIVE), CK_TRUE);
  EXPECT_EQ(private_key_data.GetCkBool(CKA_EXTRACTABLE), CK_FALSE);
  EXPECT_EQ(private_key_data.GetCkBool(kForceSoftwareAttribute), CK_TRUE);
  EXPECT_EQ(private_key_data.GetCkBool(CKA_SIGN), CK_TRUE);

  // Verify that ChapsUtil attempted to assign the correct CKA_ID to the public
  // and private key objects.
  std::vector<uint8_t> expected_cka_id = GetExpectedCkaId(private_key.get());
  EXPECT_EQ(passed_data_.public_key_cka_id, expected_cka_id);
  EXPECT_EQ(passed_data_.private_key_cka_id, expected_cka_id);
}

// Verify that ChapsUtil passed the correct slot id to the factory.
TEST_F(ChapsUtilImplTest, ImportPkcs12CertificateSuccessSlotOk) {
  chaps_util_impl_->ImportPkcs12Certificate(
      nss_test_db_.slot(), GetPkcs12Data(), kPkcs12FilePassword,
      /*is_software_backed=*/true);

  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));
}

// Successfully import public key and single certificate from PKCS12 file to
// Chaps software slot.
TEST_F(ChapsUtilImplTest, ImportPkcs12EnforceSoftwareBackedSuccess) {
  using OPTIONAL_CK_BYTE_VECTOR = absl::optional<std::vector<CK_BYTE>>;
  std::map<CK_ATTRIBUTE_TYPE, OPTIONAL_CK_BYTE_VECTOR> expected_key_data;
  // Strings below have hardcoded fields from "client.p12" which is referenced
  // by GetPkcs12Data(), they are Base64Encoded for the shorter representation.
  // You can print original CkByte values from the key_data using this example:
  // std::cout << base::Base64Encode(std::move(*key_data.GetCkByte(CKA_LABEL)));
  expected_key_data[CKA_MODULUS] = base::Base64Decode(
      "1JC7k5aWwwOpqoiNzoRHLRdmzH9h4kVmFlBU/vZ5e7hCSnnIbVJilMxDB+p0b7ozw1/"
      "bHvsRqikARkMc0OnC4EMnm6BEopqiyOnNGBy1qXwol5Mw8T8zwlzJl7FQdQdlH7pMxuID8hZ"
      "Eu8VkoEyLYJVJ1Ylaasc5BC0pHxZdNKk=");
  expected_key_data[CKA_ID] =
      base::Base64Decode("U65QueEa+ljfdKySfD6QbFrXEcM=");
  expected_key_data[CKA_PUBLIC_EXPONENT] = base::Base64Decode("AQAB");
  expected_key_data[CKA_PRIVATE_EXPONENT] = base::Base64Decode(
      "y/k2hiFy+h+BqArxSMLWKgbStlll7GL7212qsh6B5J6jviOumHj98BsyF1577"
      "NqY4VoSQmBaSxadFM9Bz5cBT8IrKr2/FjL1AC+wgdwUvGvbD426zN4Yb59cTf/"
      "bhNkvd2xocFPHeMDETFD6ISEcV6YLbPAtNlom7qVxlSTn1KE=");
  expected_key_data[CKA_PRIME_1] = base::Base64Decode(
      "8W127p18wtuvUBxz7MtZgAPk/1OGLj1RJghuVYbHaCJ9sT5AzK8eNcRqCld/"
      "bKABDdmYf3QHKYDx+vcrhcNF8w==");
  expected_key_data[CKA_PRIME_2] = base::Base64Decode(
      "4WVKE2h5oF7HYpX2sLgHXFhM77k6Hb1MalKk1MvXSYeKLnFf1Xh4Af2tUR73RmG/Mp/"
      "evvUMu6h4AvlGvn+18w==");
  expected_key_data[CKA_EXPONENT_1] = base::Base64Decode(
      "SUZzCXstKaspq4PnP2B8upj0APalzBT6MzPt4PF2RknpokkFu9oOrjz9/"
      "kOOPjbV+xEm8tAReGxVhVlNkVyyNw==");
  expected_key_data[CKA_EXPONENT_2] = base::Base64Decode(
      "DkFqwvl7n9H+yFR1ys2I4aVQEGVlsJXVbHAXrsHJtwPUkIVpK0Y4SN/"
      "zg0rzFsd94UTNQMSc7o2EMaP0fn3zUw==");
  expected_key_data[CKA_COEFFICIENT] = base::Base64Decode(
      "mV2Q/My7RVOOsSZGDEouCYMcVahOFWS84IcpYRwR9ds0KZ4hKcdyMGNR5/4ryvr9XMA+DBR/"
      "L9GBSWe6CeK3RQ==");

  std::map<CK_ATTRIBUTE_TYPE, OPTIONAL_CK_BYTE_VECTOR> expected_cert_data;
  // Strings below have hardcoded fields from "client.p12" which is referenced
  // by GetPkcs12Data(), they are Base64Encoded for shorter representation.
  // You can print original CkByte values from the key_data using this example:
  // std::cout << base::Base64Encode(std::move(*key_data.GetCkByte(CKA_LABEL)));
  expected_cert_data[CKA_CERTIFICATE_TYPE] = base::Base64Decode("AAAAAAAAAAA=");
  expected_cert_data[CKA_ID] =
      base::Base64Decode("U65QueEa+ljfdKySfD6QbFrXEcM=");
  expected_cert_data[CKA_LABEL] = default_encoded_cert_label;
  expected_cert_data[CKA_VALUE] = base::Base64Decode(
      "MIICpTCCAg6gAwIBAgIBATANBgkqhkiG9w0BAQUFADBWMQswCQYDVQQGEwJBVTETMBEGA1UE"
      "CBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMQ8wDQYD"
      "VQQDEwZ0ZXN0Y2EwIBcNMTAwNzMwMDEwMjEyWhgPMjA2MDA3MTcwMTAyMTJaMFwxCzAJBgNV"
      "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRlcm5ldCBXaWRnaXRz"
      "IFB0eSBMdGQxFTATBgNVBAMTDHRlc3R1c2VyY2VydDCBnzANBgkqhkiG9w0BAQEFAAOBjQAw"
      "gYkCgYEA1JC7k5aWwwOpqoiNzoRHLRdmzH9h4kVmFlBU/"
      "vZ5e7hCSnnIbVJilMxDB+p0b7ozw1/"
      "bHvsRqikARkMc0OnC4EMnm6BEopqiyOnNGBy1qXwol5Mw8T8zwlzJl7FQdQdlH7pMxuID8hZ"
      "Eu8VkoEyLYJVJ1Ylaasc5BC0pHxZdNKkCAwEAAaN7MHkwCQYDVR0TBAIwADAsBglghkgBhvh"
      "CAQ0EHxYdT3BlblNTTCBHZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0OBBYEFHqEH18NKRV"
      "bhkqTT8swZq22Dc4YMB8GA1UdIwQYMBaAFE8aGkwMhipgaDysVMfu3JaN29ILMA0GCSqGSIb"
      "3DQEBBQUAA4GBAKMT7cwjZtgmkFrJPAa/"
      "oOt1cdoBD7MqErx+tdvVN62q0h0Vl6UM3a94Ic0/"
      "sv1V8RT5TUYUyyuepr2Gm58uqkcbI3qflveVcvi96n7fCCo6NwxbKHmpVOx+"
      "wcPlHtjfek2KGQnee3mEN0YY/HOP5Rvj0Bh302kLrfgFx3xN1G5I");
  expected_cert_data[CKA_ISSUER] = base::Base64Decode(
      "MFYxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRlcm5l"
      "dCBXaWRnaXRzIFB0eSBMdGQxDzANBgNVBAMTBnRlc3RjYQ==");
  expected_cert_data[CKA_SUBJECT] = base::Base64Decode(
      "MFwxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRlcm5l"
      "dCBXaWRnaXRzIFB0eSBMdGQxFTATBgNVBAMTDHRlc3R1c2VyY2VydA==");
  expected_cert_data[CKA_SERIAL_NUMBER] = base::Base64Decode("AgEB");
  chaps_util_impl_->ImportPkcs12Certificate(
      nss_test_db_.slot(), GetPkcs12Data(), kPkcs12FilePassword,
      /*is_software_backed=*/true);

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));

  // Verify that ChapsUtil passed the expected attributes.
  // Check attributes for private key.
  ObjectAttributes key_data = passed_data_.pkcs12_key_attributes;
  const int expected_private_key_attributes = 19;
  EXPECT_EQ(key_data.Size(), expected_private_key_attributes);
  EXPECT_EQ(key_data.GetCkULong(CKA_CLASS), CKO_PRIVATE_KEY);
  EXPECT_EQ(key_data.GetCkULong(CKA_KEY_TYPE), CKK_RSA);
  EXPECT_EQ(key_data.GetCkBool(CKA_TOKEN), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_SENSITIVE), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(kForceSoftwareAttribute), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_PRIVATE), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_UNWRAP), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_DECRYPT), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_SIGN), CK_TRUE);
  EXPECT_EQ(key_data.GetCkBool(CKA_SIGN_RECOVER), CK_TRUE);
  EXPECT_EQ(key_data.GetCkByte(CKA_MODULUS), expected_key_data[CKA_MODULUS]);
  EXPECT_EQ(key_data.GetCkByte(CKA_ID), expected_key_data[CKA_ID]);
  EXPECT_EQ(key_data.GetCkByte(CKA_PUBLIC_EXPONENT),
            expected_key_data[CKA_PUBLIC_EXPONENT]);
  EXPECT_EQ(key_data.GetCkByte(CKA_PRIVATE_EXPONENT),
            expected_key_data[CKA_PRIVATE_EXPONENT]);
  EXPECT_EQ(key_data.GetCkByte(CKA_PRIME_1), expected_key_data[CKA_PRIME_1]);
  EXPECT_EQ(key_data.GetCkByte(CKA_PRIME_2), expected_key_data[CKA_PRIME_2]);
  EXPECT_EQ(key_data.GetCkByte(CKA_EXPONENT_1),
            expected_key_data[CKA_EXPONENT_1]);
  EXPECT_EQ(key_data.GetCkByte(CKA_EXPONENT_2),
            expected_key_data[CKA_EXPONENT_2]);
  EXPECT_EQ(key_data.GetCkByte(CKA_COEFFICIENT),
            expected_key_data[CKA_COEFFICIENT]);

  // Checking attributes for certificate.
  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];
  const int expected_cert_attributes = 10;
  EXPECT_EQ(cert_data.Size(), expected_cert_attributes);
  EXPECT_EQ(cert_data.GetCkBool(CKA_TOKEN), CK_TRUE);
  EXPECT_EQ(cert_data.GetCkULong(CKA_CLASS), CKO_CERTIFICATE);
  EXPECT_EQ(cert_data.GetCkByte(CKA_CERTIFICATE_TYPE),
            expected_cert_data[CKA_CERTIFICATE_TYPE]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_ID), expected_cert_data[CKA_ID]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_VALUE), expected_cert_data[CKA_VALUE]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_ISSUER), expected_cert_data[CKA_ISSUER]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_SUBJECT), expected_cert_data[CKA_SUBJECT]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_SERIAL_NUMBER),
            expected_cert_data[CKA_SERIAL_NUMBER]);
  EXPECT_EQ(cert_data.GetCkByte(CKA_LABEL), expected_cert_data[CKA_LABEL]);
}

// This is same test as ImportPkcs12EnforceSoftBackSuccess but with
// kKeyInSoftware = false, so key will be hardware backed.
// Only number of stored attributes and required minimum of values is checked,
// because we use same pkcs12 file "client.p12" and all values match already
// checked in  ImportPkcs12EnforceSoftwareBackedSuccess test.
TEST_F(ChapsUtilImplTest, ImportPkcs12HardwareBackedSuccess) {
  chaps_util_impl_->ImportPkcs12Certificate(
      nss_test_db_.slot(), GetPkcs12Data(), kPkcs12FilePassword,
      /*is_software_backed=*/false);

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));

  // Verify that ChapsUtil passed the expected attributes.
  // Check only kForceSoftwareAttribute attribute for private key.
  ObjectAttributes key_data = passed_data_.pkcs12_key_attributes;
  const int expected_private_key_attributes = 19;
  EXPECT_EQ(key_data.Size(), expected_private_key_attributes);
  EXPECT_EQ(key_data.GetCkULong(CKA_CLASS), CKO_PRIVATE_KEY);
  EXPECT_EQ(key_data.GetCkULong(CKA_KEY_TYPE), CKK_RSA);
  EXPECT_EQ(key_data.GetCkBool(kForceSoftwareAttribute), CK_FALSE);

  // Check only number of attributes for certificate.
  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];
  const int expected_cert_attributes = 10;
  EXPECT_EQ(cert_data.Size(), expected_cert_attributes);
}

// The passed slot is not provided by chaps. The operation fails.
TEST_F(ChapsUtilImplTest, NotChapsProvidedSlot) {
  chaps_util_impl_->SetIsChapsProvidedSlotForTesting(false);

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
}

// A ChapsSlotSession can not be created, so the operation fails.
TEST_F(ChapsUtilImplTest, ChapsSlotSessionFactoryFailure) {
  passed_data_.factory_success = false;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));
}

// A PKCS11 operation fails with a generic failure. The operation fails.
TEST_F(ChapsUtilImplTest, OperationFails) {
  passed_data_.operation_results[0] = CKR_FUNCTION_FAILED;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID. ChapsUtilImpl
// re-opens the session and retries the operation.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenOk) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 1);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID twice. ChapsUtilImpl
// re-opens the session and retries the operation.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenTwiceOk) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[1] = CKR_SESSION_CLOSED;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 2);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID many times.
// ChapsUtilImpl gives up attempts to retry after 5 times.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenGivesUp) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[1] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[2] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[3] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[4] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[5] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 5);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID and the session can
// not be re-opened. The operation fails.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenFails) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = false;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 1);
}

class ChapsUtilPKCS12ImportTest : public ChapsUtilImplTest {
 public:
  ChapsUtilPKCS12ImportTest() {
    GetPkcs12Data();
    EXPECT_FALSE(GetPkcs12Data().empty());
  }

  bool RunImportPkcs12Certificate() {
    return chaps_util_impl_->ImportPkcs12CertificateImpl(
        nss_test_db_.slot(), GetPkcs12Data(), kPkcs12FilePassword,
        /*is_software_backed=*/true, fake_pkcs12_reader_);
  }

  FakePkcs12Reader fake_pkcs12_reader_;
};

TEST_F(ChapsUtilPKCS12ImportTest, DefaultCasePKCS12ImportSuccessful) {
  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(import_result, true);
}

TEST_F(ChapsUtilPKCS12ImportTest, NoChapsSessionPKCS12ImportFailed) {
  bool import_result = chaps_util_impl_->ImportPkcs12CertificateImpl(
      /*slot=*/nullptr, GetPkcs12Data(), kPkcs12FilePassword,
      /*is_software_backed=*/true, fake_pkcs12_reader_);

  EXPECT_EQ(import_result, false);
}

// Failed import PKCS12 due to empty keys.
TEST_F(ChapsUtilPKCS12ImportTest, EmptyKeyPtrPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_pkcs12_key_and_cert_status_ =
      Pkcs12ReaderStatusCode::kKeyExtractionFailed;
  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

// Failed import PKCS12 due to missed key attribute.
TEST_F(ChapsUtilPKCS12ImportTest, MissedKeyAttributePKCS12ImportFailed) {
  // This will set all attributes to empty.
  fake_pkcs12_reader_.bignum_to_bytes_value_ = std::vector<uint8_t>();
  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, ImportOfKeyFailedPKCS12ImportFailed) {
  // Mock CreateObject operations result.
  passed_data_.operation_results[0] = CKR_GENERAL_ERROR;

  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, FailedGetCertDerPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_der_encoded_cert_status_ =
      Pkcs12ReaderStatusCode::kKeyExtractionFailed;

  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, FailedGetIssuerNameDerPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_issuer_name_der_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertIssuerDerNameFailed;

  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, FailedGetSubjectNameDerPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_subject_name_der_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameDerFailed;

  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, FailedGetSerialNumberDerPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_serial_number_der_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertSerialNumberDerFailed;

  bool import_result = RunImportPkcs12Certificate();
  EXPECT_EQ(import_result, false);
}

TEST_F(ChapsUtilPKCS12ImportTest, CertObjectCreationFailedPKCS12ImportFailed) {
  // Mock CreateObject operations result for key import.
  passed_data_.operation_results[0] = CKR_OK;
  // Mock CreateObject operations result for the certificate import.
  passed_data_.operation_results[1] = CKR_GENERAL_ERROR;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_FALSE(import_result);
}

// Empty list returned for certificates from GetPkcs12KeyAndCerts, key is ok.
// Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, NoCertsForValidationPKCS12ImportFailed) {
  fake_pkcs12_reader_.fake_certs_ =
      bssl::UniquePtr<STACK_OF(X509)>(sk_X509_new_null());

  bool import_result = chaps_util_impl_->ImportPkcs12CertificateImpl(
      nss_test_db_.slot(), GetPkcs12Data(), kPkcs12FilePassword,
      /*is_software_backed=*/true, fake_pkcs12_reader_);

  EXPECT_EQ(fake_pkcs12_reader_.get_pkcs12_key_and_cert_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.get_key_data_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.check_relation_data_called_, 0);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// GetKeyData failed to extract data for the key, validation failed.
// Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, GetKeyDataFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_key_data_status_ =
      Pkcs12ReaderStatusCode::kKeyDataMissed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_key_data_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.check_relation_data_called_, 0);

  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// CheckRelation between cert and key failed, validation failed. Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, CheckRelationFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.check_relation_status_ =
      Pkcs12ReaderStatusCode::kKeyDataMissed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.check_relation_data_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.get_subject_name_der_called_, 0);

  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// Cert is not related to key, validation failed. Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, CertNotRelatedToKeyPKCS12ImportFailed) {
  fake_pkcs12_reader_.check_relation_status_ =
      Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.check_relation_data_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.get_subject_name_der_called_, 0);

  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// Cert has no DER subject name, GetNickname failed, validation failed. Import
// failed.
TEST_F(ChapsUtilPKCS12ImportTest, CertHasNoDERSubjectNamePKCS12ImportFailed) {
  fake_pkcs12_reader_.get_subject_name_der_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameMissed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_subject_name_der_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.find_raw_certs_with_subject_called_, 0);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// FindRawCertsWithSubject failed during searching for cert with required
// subject in slot. GetNickname failed, validation failed. Import failed.
TEST_F(ChapsUtilPKCS12ImportTest,
       FindRawCertsWithSubjectFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.find_raw_certs_with_subject_ =
      Pkcs12ReaderStatusCode::kPkcs12FindCertsWithSubjectFailed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_subject_name_der_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.find_raw_certs_with_subject_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 0);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// There is one certificate with the same subject in slot, but GetLabel for it
// failed. Import successful with the currents cert's nickname.
TEST_F(ChapsUtilPKCS12ImportTest,
       GetLabelForFoundCertFailedPKCS12ImportSucess) {
  fake_pkcs12_reader_.fake_some_certs_in_slot_ = true;
  fake_pkcs12_reader_.get_label_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 2);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_TRUE(KeyImportDone());
  EXPECT_TRUE(import_result);
}

// There is one certificate with the same subject in slot. Import successful
// with already stored test cert's nickname.
TEST_F(ChapsUtilPKCS12ImportTest,
       CertWithSameSubjectInSlotPKCS12ImportSuccess) {
  fake_pkcs12_reader_.fake_some_certs_in_slot_ = true;
  // python print(base64.b64encode("127.0.0.1".encode('utf-8'))).
  auto expected_encoded_label = base::Base64Decode("MTI3LjAuMC4x");

  bool import_result = RunImportPkcs12Certificate();

  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];
  EXPECT_EQ(cert_data.GetCkByte(CKA_LABEL), expected_encoded_label);
  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 0);
  EXPECT_TRUE(KeyImportDone());
  EXPECT_TRUE(import_result);
}

// There is one certificate with the same subject, but GetLabel for it returns
// empty string. Import successful with the cert's nickname.
TEST_F(ChapsUtilPKCS12ImportTest, GetLabelReturnsEmptyPKCS12ImportSuccess) {
  fake_pkcs12_reader_.fake_some_certs_in_slot_ = true;
  fake_pkcs12_reader_.get_label_override_ = true;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 2);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_TRUE(KeyImportDone());
  EXPECT_TRUE(import_result);
}

// No certificate with the same subject exists, GetLabel for current cert
// failed, import is successful with default label.
TEST_F(ChapsUtilPKCS12ImportTest, GetLabelFailedPKCS12ImportSuccess) {
  fake_pkcs12_reader_.get_label_status_ =
      Pkcs12ReaderStatusCode::kPkcs12LabelCreationFailed;

  bool import_result = RunImportPkcs12Certificate();

  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];
  EXPECT_EQ(cert_data.GetCkByte(CKA_LABEL), default_encoded_label);
  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_TRUE(KeyImportDone());
  EXPECT_TRUE(import_result);
}

// No certificate with the same subject in slot, GetLabel for the current cert
// returned empty string, import is successful with default label.
TEST_F(ChapsUtilPKCS12ImportTest, GetLabelReturnEmptyPKCS12ImportSuccess) {
  fake_pkcs12_reader_.get_label_override_ = true;

  bool import_result = RunImportPkcs12Certificate();

  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];
  EXPECT_EQ(cert_data.GetCkByte(CKA_LABEL), default_encoded_label);

  EXPECT_EQ(fake_pkcs12_reader_.get_label_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_TRUE(KeyImportDone());
  EXPECT_TRUE(import_result);
}

// No certificate with same subject exists, MakeNicknameUnique failed, import
// failed.
TEST_F(ChapsUtilPKCS12ImportTest, MakeNicknameUniqueFailedPKCS12ImportFailed) {
  // Setting is_certs_nickname_used = true will lead to fail of making label
  // unique, it will increase counter to 100 and at the end return
  // Pkcs12ReaderStatusCode::kReachedMaxAttemptForUniqueness.
  fake_pkcs12_reader_.is_certs_nickname_used_ = true;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 100);

  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// No certificate with same subject exists, MakeNicknameUnique is called, but
// isCertsWithNicknamesInSlot has failed.  Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, CertsSearchInSlotFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.is_certs_with_nickname_in_slot_status_ =
      Pkcs12ReaderStatusCode::kPkcs12MissedNickname;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// 20 certificates with same subject already exists in slot, import successful.
// cert nicknames in slot will be 'testusercert', 'testusercert 1', ...,
// 'testusercert 19'.
TEST_F(ChapsUtilPKCS12ImportTest, CertsSearchInSlot20TimesPKCS12ImportFailed) {
  fake_pkcs12_reader_.is_certs_with_nickname_in_slots_override_ = 20;
  fake_pkcs12_reader_.is_certs_nickname_used_ = true;
  // python print(base64.b64encode("testusercert 20".encode('utf-8')))
  auto expected_encoded_label = base::Base64Decode("dGVzdHVzZXJjZXJ0IDIw");

  bool import_result = RunImportPkcs12Certificate();
  ObjectAttributes cert_data = passed_data_.pkcs12_cert_attributes[0];

  EXPECT_EQ(cert_data.GetCkByte(CKA_LABEL), expected_encoded_label);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 21);
  EXPECT_TRUE(import_result);
  EXPECT_TRUE(KeyImportDone());
}

// GetScopedCert is failed in CanFindInstalledKey/GetScopedCert. Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, GetScopedCertFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.get_der_encoded_cert_status_ =
      Pkcs12ReaderStatusCode::kPkcs12CertDerMissed;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.get_der_encode_cert_called_, 1);
  EXPECT_EQ(fake_pkcs12_reader_.is_certs_with_nickname_in_slots_called_, 1);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// FindPrivateKeyFromCert is failed in CanFindInstalledKey/DoesKeyForCertExist.
// Import failed.
TEST_F(ChapsUtilPKCS12ImportTest,
       FindPrivateKeyFromCertFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.find_key_by_cert_status_ =
      Pkcs12ReaderStatusCode::kMissedSlotInfo;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.find_key_by_cert_called_, 1);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// Private key found by cert in CanFindInstalledKey. Key import is never
// happened, but cert is imported.
TEST_F(ChapsUtilPKCS12ImportTest, FindPrivateKeyFromCertSuccPKCS12ImportSucc) {
  fake_pkcs12_reader_.find_key_by_cert_status_ =
      Pkcs12ReaderStatusCode::kSuccess;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.find_key_by_cert_called_, 1);
  EXPECT_TRUE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
  EXPECT_TRUE(CertImportDone());
}

// FindKeyByDERCert is failed in CanFindInstalledKey/DoesKeyForCertExist for
// DER type of cert. Import failed.
TEST_F(ChapsUtilPKCS12ImportTest, FindKeyByDERCertFailedPKCS12ImportFailed) {
  fake_pkcs12_reader_.find_key_by_der_cert_status_ =
      Pkcs12ReaderStatusCode::kMissedSlotInfo;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.find_key_by_cert_called_, 2);
  EXPECT_FALSE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
}

// Private key found in slot by DER cert inside CanFindInstalledKey. Key import
// is never happened, but cert is imported.
TEST_F(ChapsUtilPKCS12ImportTest, FindKeyByDERCertSuccPKCS12ImportSucc) {
  fake_pkcs12_reader_.find_key_by_der_cert_status_ =
      Pkcs12ReaderStatusCode::kSuccess;

  bool import_result = RunImportPkcs12Certificate();

  EXPECT_EQ(fake_pkcs12_reader_.find_key_by_cert_called_, 2);
  EXPECT_TRUE(import_result);
  EXPECT_TRUE(KeyImportNeverDone());
  EXPECT_TRUE(CertImportDone());
}

}  // namespace
}  // namespace chromeos
