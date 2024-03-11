// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/chaps/session_chaps_client.h"

#include <stdint.h>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using SessionId = kcer::SessionChapsClient::SessionId;
using SlotId = kcer::SessionChapsClient::SlotId;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::Mock;

namespace kcer {
namespace {

class MockChapsService : public crosapi::mojom::ChapsService {
 public:
  MockChapsService() = default;
  MockChapsService(const MockChapsService&) = delete;
  MockChapsService& operator=(const MockChapsService&) = delete;
  ~MockChapsService() override = default;

  // Implements mojom::ChapsService.
  MOCK_METHOD(void,
              GetSlotList,
              (bool token_present, GetSlotListCallback callback),
              (override));
  MOCK_METHOD(void,
              GetMechanismList,
              (uint64_t slot_id, GetMechanismListCallback callback),
              (override));
  MOCK_METHOD(void,
              OpenSession,
              (uint64_t slot_id, uint64_t flags, OpenSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              CloseSession,
              (uint64_t session_id, CloseSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateObject,
              (uint64_t session_id,
               const std::vector<uint8_t>& attributes,
               CreateObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              DestroyObject,
              (uint64_t session_id,
               uint64_t object_handle,
               DestroyObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAttributeValue,
              (uint64_t session_id,
               uint64_t object_handle,
               const std::vector<uint8_t>& attributes,
               GetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAttributeValue,
              (uint64_t session_id,
               uint64_t object_handle,
               const std::vector<uint8_t>& attributes,
               SetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjectsInit,
              (uint64_t session_id,
               const std::vector<uint8_t>& attributes,
               FindObjectsInitCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjects,
              (uint64_t session_id,
               uint64_t max_object_count,
               FindObjectsCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjectsFinal,
              (uint64_t session_id, FindObjectsFinalCallback callback),
              (override));
  MOCK_METHOD(void,
              EncryptInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               EncryptInitCallback callback),
              (override));
  MOCK_METHOD(void,
              Encrypt,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               EncryptCallback callback),
              (override));
  MOCK_METHOD(void,
              DecryptInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               DecryptInitCallback callback),
              (override));
  MOCK_METHOD(void,
              Decrypt,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DecryptCallback callback),
              (override));
  MOCK_METHOD(void,
              SignInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               SignInitCallback callback),
              (override));
  MOCK_METHOD(void,
              Sign,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               SignCallback callback),
              (override));
  MOCK_METHOD(void,
              GenerateKeyPair,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               const std::vector<uint8_t>& public_attributes,
               const std::vector<uint8_t>& private_attributes,
               GenerateKeyPairCallback callback),
              (override));
  MOCK_METHOD(void,
              WrapKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               uint64_t key_handle,
               uint64_t max_out_length,
               WrapKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              UnwrapKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               const std::vector<uint8_t>& wrapped_key,
               const std::vector<uint8_t>& attributes,
               UnwrapKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              DeriveKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t base_key_handle,
               const std::vector<uint8_t>& attributes,
               DeriveKeyCallback callback),
              (override));
};

class SessionChapsClientTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(chaps_mojo_, OpenSession)
        .WillByDefault(RunOnceCallback<2>(kSessionId, chromeos::PKCS11_CKR_OK));
  }

 protected:
  crosapi::mojom::ChapsService* GetChapsService() { return &chaps_mojo_; }

  const uint64_t kSessionId = 11;

  MockChapsService chaps_mojo_;
  SessionChapsClientImpl client_{
      base::BindRepeating(&SessionChapsClientTest::GetChapsService,
                          base::Unretained(this))};
};

// Test that DestroyObject correctly forwards the arguments to the mojo layer
// and the result back from it.
TEST_F(SessionChapsClientTest, DestroyObjectSuccess) {
  constexpr ObjectHandle kExpectedHandle(22);
  constexpr uint32_t kExpectedResultCode = 33;

  EXPECT_CALL(chaps_mojo_,
              DestroyObject(kSessionId, kExpectedHandle.value(), _))
      .WillOnce(RunOnceCallback<2>(kExpectedResultCode));

  base::test::TestFuture<uint32_t> waiter;
  client_.DestroyObject(SlotId(1), kExpectedHandle,
                        /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
}

// Test that GetAttributeValue correctly forwards the arguments to the mojo
// layer and the result back from it.
TEST_F(SessionChapsClientTest, GetAttributeValueSuccess) {
  constexpr ObjectHandle kExpectedHandle(22);
  const std::vector<uint8_t> kExpectedQuery{3, 3, 3};
  const std::vector<uint8_t> kExpectedResult{4, 4, 4};
  constexpr uint32_t kExpectedResultCode = 55;

  EXPECT_CALL(
      chaps_mojo_,
      GetAttributeValue(kSessionId, kExpectedHandle.value(), kExpectedQuery, _))
      .WillOnce(RunOnceCallback<3>(kExpectedResult, kExpectedResultCode));

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.GetAttributeValue(SlotId(1), kExpectedHandle, kExpectedQuery,
                            /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), kExpectedResult);
}

