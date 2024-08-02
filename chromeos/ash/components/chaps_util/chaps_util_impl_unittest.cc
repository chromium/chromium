// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/chaps_util/chaps_util_impl.h"

#include <pkcs11t.h>
#include <secmodt.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/stack.h"
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
const std::optional<std::vector<CK_BYTE>> default_encoded_cert_label =
    base::Base64Decode("dGVzdHVzZXJjZXJ0");
// python print(base64.b64encode("default nickname".encode('utf-8'))).
const std::optional<std::vector<CK_BYTE>> default_encoded_label =
    base::Base64Decode("VW5rbm93biBvcmc=");
const std::optional<std::vector<CK_BYTE>> cka_id_for_ec_key =
    base::Base64Decode("9kVFdOhn8yYso7a/wG2uC0wdHWo=");
const std::optional<std::vector<CK_BYTE>> cka_ex_point_ec_key =
    base::Base64Decode(
        "BP+"
        "IQBEPm3e3ABQMhQaZlE0w8qIjn0tKH6jTEekQvtKoUhFo2nM4Q9VA3MLljVF7vabV8CuH9"
        "/"
        "UkKt2FMg2iHGM=");
const std::optional<std::vector<CK_BYTE>> cka_ec_params_ec_key =
    base::Base64Decode("BggqhkjOPQMBBw==");
const std::optional<std::vector<CK_BYTE>> cka_value_ec_key =
    base::Base64Decode("fvWtrgVAq5JApBuCPK92IUAQQnnEoLUrBgZ/KGFhz7E=");

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
      attr_map[CKA_EC_POINT] = {kCkBytes, "CKA_EC_POINT"};
      attr_map[CKA_DERIVE] = {kCkBool, "CKA_DERIVE"};
      attr_map[CKA_EC_PARAMS] = {kCkBytes, "CKA_EC_PARAMS"};
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

  std::optional<CK_BBOOL> CkBool() { return ck_bool_value_; }

  std::optional<CK_ULONG> CkULong() { return ck_ulong_value_; }

  std::optional<std::vector<CK_BYTE>> CkByte() { return ck_bytes_value_; }

 private:
  std::string name_;
  std::optional<CK_BBOOL> ck_bool_value_;
  std::optional<CK_ULONG> ck_ulong_value_;
  std::optional<std::vector<CK_BYTE>> ck_bytes_value_;

  static std::optional<CK_BBOOL> ParseCkBBool(
      const CK_ATTRIBUTE& attribute,
      const std::string& attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_BBOOL)) {
      ADD_FAILURE() << "Size to small for CK_BBOOL for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return std::nullopt;
    }
    CK_BBOOL value;
    memcpy(&value, attribute.pValue, sizeof(CK_BBOOL));
    return value;
  }

  static std::optional<CK_ULONG> ParseCkULong(
      const CK_ATTRIBUTE& attribute,
      const std::string& attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_ULONG)) {
      ADD_FAILURE() << "Size to small for CK_ULONG for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return std::nullopt;
    }
    CK_ULONG value;
    memcpy(&value, attribute.pValue, sizeof(CK_ULONG));
    return value;
  }

  static std::optional<std::vector<CK_BYTE>> ParseCkBytes(
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

  std::optional<CK_BBOOL> GetCkBool(const CK_ATTRIBUTE_TYPE attribute_type) {
    return parsed_attributes_map[attribute_type].CkBool();
  }

  std::optional<CK_ULONG> GetCkULong(const CK_ATTRIBUTE_TYPE attribute_type) {
    return parsed_attributes_map[attribute_type].CkULong();
  }

  std::optional<std::vector<CK_BYTE>> GetCkByte(
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
  std::optional<CK_SLOT_ID> slot_id;

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
  const raw_ptr<PK11SlotInfo> slot_;
  // Unowned.
  const raw_ptr<PassedData> passed_data_;

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
  const raw_ptr<PK11SlotInfo> slot_;
  // Unowned.
  const raw_ptr<PassedData> passed_data_;
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
    std::optional<std::vector<uint8_t>> file_data = ReadFileToBytes(file_path);
    EXPECT_TRUE(file_data.has_value());
    if (!file_data.has_value()) {
      return {};
    }
    return file_data.value();
  }

  static std::vector<uint8_t>& GetPkcs12Data(std::string file_name) {
    static std::vector<uint8_t> pkcs12_data_;
    pkcs12_data_ = ReadTestFile(file_name);
    return pkcs12_data_;
  }

  static std::vector<uint8_t>& GetPkcs12Data() {
    return GetPkcs12Data("client.p12");
  }

  static std::vector<uint8_t>& GetPkcs12WithEcKeyData() {
    return GetPkcs12Data("client_with_ec_key.p12");
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

}  // namespace
}  // namespace chromeos
