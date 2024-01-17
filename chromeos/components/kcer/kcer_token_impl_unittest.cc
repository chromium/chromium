// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/kcer_token_impl.h"

#include <string_view>

#include "base/base64.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "chromeos/components/kcer/chaps/mock_high_level_chaps_client.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using AttributeId = kcer::HighLevelChapsClient::AttributeId;

namespace kcer::internal {
namespace {

constexpr int kDefaultAttempts = KcerTokenImpl::kDefaultAttempts;

constexpr char kFakeRsaModulusBase64[] =
    "pKQjSyvO8LMtTx1ZKIhymKPSwn0GXBxjBshy7390MRKDa8CXfKsrkicIdbUQ54RlY2GGuxufuo"
    "kdz7WBugxW5zReJkBcMG8idCaG6moQIr3nIgOpP1ntN0Y7xFrXIshKLifm6m9AaYyXoKMjq1wc"
    "rFb1zDO3iZoZi5a4RvSueuwTPJ6nMo6ABRqe2dcJaTeBgFtt3au49psAe3MYBtym191C3BXlc3"
    "Ei+I25Es0Pf2moxaal8BmJuaZxAIkmOFWDto9ChelM+8KA7F28Js/CHEUlGlV1g9JCOJEpH/"
    "Hh8mF9taYmrzzOsrDOjW4bgWVXTxOVFbkI8Znj/9Yt7VyWdQ==";
constexpr char kFakeRsaExponentBase64[] = "AQAB";
// The correct id for `kFakeRsaModulusBase64`.
constexpr char kFakeRsaPkcs11IdBase64[] = "b4IkC5I3TLzDPDfaMaVES/hL6I4=";
// The correct SPKI for `kFakeRsaModulusBase64` and `kFakeRsaExponentBase64`
// pair.
constexpr char kFakeRsaSpkiBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApKQjSyvO8LMtTx1ZKIhymKPSwn0GXB"
    "xjBshy7390MRKDa8CXfKsrkicIdbUQ54RlY2GGuxufuokdz7WBugxW5zReJkBcMG8idCaG6moQ"
    "Ir3nIgOpP1ntN0Y7xFrXIshKLifm6m9AaYyXoKMjq1wcrFb1zDO3iZoZi5a4RvSueuwTPJ6nMo"
    "6ABRqe2dcJaTeBgFtt3au49psAe3MYBtym191C3BXlc3Ei+"
    "I25Es0Pf2moxaal8BmJuaZxAIkmOFWDto9ChelM+8KA7F28Js/CHEUlGlV1g9JCOJEpH/"
    "Hh8mF9taYmrzzOsrDOjW4bgWVXTxOVFbkI8Znj/9Yt7VyWdQIDAQAB";

constexpr char kFakeEcPublicValueBase64[] =
    "BEEE9zBHRlSWLfKiDRa63Ztqagi6rnkCpQ3L8/voA1/"
    "orozntbgol7gilBcwU3cAqdazmeWz7XRNk3OE++XVFzGgbA==";
// The correct id for `kFakeEcPublicValueBase64`.
constexpr char kFakeEcPkcs11IdBase64[] = "7vBH+E9iez6kgpEWm0+MSjVZxpI=";
// The correct SPKI for `kFakeEcPublicValueBase64` and `kFakeEcPkcs11IdBase64`
// pair.
constexpr char kFakeEcSpkiBase64[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE9zBHRlSWLfKiDRa63Ztqagi6rnkCpQ3L8/"
    "voA1/orozntbgol7gilBcwU3cAqdazmeWz7XRNk3OE++XVFzGgbA==";

template <typename T1, typename T2>
bool SpanEqual(base::span<T1> s1, base::span<T2> s2) {
  return base::ranges::equal(base::as_bytes(s1), base::as_bytes(s2));
}

bool FindAttribute(chaps::AttributeList attrs,
                   uint32_t attribute_type,
                   base::span<const uint8_t> value) {
  for (int i = 0; i < attrs.attributes_size(); ++i) {
    const chaps::Attribute& cur_attr = attrs.attributes(i);
    if (cur_attr.type() != attribute_type) {
      continue;
    }
    // There shouldn't be two attributes with the same type and different
    // values, if this one is not the one, return false;
    if (!cur_attr.has_length() || !cur_attr.has_value()) {
      return false;
    }

    return SpanEqual(base::make_span(cur_attr.value()), value);
  }
  return false;
}

// `T` must be a simple type, i.e. no internal pointers, etc.
// `value` must outlive the returned span.
template <typename T>
base::span<const uint8_t> MakeSpan(T* value) {
  return base::as_bytes(base::span<T>(value, /*count=*/1u));
}

void AddAttribute(chaps::AttributeList& attr_list,
                  chromeos::PKCS11_CK_ATTRIBUTE_TYPE type,
                  base::span<const uint8_t> data) {
  chaps::Attribute* new_attr = attr_list.add_attributes();
  new_attr->set_type(type);
  new_attr->set_value(std::string(data.begin(), data.end()));
  new_attr->set_length(data.size());
}

chaps::AttributeList GetRsaKeyAttrs(base::span<const uint8_t> pkcs11_id,
                                    base::span<const uint8_t> modulus,
                                    base::span<const uint8_t> exponent) {
  chaps::AttributeList rsa_attrs;
  AddAttribute(rsa_attrs, chromeos::PKCS11_CKA_ID, pkcs11_id);
  AddAttribute(rsa_attrs, chromeos::PKCS11_CKA_MODULUS, modulus);
  AddAttribute(rsa_attrs, chromeos::PKCS11_CKA_PUBLIC_EXPONENT, exponent);
  return rsa_attrs;
}

chaps::AttributeList GetEcKeyAttrs(base::span<const uint8_t> pkcs11_id,
                                   base::span<const uint8_t> ec_point) {
  chaps::AttributeList ec_attrs;
  AddAttribute(ec_attrs, chromeos::PKCS11_CKA_ID, pkcs11_id);
  AddAttribute(ec_attrs, chromeos::PKCS11_CKA_EC_POINT, ec_point);
  return ec_attrs;
}

class ScopedNotificationsObserver : public net::CertDatabase::Observer {
 public:
  ScopedNotificationsObserver() {
    net::CertDatabase::GetInstance()->AddObserver(this);
  }
  ~ScopedNotificationsObserver() override {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
  }

  void OnClientCertStoreChanged() override { ++counter_; }

  size_t counter_ = 0;
};

class KcerTokenImplTest : public testing::Test {
 public:
  KcerTokenImplTest()
      : rsa_modulus_(base::Base64Decode(kFakeRsaModulusBase64).value()),
        rsa_pub_exponent_(base::Base64Decode(kFakeRsaExponentBase64).value()),
        rsa_pkcs11_id_(base::Base64Decode(kFakeRsaPkcs11IdBase64).value()),
        rsa_spki_(base::Base64Decode(kFakeRsaSpkiBase64).value()),
        ec_public_value_(base::Base64Decode(kFakeEcPublicValueBase64).value()),
        ec_pkcs11_id_(base::Base64Decode(kFakeEcPkcs11IdBase64).value()),
        ec_spki_(base::Base64Decode(kFakeEcSpkiBase64).value()),
        token_(Token::kUser, &chaps_client_) {}

  void TearDown() override {
    // Check the notifications about cert changes. If a test doesn't configure
    // anything, then by default it is expected to not emit any notifications.
    EXPECT_EQ(notifications_observer_.counter_, expected_notifications_count_);
  }

 protected:
  chaps::AttributeList GetFakeRsaPublicKeyAttrs() {
    std::string modulus_str(rsa_modulus_.begin(), rsa_modulus_.end());
    std::string exponent_str(rsa_pub_exponent_.begin(),
                             rsa_pub_exponent_.end());

    chaps::AttributeList result;
    chaps::Attribute* modulus = result.add_attributes();
    modulus->set_type(chromeos::PKCS11_CKA_MODULUS);
    modulus->set_value(std::move(modulus_str));
    modulus->set_length(modulus->value().size());
    chaps::Attribute* exponent = result.add_attributes();
    exponent->set_type(chromeos::PKCS11_CKA_PUBLIC_EXPONENT);
    exponent->set_value(std::move(exponent_str));
    exponent->set_length(exponent->value().size());
    return result;
  }

  chaps::AttributeList GetFakeEcPublicKeyAttrs() {
    std::string point_str(ec_public_value_.begin(), ec_public_value_.end());

    chaps::AttributeList result;
    chaps::Attribute* point = result.add_attributes();
    point->set_type(chromeos::PKCS11_CKA_EC_POINT);
    point->set_value(std::move(point_str));
    point->set_length(point->value().size());

    return result;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};

  ScopedNotificationsObserver notifications_observer_;
  size_t expected_notifications_count_ = 0;

  std::vector<uint8_t> rsa_modulus_;
  std::vector<uint8_t> rsa_pub_exponent_;
  Pkcs11Id rsa_pkcs11_id_;
  PublicKeySpki rsa_spki_;

  std::vector<uint8_t> ec_public_value_;
  Pkcs11Id ec_pkcs11_id_;
  PublicKeySpki ec_spki_;

  SessionChapsClient::SlotId pkcs11_slot_id_{1};
  MockHighLevelChapsClient chaps_client_;
  KcerTokenImpl token_;
};

// Test that GenerateRsaKey can successfully generate a key pair.
TEST_F(KcerTokenImplTest, GenerateRsaKeySuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  std::vector<uint8_t> mechanism_attrs;
  chaps::AttributeList public_key_attrs;
  EXPECT_CALL(chaps_client_,
              GenerateKeyPair(pkcs11_slot_id_,
                              chromeos::PKCS11_CKM_RSA_PKCS_KEY_PAIR_GEN,
                              mechanism_attrs, _, _, _))
      .WillOnce(DoAll(MoveArg<3>(&public_key_attrs),
                      RunOnceCallback<5>(result_pub_key_handle,
                                         result_priv_key_handle, result_code)));

  EXPECT_CALL(chaps_client_,
              GetAttributeValue(pkcs11_slot_id_, result_pub_key_handle, _, _))
      .WillOnce(RunOnceCallback<3>(GetFakeRsaPublicKeyAttrs(), result_code));

  chaps::AttributeList pkcs11_id_attrs;
  EXPECT_CALL(chaps_client_,
              SetAttributeValue(pkcs11_slot_id_, key_handles, _, _))
      .WillOnce(
          DoAll(MoveArg<2>(&pkcs11_id_attrs), RunOnceCallback<3>(result_code)));

  RsaModulusLength modulus_length_enum = RsaModulusLength::k2048;
  chromeos::PKCS11_CK_ULONG modulus_length_bits =
      static_cast<uint32_t>(modulus_length_enum);

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(modulus_length_enum,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(public_key_attrs, chromeos::PKCS11_CKA_MODULUS_BITS,
                            MakeSpan(&modulus_length_bits)));
  EXPECT_TRUE(FindAttribute(pkcs11_id_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));

  EXPECT_TRUE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get()->GetToken(), Token::kUser);
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetPkcs11Id().value()),
                        base::make_span(rsa_pkcs11_id_.value())));
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetSpki().value()),
                        base::make_span(rsa_spki_.value())));
}