// Test that GetAttributeValue correctly forwards the arguments to the mojo
// layer and the result back from it.
TEST_F(SessionChapsClientTest, SetAttributeValueSuccess) {
  constexpr ObjectHandle kExpectedHandle(22);
  const std::vector<uint8_t> kExpectedAttrs{3, 3, 3};
  constexpr uint32_t kExpectedResultCode = 44;

  EXPECT_CALL(
      chaps_mojo_,
      SetAttributeValue(kSessionId, kExpectedHandle.value(), kExpectedAttrs, _))
      .WillOnce(RunOnceCallback<3>(kExpectedResultCode));

  base::test::TestFuture<uint32_t> waiter;
  client_.SetAttributeValue(SlotId(1), kExpectedHandle, kExpectedAttrs,
                            /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
}

// Test that FindObjects correctly forwards the arguments to the mojo layer
// and the result back from it.
TEST_F(SessionChapsClientTest, FindObjectsSuccess) {
  const std::vector<uint8_t> kExpectedAttrs{1, 1, 1};
  constexpr uint32_t kExpectedResultCode = chromeos::PKCS11_CKR_OK;
  constexpr uint64_t kExpectedMaxObjectCount = 1 << 20;
  const std::vector<uint64_t> kExpectedObjectList{4, 4};
  const std::vector<ObjectHandle> kExpectedTypedList{ObjectHandle(4),
                                                     ObjectHandle(4)};

  EXPECT_CALL(chaps_mojo_, FindObjectsInit(kSessionId, kExpectedAttrs, _))
      .WillOnce(RunOnceCallback<2>(kExpectedResultCode));
  EXPECT_CALL(chaps_mojo_, FindObjects(kSessionId, kExpectedMaxObjectCount, _))
      .WillOnce(RunOnceCallback<2>(kExpectedObjectList, kExpectedResultCode));
  EXPECT_CALL(chaps_mojo_, FindObjectsFinal(kSessionId, _))
      .WillOnce(RunOnceCallback<1>(kExpectedResultCode));

  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
  client_.FindObjects(SlotId(1), kExpectedAttrs,
                      /*attempts_left=*/1, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<std::vector<ObjectHandle>>(), kExpectedTypedList);
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
}

// Test that FindObjects correctly fails when mojom::FindObjectsInit returns an
// error.
TEST_F(SessionChapsClientTest, FindObjectsInitFailed) {
  constexpr uint32_t kExpectedErrorCode = 11;

  EXPECT_CALL(chaps_mojo_, FindObjectsInit)
      .WillOnce(RunOnceCallback<2>(kExpectedErrorCode));
  EXPECT_CALL(chaps_mojo_, FindObjects).Times(0);
  EXPECT_CALL(chaps_mojo_, FindObjectsFinal).Times(0);

  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
  client_.FindObjects(SlotId(1), /*attributes=*/{},
                      /*attempts_left=*/1, waiter.GetCallback());

  EXPECT_TRUE(waiter.Get<std::vector<ObjectHandle>>().empty());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedErrorCode);
}

// Test that FindObjects correctly fails when  mojom::FindObjects returns an
// error.
TEST_F(SessionChapsClientTest, FindObjectsFailed) {
  constexpr uint32_t kExpectedErrorCode = 11;

  EXPECT_CALL(chaps_mojo_, FindObjectsInit)
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_mojo_, FindObjects)
      .WillOnce(
          RunOnceCallback<2>(std::vector<uint64_t>(), kExpectedErrorCode));
  EXPECT_CALL(chaps_mojo_, FindObjectsFinal).Times(0);

  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
  client_.FindObjects(SlotId(1), /*attributes=*/{},
                      /*attempts_left=*/1, waiter.GetCallback());

  EXPECT_TRUE(waiter.Get<std::vector<ObjectHandle>>().empty());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedErrorCode);
}

