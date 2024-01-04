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
using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;

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
constexpr char kFakePkcs11IdBase64[] = "b4IkC5I3TLzDPDfaMaVES/hL6I4=";
// The correct SPKI for `kFakeRsaModulusBase64` and `kFakeRsaExponentBase64`
// pair.
constexpr char kFakeSpkiBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApKQjSyvO8LMtTx1ZKIhymKPSwn0GXB"
    "xjBshy7390MRKDa8CXfKsrkicIdbUQ54RlY2GGuxufuokdz7WBugxW5zReJkBcMG8idCaG6moQ"
    "Ir3nIgOpP1ntN0Y7xFrXIshKLifm6m9AaYyXoKMjq1wcrFb1zDO3iZoZi5a4RvSueuwTPJ6nMo"
    "6ABRqe2dcJaTeBgFtt3au49psAe3MYBtym191C3BXlc3Ei+"
    "I25Es0Pf2moxaal8BmJuaZxAIkmOFWDto9ChelM+8KA7F28Js/CHEUlGlV1g9JCOJEpH/"
    "Hh8mF9taYmrzzOsrDOjW4bgWVXTxOVFbkI8Znj/9Yt7VyWdQIDAQAB";

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
        pkcs11_id_(base::Base64Decode(kFakePkcs11IdBase64).value()),
        spki_(base::Base64Decode(kFakeSpkiBase64).value()),
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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};

  ScopedNotificationsObserver notifications_observer_;
  size_t expected_notifications_count_ = 0;

  std::vector<uint8_t> rsa_modulus_;
  std::vector<uint8_t> rsa_pub_exponent_;
  Pkcs11Id pkcs11_id_;
  PublicKeySpki spki_;
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
                            pkcs11_id_.value()));

  EXPECT_TRUE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get()->GetToken(), Token::kUser);
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetPkcs11Id().value()),
                        base::make_span(pkcs11_id_.value())));
  EXPECT_TRUE(SpanEqual(base::make_span(waiter.Get()->GetSpki().value()),
                        base::make_span(spki_.value())));
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

// Test that RemoveKeyAndCerts can successfully remove a key pair and certs.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

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
                            pkcs11_id_.value()));
  EXPECT_TRUE(waiter.Get().has_value());
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts correctly fails when the search for objects
// fails.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_objects_attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            pkcs11_id_.value()));
  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToSearchForObjects);
}

// Test that RemoveKeyAndCerts retries several times when the search for objects
// fails with a session error.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsRetrySearchOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

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

// Test that GenerateRsaKey correctly fails when the removal of objects fails.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsFailToDestroy) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;

  chaps::AttributeList find_objects_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(
          MoveArg<1>(&find_objects_attrs),
          RunOnceCallback<2>(result_object_list, chromeos::PKCS11_CKR_OK)));

  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(pkcs11_slot_id_, result_object_list, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<base::expected<void, Error>> waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key), waiter.GetCallback());

  EXPECT_TRUE(FindAttribute(find_objects_attrs, chromeos::PKCS11_CKA_ID,
                            pkcs11_id_.value()));
  ASSERT_FALSE(waiter.Get().has_value());
  EXPECT_EQ(waiter.Get().error(), Error::kFailedToRemoveObjects);
  expected_notifications_count_ = 1;
}

// Test that RemoveKeyAndCerts retries several times when the removal of objects
// fails with a session error. RemoveKeyAndCerts has to retry all the previous
// methods.
TEST_F(KcerTokenImplTest, RemoveKeyAndCertsRetryDestroyOnSessionError) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

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

// Test that DoesPrivateKeyExist can successfully check whether a private key
// exists when it exists.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistKeyExistsSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

  std::vector<ObjectHandle> result_object_list{ObjectHandle(10)};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  EXPECT_TRUE(
      FindAttribute(attrs, chromeos::PKCS11_CKA_ID, pkcs11_id_.value()));
  ASSERT_TRUE(waiter.Get().has_value());
  EXPECT_TRUE(waiter.Get().value());
}

// Test that DoesPrivateKeyExist can successfully check whether a private key
// exists when it doesn't exist.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistKeyDoesNotExistsSuccess) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

  std::vector<ObjectHandle> result_object_list{};
  uint32_t result_code = chromeos::PKCS11_CKR_OK;

  chaps::AttributeList attrs;
  EXPECT_CALL(chaps_client_, FindObjects(pkcs11_slot_id_, _, _))
      .WillOnce(DoAll(MoveArg<1>(&attrs),
                      RunOnceCallback<2>(result_object_list, result_code)));

  base::test::TestFuture<base::expected<bool, Error>> waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             waiter.GetCallback());

  EXPECT_TRUE(
      FindAttribute(attrs, chromeos::PKCS11_CKA_ID, pkcs11_id_.value()));
  ASSERT_TRUE(waiter.Get<0>().has_value());
  EXPECT_FALSE(waiter.Get<0>().value());
}

// Test that DoesPrivateKeyExist correctly fails when the search for objects
// fails.
TEST_F(KcerTokenImplTest, DoesPrivateKeyExistFailToSearch) {
  token_.InitializeWithoutNss(pkcs11_slot_id_);
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

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
  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

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

// Test that all methods are queued until the token is initialized and unblocked
// after that. In this scenario fail all the methods for simplicity.
TEST_F(KcerTokenImplTest, AllMethodsAreBlockedUntilTokenInitialization) {
  EXPECT_CALL(chaps_client_, GenerateKeyPair)
      .WillRepeatedly(RunOnceCallbackRepeatedly<5>(
          ObjectHandle(), ObjectHandle(), chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_GENERAL_ERROR));

  PublicKey public_key(Token::kUser, pkcs11_id_, spki_);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_rsa_waiter;
  token_.GenerateRsaKey(RsaModulusLength::k2048, true,
                        generate_rsa_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> remove_key_waiter;
  token_.RemoveKeyAndCerts(PrivateKeyHandle(public_key),
                           remove_key_waiter.GetCallback());
  base::test::TestFuture<base::expected<bool, Error>> key_exists_waiter;
  token_.DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                             key_exists_waiter.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(remove_key_waiter.IsReady());
  EXPECT_FALSE(key_exists_waiter.IsReady());

  token_.InitializeWithoutNss(pkcs11_slot_id_);

  EXPECT_FALSE(generate_rsa_waiter.Get<>().has_value());
  EXPECT_FALSE(remove_key_waiter.Get<>().has_value());
  EXPECT_FALSE(key_exists_waiter.Get<>().has_value());
}

}  // namespace
}  // namespace kcer::internal