// Test that GenerateRsaKey correctly sets attributes for a software backed key
// pair.
TEST_F(KcerTokenImplTest, GenerateRsaKeySoftwareBacked) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  chaps::AttributeList private_key_attrs;
  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(DoAll(MoveArg<4>(&private_key_attrs),
                      RunOnceCallback<5>(ObjectHandle(), ObjectHandle(),
                                         chromeos::PKCS11_CKR_GENERAL_ERROR)));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/false, waiter.GetCallback());

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  EXPECT_TRUE(FindAttribute(private_key_attrs, chaps::kForceSoftwareAttribute,
                            MakeSpan(&kTrue)));

  // The rest is not important for this test.
  EXPECT_FALSE(waiter.Get().has_value());
}

// Test that GenerateRsaKey correctly fails when the generation of a key pair
// fails.
TEST_F(KcerTokenImplTest, GenerateRsaKeyFailToGenerate) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(ObjectHandle(), ObjectHandle(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToGenerateKey);
}

// Test that GenerateRsaKey retries several times when generation of a key pair
// fails with a session error.
TEST_F(KcerTokenImplTest, GenerateRsaKeyRetryGenerateOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          ObjectHandle(), ObjectHandle(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that GenerateRsaKey correctly fails when the reading of public key
// attributes fails.
TEST_F(KcerTokenImplTest, GenerateRsaKeyFailToReadPublicKey) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(result_pub_key_handle,
                                   result_priv_key_handle,
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .WillOnce(RunOnceCallback<3>(chaps::AttributeList(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, key_handles, _))
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToExportPublicKey);
}

// Test that GenerateRsaKey retries several times when reading of public key
// attributes fails with a session error. GenerateRsaKey has to retry all the
// previous methods.
TEST_F(KcerTokenImplTest, GenerateRsaKeyRetryReadAttrsOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(result_pub_key_handle,
                                                   result_priv_key_handle,
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          chaps::AttributeList(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that GenerateRsaKey correctly fails when the writing of the id on the
// public and private keys fails.
TEST_F(KcerTokenImplTest, GenerateRsaKeyFailToSetId) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(result_pub_key_handle,
                                   result_priv_key_handle,
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .WillOnce(RunOnceCallback<3>(GetFakeRsaPublicKeyAttrs(),
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handles, _, _))
      .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, key_handles, _))
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToWriteAttribute);
}

// Test that GenerateRsaKey retries several times when the writing of the id on
// the public and private keys fails with a session error. GenerateRsaKey has to
// retry all the previous methods.
TEST_F(KcerTokenImplTest, GenerateRsaKeyRetrySetIdOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(result_pub_key_handle,
                                                   result_priv_key_handle,
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(GetFakeRsaPublicKeyAttrs(),
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handles, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<3>(chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048,
                        /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that GenerateEcKey can successfully generate a key pair.
TEST_F(KcerTokenImplTest, GenerateEcKeySuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  std::vector<uint8_t> mechanism_attrs;
  chaps::AttributeList public_key_attrs;
  EXPECT_CALL(
      chaps_client_,
      GenerateKeyPair(pkcs11_slot_id_, chromeos::PKCS11_CKM_EC_KEY_PAIR_GEN,
                      mechanism_attrs, _, _, _))
      .WillOnce(DoAll(MoveArg<3>(&public_key_attrs),
                      RunOnceCallback<5>(result_pub_key_handle,
                                         result_priv_key_handle, result_code)));

  EXPECT_CALL(chaps_client_,
              GetAttributeValue(pkcs11_slot_id_, result_pub_key_handle, _, _))
      .WillOnce(RunOnceCallback<3>(GetFakeEcPublicKeyAttrs(), result_code));

  chaps::AttributeList pkcs11_id_attrs;
  EXPECT_CALL(chaps_client_,
              SetAttributeValue(pkcs11_slot_id_, key_handles, _, _))
      .WillOnce(
          DoAll(MoveArg<2>(&pkcs11_id_attrs), RunOnceCallback<3>(result_code)));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(pkcs11_id_attrs, chromeos::PKCS11_CKA_ID,
                            ec_pkcs11_id_.value()));

  EXPECT_TRUE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get()->GetToken(), Token::kUser);
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetPkcs11Id().value()),
                        base::make_span(ec_pkcs11_id_.value())));
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetSpki().value()),
                        base::make_span(ec_spki_.value())));
}