// Test that Sign correctly forwards the arguments to the mojo layer and the
// result back from it.
TEST_F(SessionChapsClientTest, SignSuccess) {
  constexpr uint64_t kExpectedMechanismType = 11;
  const std::vector<uint8_t> kExpectedMechanismParams = {2, 2, 2};
  constexpr uint64_t kExpectedKeyHandle = 33;
  constexpr ObjectHandle kTypedKeyHandle(kExpectedKeyHandle);
  constexpr uint64_t kExpectedMaxOutLength = 512;
  const std::vector<uint8_t> kExpectedData = {4, 4, 4};
  constexpr uint64_t kExpectedActualOutLength = 55;
  const std::vector<uint8_t> kExpectedSignature = {6, 6};
  constexpr uint32_t kExpectedResultCode = 77;

  EXPECT_CALL(chaps_mojo_,
              SignInit(kSessionId, kExpectedMechanismType,
                       kExpectedMechanismParams, kExpectedKeyHandle, _))
      .WillOnce(RunOnceCallback<4>(chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_mojo_,
              Sign(kSessionId, kExpectedData, kExpectedMaxOutLength, _))
      .WillOnce(RunOnceCallback<3>(kExpectedActualOutLength, kExpectedSignature,
                                   kExpectedResultCode));

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.Sign(SlotId(1), kExpectedMechanismType, kExpectedMechanismParams,
               kTypedKeyHandle, kExpectedData,
               /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), kExpectedSignature);
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
}

// Test that Sign correctly fails when mojom::SignInit returns an error.
TEST_F(SessionChapsClientTest, SignInitFailed) {
  constexpr uint32_t kExpectedErrorCode = 11;

  EXPECT_CALL(chaps_mojo_, SignInit)
      .WillOnce(RunOnceCallback<4>(kExpectedErrorCode));
  EXPECT_CALL(chaps_mojo_, Sign).Times(0);

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.Sign(SlotId(1), /*mechanism_type=*/0, /*mechanism_parameter=*/{},
               /*key_handle=*/ObjectHandle(0), /*data=*/{},
               /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_TRUE(waiter.Get<std::vector<uint8_t>>().empty());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedErrorCode);
}

// Test that Sign correctly fails when mojom::Sign returns an error.
TEST_F(SessionChapsClientTest, SignFailed) {
  constexpr uint32_t kExpectedErrorCode = 11;

  EXPECT_CALL(chaps_mojo_, SignInit)
      .WillOnce(RunOnceCallback<4>(chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_mojo_, Sign)
      .WillOnce(
          RunOnceCallback<3>(0, std::vector<uint8_t>(), kExpectedErrorCode));

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.Sign(SlotId(1), /*mechanism_type=*/0, /*mechanism_parameter=*/{},
               /*key_handle=*/ObjectHandle(0), /*data=*/{},
               /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_TRUE(waiter.Get<std::vector<uint8_t>>().empty());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedErrorCode);
}

// Test that GenerateKeyPair correctly forwards the arguments to the mojo layer
// and the result back from it.
TEST_F(SessionChapsClientTest, GenerateKeyPairSuccess) {
  constexpr uint64_t kExpectedMechanismType = 11;
  const std::vector<uint8_t> kExpectedMechanismParams = {2, 2, 2};
  const std::vector<uint8_t> kExpectedPublicAttrs = {3, 3, 3};
  const std::vector<uint8_t> kExpectedPrivateAttrs = {4, 4, 4};
  constexpr uint64_t kExpectedPublicKey = 55;
  constexpr ObjectHandle kTypedPublicKey(kExpectedPublicKey);
  constexpr uint64_t kExpectedPrivateKey = 66;
  constexpr ObjectHandle kTypedPrivateKey(kExpectedPrivateKey);
  constexpr uint32_t kExpectedResultCode = 77;

  EXPECT_CALL(chaps_mojo_,
              GenerateKeyPair(kSessionId, kExpectedMechanismType,
                              kExpectedMechanismParams, kExpectedPublicAttrs,
                              kExpectedPrivateAttrs, _))
      .WillOnce(RunOnceCallback<5>(kExpectedPublicKey, kExpectedPrivateKey,
                                   kExpectedResultCode));

  base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
  client_.GenerateKeyPair(SlotId(1), kExpectedMechanismType,
                          kExpectedMechanismParams, kExpectedPublicAttrs,
                          kExpectedPrivateAttrs, /*attempts_left=*/1,
                          waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), kExpectedResultCode);
  EXPECT_EQ(waiter.Get<0>(), kTypedPublicKey);
  EXPECT_EQ(waiter.Get<1>(), kTypedPrivateKey);
}