// Test that GenerateEcKey correctly sets attributes for a software backed key
// pair.
TEST_F(KcerTokenImplTest, GenerateEcKeySoftwareBacked) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  chaps::AttributeList private_key_attrs;
  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(DoAll(MoveArg<4>(&private_key_attrs),
                      RunOnceCallback<5>(ObjectHandle(), ObjectHandle(),
                                         chromeos::PKCS11_CKR_GENERAL_ERROR)));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/false, waiter.GetCallback());

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  EXPECT_TRUE(FindAttribute(private_key_attrs, chaps::kForceSoftwareAttribute,
                            MakeSpan(&kTrue)));

  // The rest is not important for this test.
  EXPECT_FALSE(waiter.Get().has_value());
}

// Test that GenerateEcKey correctly fails when the generation of a key pair
// fails.
TEST_F(KcerTokenImplTest, GenerateEcKeyFailToGenerate) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(ObjectHandle(), ObjectHandle(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToGenerateKey);
}

// Test that GenerateEcKey retries several times when generation of a key pair
// fails with a session error.
TEST_F(KcerTokenImplTest, GenerateEcKeyRetryGenerateOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          ObjectHandle(), ObjectHandle(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that GenerateEcKey correctly fails when the reading of public key
// attributes fails.
TEST_F(KcerTokenImplTest, GenerateEcKeyFailToReadPublicKey) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(result_pub_key_handle,
                                   result_priv_key_handle,
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .WillOnce(RunOnceCallback<3>(chaps::AttributeList(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, key_handles, _))
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToExportPublicKey);
}

// Test that GenerateEcKey retries several times when reading of public key
// attributes fails with a session error. GenerateEcKey has to retry all the
// previous methods.
TEST_F(KcerTokenImplTest, GenerateEcKeyRetryReadAttrsOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(result_pub_key_handle,
                                                   result_priv_key_handle,
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          chaps::AttributeList(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that GenerateEcKey correctly fails when the writing of the id on the
// public and private keys fails.
TEST_F(KcerTokenImplTest, GenerateEcKeyFailToSetId) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillOnce(RunOnceCallback<5>(result_pub_key_handle,
                                   result_priv_key_handle,
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .WillOnce(RunOnceCallback<3>(GetFakeEcPublicKeyAttrs(),
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handles, _, _))
      .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, key_handles, _))
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToWriteAttribute);
}

// Test that GenerateEcKey retries several times when the writing of the id on
// the public and private keys fails with a session error. GenerateEcKey has to
// retry all the previous methods.
TEST_F(KcerTokenImplTest, GenerateEcKeyRetrySetIdOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle result_pub_key_handle{10};
  ObjectHandle result_priv_key_handle{20};
  std::vector<ObjectHandle> key_handles(
      {result_pub_key_handle, result_priv_key_handle});

  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(result_pub_key_handle,
                                                   result_priv_key_handle,
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(GetFakeEcPublicKeyAttrs(),
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handles, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<3>(chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<PublicKey, Error>> waiter;
  token_.GenerateEcKey(EllipticCurve::kP256,
                       /*hardware_backed=*/true, waiter.GetCallback());

  EXPECT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that RemoveKeyAndCerts can successfully remove a key pair and certs by
// PKCS#11 id.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsByIdSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  // These ids represent all the objects that are related to `public_key` and
  // should be deleted.
  std::vector<ObjectHandle> result_object_list{
      ObjectHandle(10), ObjectHandle(20), ObjectHandle(30)};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
  EXPECT_TRUE(waiter.Get().has_value());
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts can successfully remove a key pair and certs by
// SPKI for RSA keys.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsBySpkiRsaSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  // These ids represent all the objects that should be deleted.
  std::vector<ObjectHandle> result_object_list{
      ObjectHandle(10), ObjectHandle(20), ObjectHandle(30)};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(rsa_spki_), waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
  EXPECT_TRUE(waiter.Get().has_value());
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts can successfully remove a key pair and certs by
// SPKI for EC keys.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsBySpkiEcSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  // These ids represent all the objects that should be deleted.
  std::vector<ObjectHandle> result_object_list{
      ObjectHandle(10), ObjectHandle(20), ObjectHandle(30)};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(ec_spki_), waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            ec_pkcs11_id_.value()));
  EXPECT_TRUE(waiter.Get().has_value());
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts correctly fails when it cannot recover PKCS#11
// from the provided SPKI.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsBySpkiFail) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  PublicKeySpki bad_spki({1, 2, 3, 4, 5});  // Not a valid SPKI.

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(bad_spki), waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToGetPkcs11Id);
}

// Test that RemoveKeyAndCerts correctly fails when the search for objects
// fails.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(RunOnceCallback<2>(result_object_list, result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that RemoveKeyAndCerts retries several times when the search for objects
// fails with a session error.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsRetrySearchOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_SESSION_CLOSED;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<2>(result_object_list, result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that RemoveKeyAndCerts correctly fails when the removal of objects
// fails.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsFailToDestroy) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(
          RunOnceCallback<2>(result_object_list, chromeos::PKCS11_CKR_OK));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToRemoveObjects);
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts retries several times when the removal of objects
// fails with a session error. RemoveKeyAndCerts has to retry all the previous
// methods.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsRetryDestroyOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_SESSION_CLOSED;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(result_object_list,
                                                   chromeos::PKCS11_CKR_OK));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that ListKeys can successfully list keys when there are no keys.
TEST_F(KcerTokenImplTest, ListKeysSuccessWithNoKeys) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<ObjectHandle> result_object_list{};
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(result_object_list,
                                                   chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_TRUE(waiter.Get().has_value());
  EXPECT_TRUE(waiter.Get().value().empty());
}

// Test that ListKeys can successfully list keys when there is one RSA key.
TEST_F(KcerTokenImplTest, ListKeysSuccessWithOneRsaKey) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle rsa_handle{1};
  std::vector<ObjectHandle> rsa_handles{rsa_handle};
  std::vector<ObjectHandle> ec_handles{};
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(2)
      .WillOnce(RunOnceCallback<2>(rsa_handles, chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ec_handles, chromeos::PKCS11_CKR_OK));

  chaps::AttributeList rsa_attrs =
      GetRsaKeyAttrs(rsa_pkcs11_id_.value(), rsa_modulus_, rsa_pub_exponent_);
  EXPECT_CALL(
      chaps_client_,
      GetAttributeValue(pkcs11_slot_id_, rsa_handle,
                        std::vector<AttributeId>{AttributeId::kPkcs11Id,
                                                 AttributeId::kModulus,
                                                 AttributeId::kPublicExponent},
                        _))
      .WillOnce(RunOnceCallback<3>(rsa_attrs, chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_TRUE(waiter.Get().has_value());
  ASSERT_EQ(waiter.Get().value().size(), 1u);
  PublicKey pub_key = waiter.Get().value().front();
  EXPECT_EQ(pub_key.GetPkcs11Id(), rsa_pkcs11_id_);
  EXPECT_EQ(pub_key.GetSpki(), rsa_spki_);
  EXPECT_EQ(pub_key.GetToken(), Token::kUser);
}

// Test that ListKeys can successfully list keys when there is one EC key.
TEST_F(KcerTokenImplTest, ListKeysSuccessWithOneEcKey) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle ec_handle{1};
  std::vector<ObjectHandle> rsa_handles{};
  std::vector<ObjectHandle> ec_handles{ec_handle};
  std::vector<ObjectHandle> priv_key_handles{ObjectHandle(2)};
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(3)
      .WillOnce(RunOnceCallback<2>(rsa_handles, chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ec_handles, chromeos::PKCS11_CKR_OK))
      // For each EC handle `token_` will check that the private key exists.
      .WillOnce(RunOnceCallback<2>(priv_key_handles, chromeos::PKCS11_CKR_OK));

  chaps::AttributeList ec_attrs =
      GetEcKeyAttrs(ec_pkcs11_id_.value(), ec_public_value_);
  EXPECT_CALL(chaps_client_,
              GetAttributeValue(pkcs11_slot_id_, ec_handle,
                                std::vector<AttributeId>{AttributeId::kPkcs11Id,
                                                         AttributeId::kEcPoint},
                                _))
      .WillOnce(RunOnceCallback<3>(ec_attrs, chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_TRUE(waiter.Get().has_value());
  ASSERT_EQ(waiter.Get().value().size(), 1u);
  PublicKey pub_key = waiter.Get().value().front();
  EXPECT_EQ(pub_key.GetPkcs11Id(), ec_pkcs11_id_);
  EXPECT_EQ(pub_key.GetSpki(), ec_spki_);
  EXPECT_EQ(pub_key.GetToken(), Token::kUser);
}

// Test that ListKeys can successfully list keys when there is one RSA and one
// EC key.
TEST_F(KcerTokenImplTest, ListKeysSuccessWithTwoKeys) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  ObjectHandle rsa_handle{1};
  ObjectHandle ec_handle{2};
  std::vector<ObjectHandle> rsa_handles{rsa_handle};
  std::vector<ObjectHandle> ec_handles{ec_handle};
  std::vector<ObjectHandle> priv_key_handles{ObjectHandle(3)};
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(3)
      .WillOnce(RunOnceCallback<2>(rsa_handles, chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ec_handles, chromeos::PKCS11_CKR_OK))
      // For each EC handle `token_` will check that the private key exists.
      .WillOnce(RunOnceCallback<2>(priv_key_handles, chromeos::PKCS11_CKR_OK));

  chaps::AttributeList rsa_attrs =
      GetRsaKeyAttrs(rsa_pkcs11_id_.value(), rsa_modulus_, rsa_pub_exponent_);
  EXPECT_CALL(chaps_client_,
              GetAttributeValue(pkcs11_slot_id_, rsa_handle, _, _))
      .WillOnce(RunOnceCallback<3>(rsa_attrs, chromeos::PKCS11_CKR_OK));
  chaps::AttributeList ec_attrs =
      GetEcKeyAttrs(ec_pkcs11_id_.value(), ec_public_value_);
  EXPECT_CALL(chaps_client_,
              GetAttributeValue(pkcs11_slot_id_, ec_handle, _, _))
      .WillOnce(RunOnceCallback<3>(ec_attrs, chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_TRUE(waiter.Get().has_value());
  ASSERT_EQ(waiter.Get().value().size(), 2u);
  // The order is not guaranteed, but in practice should be stable.
  PublicKey rsa_pub_key = waiter.Get().value().front();
  PublicKey ec_pub_key = waiter.Get().value().back();
  EXPECT_EQ(rsa_pub_key.GetPkcs11Id(), rsa_pkcs11_id_);
  EXPECT_EQ(rsa_pub_key.GetSpki(), rsa_spki_);
  EXPECT_EQ(rsa_pub_key.GetToken(), Token::kUser);
  EXPECT_EQ(ec_pub_key.GetPkcs11Id(), ec_pkcs11_id_);
  EXPECT_EQ(ec_pub_key.GetSpki(), ec_spki_);
  EXPECT_EQ(ec_pub_key.GetToken(), Token::kUser);
}

// Test that ListKeys correctly skips invalid keys.
TEST_F(KcerTokenImplTest, ListKeysBadKeysAreSkipped) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  // Same handles will be returned for RSA and EC keys, that's not realistic,
  // but good enough for the test.
  std::vector<ObjectHandle> result_object_list{ObjectHandle{1},
                                               ObjectHandle{2}};
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(result_object_list,
                                                   chromeos::PKCS11_CKR_OK));

  chaps::AttributeList bad_rsa_attrs = GetRsaKeyAttrs(
      rsa_pkcs11_id_.value(),
      std::vector<uint8_t>(rsa_modulus_.begin(), rsa_modulus_.end() - 1),
      rsa_pub_exponent_);
  chaps::AttributeList bad_ec_attrs = GetEcKeyAttrs(
      ec_pkcs11_id_.value(), std::vector<uint8_t>(ec_public_value_.begin(),
                                                  ec_public_value_.end() - 1));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(4)
      .WillOnce(
          RunOnceCallback<3>(chaps::AttributeList(), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<3>(bad_rsa_attrs, chromeos::PKCS11_CKR_OK))
      .WillOnce(
          RunOnceCallback<3>(chaps::AttributeList(), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<3>(bad_ec_attrs, chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_TRUE(waiter.Get().has_value());
  EXPECT_TRUE(waiter.Get().value().empty());
}

// Test that ListKeys correctly fails when Chaps fails to find RSA keys.
TEST_F(KcerTokenImplTest, ListKeysFailedToListRsaKeys) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<ObjectHandle> handles{};
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(
          RunOnceCallback<2>(handles, chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that ListKeys correctly fails when Chaps fails to find EC keys.
TEST_F(KcerTokenImplTest, ListKeysFailedToListEcKeys) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<ObjectHandle> handles{};
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(2)
      .WillOnce(RunOnceCallback<2>(handles, chromeos::PKCS11_CKR_OK))
      .WillOnce(
          RunOnceCallback<2>(handles, chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that ListKeys correctly retries when Chaps fails to find RSA keys with a
// session error.
TEST_F(KcerTokenImplTest, ListKeysRetryFindRsaOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<ObjectHandle> handles{};
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          handles, chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that ListKeys correctly retries when Chaps fails to retrieve attributes
// for RSA keys with a session error.
TEST_F(KcerTokenImplTest, ListKeysRetryGetRsaOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<ObjectHandle> handles{ObjectHandle(1)};
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<2>(handles, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          chaps::AttributeList(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that ListKeys correctly retries when Chaps fails to find EC keys with a
// session error.
TEST_F(KcerTokenImplTest, ListKeysRetryFindEcOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  // Alternates between replying with OK and SESSION_CLOSED to handle
  // alternating calls for RSA and EC keys.
  auto fake_find_objects = [](auto slot_id, auto attrs, auto callback) {
    static bool next_is_rsa = true;
    bool is_rsa = std::exchange(next_is_rsa, /*new_value=*/!next_is_rsa);
    if (is_rsa) {
      return std::move(callback).Run(std::vector<ObjectHandle>(),
                                     chromeos::PKCS11_CKR_OK);
    } else {
      return std::move(callback).Run(std::vector<ObjectHandle>(),
                                     chromeos::PKCS11_CKR_SESSION_CLOSED);
    }
  };

  std::vector<ObjectHandle> handles{ObjectHandle(1)};
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(2 * kDefaultAttempts)
      .WillRepeatedly(Invoke(fake_find_objects));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that ListKeys correctly retries when Chaps fails to retrieve attributes
// for EC keys with a session error.
TEST_F(KcerTokenImplTest, ListKeysRetryGetEcOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  // Alternates between returning no handles and one handle to process
  // alternating calls for RSA and EC keys.
  auto fake_find_objects = [](auto slot_id, auto attrs, auto callback) {
    static bool next_is_rsa = true;
    bool is_rsa = std::exchange(next_is_rsa, /*new_value=*/!next_is_rsa);
    if (is_rsa) {
      return std::move(callback).Run(std::vector<ObjectHandle>(),
                                     chromeos::PKCS11_CKR_OK);
    } else {
      return std::move(callback).Run(std::vector<ObjectHandle>{ObjectHandle(1)},
                                     chromeos::PKCS11_CKR_OK);
    }
  };

  std::vector<ObjectHandle> handles{ObjectHandle(1)};
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(2 * kDefaultAttempts)
      .WillRepeatedly(Invoke(fake_find_objects));

  EXPECT_CALL(chaps_client_, GetAttributeValue)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          chaps::AttributeList(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>> waiter;
  token_.ListKeys(waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that DoesPrivateKeyExist can successfully check whether a private key
// exists when it exists.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistKeyExistsSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{ObjectHandle(10)};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList attrs;
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(DoAll(MoveArg<1>(&attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(
      FindAttribute(attrs, chromeos::PKCS11_CKA_ID, rsa_pkcs11_id_.value()));
  ASSERT_TRUE(waiter.Get().has_value());
  EXPECT_TRUE(waiter.Get().value());
}

// Test that DoesPrivateKeyExist can successfully check whether a private key
// exists when it doesn't exist.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistKeyDoesNotExistsSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(
      FindAttribute(attrs, chromeos::PKCS11_CKA_ID, rsa_pkcs11_id_.value()));
  ASSERT_TRUE(waiter.Get<0>().has_value());
  EXPECT_FALSE(waiter.Get<0>().value());
}

// Test that DoesPrivateKeyExist correctly fails when the search for objects
// fails.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(result_object_list, result_code));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  EXPECT_FALSE(waiter.Get<0>().has_value());
  EXPECT_EQ(waiter.Get<0>().error(), Error::kFailedToSearchForObjects);
}

// Test that DoesPrivateKeyExist retries several times when the search for
// objects fails with a session error.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistRetryOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_SESSION_CLOSED;

  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<2>(result_object_list, result_code));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  EXPECT_FALSE(waiter.Get<0>().has_value());
  EXPECT_EQ(waiter.Get<0>().error(), Error::kPkcs11SessionFailure);
}

// Test that Sign can successfully create a RsaSha1 signature.
TEST_F(KcerTokenImplTest, SignRsaSha1Success) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Digest for the same data and algorithm is always the same and was recorded
  // from a working device.
  std::vector<uint8_t> expected_digest =
      base::Base64Decode("MCEwCQYFKw4DAhoFAAQUxTkeMIryW0LVk01qIBo06JjSVcY=")
          .value();
  ObjectHandle expected_key_handle(10);

  std::vector<ObjectHandle> result_object_list({expected_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::vector<uint8_t> result_signature_bytes({11, 12, 13, 14, 15});
  Signature result_signature(result_signature_bytes);

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));
  EXPECT_CALL(
      chaps_client_,
      Sign(pkcs11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS,
           std::vector<uint8_t>(), expected_key_handle, expected_digest, _))
      .WillOnce(DoAll(RunOnceCallback<5>(result_signature_bytes, result_code)));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_TRUE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), result_signature);
  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
}

// Test that Sign can successfully create a RsaSha256 signature.
TEST_F(KcerTokenImplTest, SignRsaSha256Success) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha256;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Digest for the same data and algorithm is always the same and was recorded
  // from a working device.
  std::vector<uint8_t> expected_digest =
      base::Base64Decode(
          "MDEwDQYJYIZIAWUDBAIBBQAEIMhI4QE/"
          "nwSp1j+kPOf9SvA1FSx8ZppKQEtnEHzuXy5O")
          .value();
  ObjectHandle expected_key_handle(10);

  std::vector<ObjectHandle> result_object_list({expected_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::vector<uint8_t> result_signature_bytes({11, 12, 13, 14, 15});
  Signature result_signature(result_signature_bytes);

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));
  EXPECT_CALL(
      chaps_client_,
      Sign(pkcs11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS,
           std::vector<uint8_t>(), expected_key_handle, expected_digest, _))
      .WillOnce(DoAll(RunOnceCallback<5>(result_signature_bytes, result_code)));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_TRUE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), result_signature);
  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
}

// Test that Sign can successfully create a RsaPssSha256 signature.
TEST_F(KcerTokenImplTest, SignRsaPssSha256Success) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPssRsaeSha256;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Digest for the same data and algorithm is always the same and was recorded
  // from a working device.
  std::vector<uint8_t> expected_digest =
      base::Base64Decode("yEjhAT+fBKnWP6Q85/1K8DUVLHxmmkpAS2cQfO5fLk4=")
          .value();
  // Mechanism parameters are always the same for a given algorithm.
  std::vector<uint8_t> expected_mechanism_param =
      base::Base64Decode("UAIAAAAAAAACAAAAAAAAACAAAAAAAAAA").value();
  ObjectHandle expected_key_handle(10);

  std::vector<ObjectHandle> result_object_list({expected_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::vector<uint8_t> result_signature_bytes({11, 12, 13, 14, 15});
  Signature result_signature(result_signature_bytes);

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));
  EXPECT_CALL(
      chaps_client_,
      Sign(pkcs11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS_PSS,
           expected_mechanism_param, expected_key_handle, expected_digest, _))
      .WillOnce(DoAll(RunOnceCallback<5>(result_signature_bytes, result_code)));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_TRUE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), result_signature);
  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
}

// Test that Sign can successfully create a EcSha256 signature.
TEST_F(KcerTokenImplTest, SignEcSha256) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, ec_pkcs11_id_, ec_spki_);
  SigningScheme signing_scheme = SigningScheme::kEcdsaSecp256r1Sha256;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Digest for the same data and algorithm is always the same and was recorded
  // from a working device.
  std::vector<uint8_t> expected_digest =
      base::Base64Decode("yEjhAT+fBKnWP6Q85/1K8DUVLHxmmkpAS2cQfO5fLk4=")
          .value();
  ObjectHandle expected_key_handle(10);

  std::vector<ObjectHandle> result_object_list({expected_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  // Signature is different for each key, this one was recorded from a working
  // device.
  std::vector<uint8_t> result_chaps_signature =
      base::Base64Decode(
          "aNhCYZ1TL7eSxbrA6t/+XBAllGfi0zom4Ybo++iwW81Yob2LDKX6OOUX2h661/"
          "INbTVYGDO5kNDqLBc1BUxgkA==")
          .value();
  // `result_chaps_signature` needs to be reencoded by Kcer into a different
  // format, this is the expected result.
  Signature result_signature(
      base::Base64Decode("MEQCIGjYQmGdUy+3ksW6wOrf/"
                         "lwQJZRn4tM6JuGG6PvosFvNAiBYob2LDKX6OOUX2h661/"
                         "INbTVYGDO5kNDqLBc1BUxgkA==")
          .value());

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));
  EXPECT_CALL(chaps_client_, Sign(pkcs11_slot_id_, chromeos::PKCS11_CKM_ECDSA,
                                  std::vector<uint8_t>(), expected_key_handle,
                                  expected_digest, _))
      .WillOnce(DoAll(RunOnceCallback<5>(result_chaps_signature, result_code)));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_TRUE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), result_signature);
  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            ec_pkcs11_id_.value()));
}