// Test that the first call to each slot opens a session that can be reused by
// all the other methods.
TEST_F(SessionChapsClientTest, AllMethodsDontReopenSession) {
  EXPECT_CALL(chaps_mojo_, OpenSession(0, _, _))
      .WillOnce(RunOnceCallback<2>(kSessionId, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_mojo_, OpenSession(1, _, _))
      .WillOnce(RunOnceCallback<2>(kSessionId, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_mojo_, DestroyObject)
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(chromeos::PKCS11_CKR_OK));

  // Call a method on two different slots that should open new sessions for
  // them.
  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(slot_id), ObjectHandle(0), /*attempts_left=*/1,
                          waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  Mock::VerifyAndClear(&chaps_mojo_);
  EXPECT_CALL(chaps_mojo_, OpenSession).Times(0);
  // Use all methods and check that they are reusing the exiting sessions.

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, DestroyObject)
        .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(slot_id), ObjectHandle(1),
                          /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, GetAttributeValue)
        .WillOnce(RunOnceCallback<3>(std::vector<uint8_t>(),
                                     chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.GetAttributeValue(SlotId(slot_id), ObjectHandle(1),
                              /*attributes_query=*/{}, /*attempts_left=*/1,
                              waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, SetAttributeValue)
        .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<uint32_t> waiter;
    client_.SetAttributeValue(SlotId(slot_id), ObjectHandle(1),
                              /*attributes=*/{},
                              /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, FindObjectsInit)
        .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));
    EXPECT_CALL(chaps_mojo_, FindObjects)
        .WillOnce(RunOnceCallback<2>(std::vector<uint64_t>(),
                                     chromeos::PKCS11_CKR_OK));
    EXPECT_CALL(chaps_mojo_, FindObjectsFinal)
        .WillOnce(RunOnceCallback<1>(chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
    client_.FindObjects(SlotId(slot_id),
                        /*attributes=*/{},
                        /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, SignInit)
        .WillOnce(RunOnceCallback<4>(chromeos::PKCS11_CKR_OK));
    EXPECT_CALL(chaps_mojo_, Sign)
        .WillOnce(RunOnceCallback<3>(0, std::vector<uint8_t>(),
                                     chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.Sign(SlotId(slot_id), /*mechanism_type=*/0,
                 /*mechanism_parameter=*/{}, /*key_handle=*/ObjectHandle(0),
                 /*data=*/{}, /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    EXPECT_CALL(chaps_mojo_, GenerateKeyPair)
        .WillOnce(RunOnceCallback<5>(0, 0, chromeos::PKCS11_CKR_OK));
    base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
    client_.GenerateKeyPair(
        SlotId(slot_id), /*mechanism_type=*/0,
        /*mechanism_parameter=*/{}, /*public_attributes=*/{},
        /*private_attributes=*/{}, /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }
}

// Test that all methods try to open a session multiple times when there's no
// open session at the beginning of each method.
TEST_F(SessionChapsClientTest, AllMethodsTryReopenSession) {
  constexpr int kAttempts = 5;
  constexpr uint64_t kSlotId = 1;

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(kSlotId), ObjectHandle(1), kAttempts,
                          waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.GetAttributeValue(SlotId(kSlotId), ObjectHandle(1),
                              /*attributes_query=*/{}, kAttempts,
                              waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<uint32_t> waiter;
    client_.SetAttributeValue(SlotId(kSlotId), ObjectHandle(1),
                              /*attributes=*/{}, kAttempts,
                              waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
    client_.FindObjects(SlotId(kSlotId),
                        /*attributes=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.Sign(SlotId(kSlotId), /*mechanism_type=*/0,
                 /*mechanism_parameter=*/{}, /*key_handle=*/ObjectHandle(0),
                 /*data=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }

  {
    EXPECT_CALL(chaps_mojo_, OpenSession(kSlotId, _, _))
        .Times(kAttempts)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
            0, chromeos::PKCS11_CKR_GENERAL_ERROR));
    base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
    client_.GenerateKeyPair(
        SlotId(kSlotId), /*mechanism_type=*/0,
        /*mechanism_parameter=*/{}, /*public_attributes=*/{},
        /*private_attributes=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_FAILED_TO_OPEN_SESSION);
    Mock::VerifyAndClear(&chaps_mojo_);
  }
}

}  // namespace
}  // namespace kcer