// Test that Sign correctly fails when it fails to find the key.
TEST_F(KcerTokenImplTest, SignFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that Sign retries several times when the search for the key fails with a
// session error.
TEST_F(KcerTokenImplTest, SignRetrySearchOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that Sign correctly fails when Chaps fails to sign.
TEST_F(KcerTokenImplTest, SignFailToSign) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(
          RunOnceCallback<2>(std::vector<ObjectHandle>({ObjectHandle(10)}),
                             chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, Sign)
      .WillOnce(RunOnceCallback<5>(std::vector<uint8_t>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kFailedToSign);
}

// Test that Sign retries several times when Chaps fails to sign with a session
// error.
TEST_F(KcerTokenImplTest, SignRetrySignOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>({ObjectHandle(10)}),
          chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, Sign)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          std::vector<uint8_t>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that SignRsaPkcs1Raw can successfully create a signature.
TEST_F(KcerTokenImplTest, SignRsaPkcs1RawSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  DigestWithPrefix digest({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  ObjectHandle expected_key_handle(10);

  std::vector<ObjectHandle> result_object_list({expected_key_handle});
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::vector<uint8_t> result_signature_bytes({11, 12, 13, 14, 15});
  Signature result_signature(result_signature_bytes);

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));
  EXPECT_CALL(
      chaps_client_,
      Sign(pkcs11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS,
           std::vector<uint8_t>(), expected_key_handle, digest.value(), _))
      .WillOnce(DoAll(RunOnceCallback<5>(result_signature_bytes, result_code)));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key), digest,
                         sign_waiter.GetCallback());

  ASSERT_TRUE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), result_signature);
  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_CLASS,
                            MakeSpan(&priv_key_class)));
  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
}

// Test that SignRsaPkcs1Raw correctly fails when it fails to find the key.
TEST_F(KcerTokenImplTest, SignRsaPkcs1RawFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  DigestWithPrefix digest({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key), digest,
                         sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that SignRsaPkcs1Raw retries several times when the search for the key
// fails with a session error.
TEST_F(KcerTokenImplTest, SignRsaPkcs1RawRetrySearchOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  DigestWithPrefix digest({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key), digest,
                         sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that SignRsaPkcs1Raw correctly fails when Chaps fails to sign.
TEST_F(KcerTokenImplTest, SignRsaPkcs1RawFailToSign) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  DigestWithPrefix digest({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(
          RunOnceCallback<2>(std::vector<ObjectHandle>({ObjectHandle(10)}),
                             chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, Sign)
      .WillOnce(RunOnceCallback<5>(std::vector<uint8_t>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key), digest,
                         sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kFailedToSign);
}

// Test that SignRsaPkcs1Raw retries several times when Chaps fails to sign with
// a session error.
TEST_F(KcerTokenImplTest, SignRsaPkcs1RawRetrySignOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);
  DigestWithPrefix digest({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>({ObjectHandle(10)}),
          chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, Sign)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          std::vector<uint8_t>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key), digest,
                         sign_waiter.GetCallback());

  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kPkcs11SessionFailure);
}

TEST_F(KcerTokenImplTest, GetTokenInfoForUserToken) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  base::test::TestFuture<base::expected<TokenInfo, Error>> info_waiter;
  token_.GetTokenInfo(info_waiter.GetCallback());

  ASSERT_TRUE(info_waiter.Get().has_value());
  const TokenInfo& info = info_waiter.Get().value();
  EXPECT_EQ(info.pkcs11_id, pkcs11_slot_id_.value());
  EXPECT_EQ(info.token_name, "User Token");
  EXPECT_EQ(info.module_name, "Chaps");
}

TEST_F(KcerTokenImplTest, GetTokenInfoForDeviceToken) {
  KcerTokenImpl device_token(Token::kDevice, &chaps_client_);
  device_token.InitializeWithoutNss(pkcs11_slot_id_);

  base::test::TestFuture<base::expected<TokenInfo, Error>> info_waiter;
  device_token.GetTokenInfo(info_waiter.GetCallback());

  ASSERT_TRUE(info_waiter.Get().has_value());
  const TokenInfo& info = info_waiter.Get().value();
  EXPECT_EQ(info.pkcs11_id, pkcs11_slot_id_.value());
  EXPECT_EQ(info.token_name, "Device Token");
  EXPECT_EQ(info.module_name, "Chaps");
}

// Test that SetKeyNickname can successfully set a nickname.
TEST_F(KcerTokenImplTest, SetKeyNicknameSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  ObjectHandle key_handle{1};
  std::vector<ObjectHandle> key_handles{key_handle};
  chaps::AttributeList find_key_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&find_key_attrs),
                RunOnceCallback<2>(key_handles, chromeos::PKCS11_CKR_OK)));

  chaps::AttributeList nickname_attrs;
  EXPECT_CALL(chaps_client_,
              SetAttributeValue(pkcs11_slot_id_, key_handle, _, _))
      .WillOnce(DoAll(MoveArg<2>(&nickname_attrs),
                      RunOnceCallback<3>(chromeos::PKCS11_CKR_OK)));

  std::string new_nickname = "new_nickname";
  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), new_nickname,
                        waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_key_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
  EXPECT_TRUE(FindAttribute(nickname_attrs, chromeos::PKCS11_CKA_LABEL,
                            base::as_bytes(base::make_span(new_nickname))));
  EXPECT_TRUE(waiter.Get().has_value());
}

// Test that SetKeyNickname can successfully set a nickname when the key is
// specified by SPKI.
TEST_F(KcerTokenImplTest, SetKeyNicknameBySpkiSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  ObjectHandle key_handle{1};
  std::vector<ObjectHandle> key_handles{key_handle};
  chaps::AttributeList find_key_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&find_key_attrs),
                RunOnceCallback<2>(key_handles, chromeos::PKCS11_CKR_OK)));

  chaps::AttributeList nickname_attrs;
  EXPECT_CALL(chaps_client_,
              SetAttributeValue(pkcs11_slot_id_, key_handle, _, _))
      .WillOnce(DoAll(MoveArg<2>(&nickname_attrs),
                      RunOnceCallback<3>(chromeos::PKCS11_CKR_OK)));

  std::string new_nickname = "new_nickname";
  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(Token::kUser, rsa_spki_), new_nickname,
                        waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_key_attrs, chromeos::PKCS11_CKA_ID,
                            rsa_pkcs11_id_.value()));
  EXPECT_TRUE(FindAttribute(nickname_attrs, chromeos::PKCS11_CKA_LABEL,
                            base::as_bytes(base::make_span(new_nickname))));
  EXPECT_TRUE(waiter.Get().has_value());
}

// Test that SetKeyNickname correctly fails when the key is specified by an
// invalid SPKI.
TEST_F(KcerTokenImplTest, SetKeyNicknameBySpkiFail) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);

  std::vector<uint8_t> bad_spki = rsa_spki_.value();
  bad_spki.pop_back();
  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(PublicKeySpki(bad_spki)), "",
                        waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToGetPkcs11Id);
}

// Test that SetKeyNickname correctly fails when the key cannot be found.
TEST_F(KcerTokenImplTest, SetKeyNicknameFailToFind) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), "", waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kKeyNotFound);
}

// Test that SetKeyNickname correctly fails when Chaps fails to set the
// attribute.
TEST_F(KcerTokenImplTest, SetKeyNicknameFailToSet) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  ObjectHandle key_handle(1);
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{key_handle},
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handle, _, _))
      .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), "", waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToWriteAttribute);
}

// Test that SetKeyNickname retries several times when Chaps fails to find the
// key with a session error.
TEST_F(KcerTokenImplTest, SetKeyNicknameRetryToFind) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), "", waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that SetKeyNickname retries several times when Chaps fails to set an
// attribute with a session error.
TEST_F(KcerTokenImplTest, SetKeyNicknameRetryToSet) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  ObjectHandle key_handle(1);
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>{key_handle}, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, SetAttributeValue(_, key_handle, _, _))
      .Times(kDefaultAttempts)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<3>(chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), "", waiter.GetCallback());

  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kPkcs11SessionFailure);
}

// Test that all methods are queued until the token is initialized and unblocked
// after that. In this scenario fail all the methods for simplicity.
TEST_F(KcerTokenImplTest, AllMethodsAreBlockedUntilTokenInitialization) {
  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          ObjectHandle(), ObjectHandle(), chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_GENERAL_ERROR));

  PublicKey public_key(Token::kUser, rsa_pkcs11_id_, rsa_spki_);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_rsa_waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048, true,
                        generate_rsa_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> remove_key_waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key),
                           remove_key_waiter.GetCallback());
  base::test::TestFuture<base::expected<bool, Error>> key_exists_waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             key_exists_waiter.GetCallback());

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  token_.Sign(PrivateKeyHandle(public_key), SigningScheme::kRsaPkcs1Sha1,
              DataToSign({1, 2, 3}), sign_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_raw_waiter;
  token_.SignRsaPkcs1Raw(PrivateKeyHandle(public_key),
                         DigestWithPrefix({1, 2, 3}),
                         sign_raw_waiter.GetCallback());
  base::test::TestFuture<base::expected<std::vector<PublicKey>, Error>>
      list_keys_waiter;
  token_.ListKeys(list_keys_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
  token_.SetKeyNickname(PrivateKeyHandle(public_key), "",
                        set_nickname_waiter.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(remove_key_waiter.IsReady());
  EXPECT_FALSE(key_exists_waiter.IsReady());
  EXPECT_FALSE(sign_waiter.IsReady());
  EXPECT_FALSE(sign_raw_waiter.IsReady());
  EXPECT_FALSE(list_keys_waiter.IsReady());
  EXPECT_FALSE(set_nickname_waiter.IsReady());

  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_FALSE(generate_rsa_waiter.Get().has_value());
  EXPECT_FALSE(remove_key_waiter.Get().has_value());
  EXPECT_FALSE(key_exists_waiter.Get().has_value());
  EXPECT_FALSE(sign_waiter.Get().has_value());
  EXPECT_FALSE(sign_raw_waiter.Get().has_value());
  EXPECT_FALSE(list_keys_waiter.Get().has_value());
  EXPECT_FALSE(set_nickname_waiter.Get().has_value());
}

}  // namespace
}  // namespace kcer::internal
