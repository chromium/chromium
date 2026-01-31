// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::RunOnceCallback;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class MockUnexportableKeysInternalsPage
    : public unexportable_keys_internals::mojom::Page {
 public:
  MockUnexportableKeysInternalsPage() = default;
  ~MockUnexportableKeysInternalsPage() override = default;

  mojo::PendingRemote<unexportable_keys_internals::mojom::Page>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<unexportable_keys_internals::mojom::Page> receiver_{this};
};

class UnexportableKeysInternalsHandlerTest : public testing::Test {
 public:
  UnexportableKeysInternalsHandlerTest() {
    auto mock_key_service = std::make_unique<
        StrictMock<unexportable_keys::MockUnexportableKeyService>>();
    mock_key_service_ = mock_key_service.get();
    handler_ = std::make_unique<UnexportableKeysInternalsHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(),
        mock_page_.BindAndGetRemote(), std::move(mock_key_service));
  }

 protected:
  UnexportableKeysInternalsHandler& handler() { return CHECK_DEREF(handler_); }

  StrictMock<unexportable_keys::MockUnexportableKeyService>&
  mock_key_service() {
    return CHECK_DEREF(mock_key_service_);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<UnexportableKeysInternalsHandler> handler_;
  raw_ptr<StrictMock<unexportable_keys::MockUnexportableKeyService>>
      mock_key_service_;
  StrictMock<MockUnexportableKeysInternalsPage> mock_page_;
  mojo::Remote<unexportable_keys_internals::mojom::PageHandler> handler_remote_;
};

TEST_F(UnexportableKeysInternalsHandlerTest,
       GetUnexportableKeysInfoFailsToGetKeys) {
  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(base::unexpected(
          unexportable_keys::ServiceError::kOperationNotSupported)));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(get_future.Get(), IsEmpty());
}

TEST_F(UnexportableKeysInternalsHandlerTest,
       GetUnexportableKeysInfoSkipsKeyIfGetWrappedKeyFails) {
  const unexportable_keys::UnexportableKeyId key_id_1;
  const unexportable_keys::UnexportableKeyId key_id_2;

  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector{
          key_id_1,
          key_id_2,
      }));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_1))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 7}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_1))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_1))
      .WillOnce(Return("key_tag_1"));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_1))
      .WillOnce(Return(base::Time::Now()));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_2))
      .WillOnce(Return(base::unexpected(
          unexportable_keys::ServiceError::kOperationNotSupported)));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(
      get_future.Get(),
      UnorderedElementsAre(Pointee(Field(
          &unexportable_keys_internals::mojom::UnexportableKeyInfo::key_id,
          Eq(key_id_1)))));
}

TEST_F(UnexportableKeysInternalsHandlerTest,
       GetUnexportableKeysInfoSkipsKeyIfGetAlgorithmFails) {
  const unexportable_keys::UnexportableKeyId key_id_1;
  const unexportable_keys::UnexportableKeyId key_id_2;

  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector{
          key_id_1,
          key_id_2,
      }));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_1))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 7}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_1))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_1))
      .WillOnce(Return("key_tag_1"));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_1))
      .WillOnce(Return(base::Time::Now()));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_2))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 8}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_2))
      .WillOnce(Return(base::unexpected(
          unexportable_keys::ServiceError::kOperationNotSupported)));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(
      get_future.Get(),
      UnorderedElementsAre(Pointee(Field(
          &unexportable_keys_internals::mojom::UnexportableKeyInfo::key_id,
          Eq(key_id_1)))));
}

TEST_F(UnexportableKeysInternalsHandlerTest,
       GetUnexportableKeysInfoSkipsKeyIfGetKeyTagFails) {
  const unexportable_keys::UnexportableKeyId key_id_1;
  const unexportable_keys::UnexportableKeyId key_id_2;

  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector{
          key_id_1,
          key_id_2,
      }));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_1))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 7}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_1))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_1))
      .WillOnce(Return("key_tag_1"));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_1))
      .WillOnce(Return(base::Time::Now()));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_2))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 8}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_2))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_2))
      .WillOnce(Return(base::unexpected(
          unexportable_keys::ServiceError::kOperationNotSupported)));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(
      get_future.Get(),
      UnorderedElementsAre(Pointee(Field(
          &unexportable_keys_internals::mojom::UnexportableKeyInfo::key_id,
          Eq(key_id_1)))));
}

TEST_F(UnexportableKeysInternalsHandlerTest,
       GetUnexportableKeysInfoSkipsKeyIfGetCreationTimeFails) {
  const unexportable_keys::UnexportableKeyId key_id_1;
  const unexportable_keys::UnexportableKeyId key_id_2;

  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector{
          key_id_1,
          key_id_2,
      }));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_1))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 7}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_1))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_1))
      .WillOnce(Return("key_tag_1"));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_1))
      .WillOnce(Return(base::Time::Now()));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_2))
      .WillOnce(Return(std::vector<uint8_t>{9, 9, 8}));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_2))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_2))
      .WillOnce(Return("key_tag_2"));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_2))
      .WillOnce(Return(base::unexpected(
          unexportable_keys::ServiceError::kOperationNotSupported)));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(
      get_future.Get(),
      UnorderedElementsAre(Pointee(Field(
          &unexportable_keys_internals::mojom::UnexportableKeyInfo::key_id,
          Eq(key_id_1)))));
}

TEST_F(UnexportableKeysInternalsHandlerTest, GetUnexportableKeysInfoSucceeds) {
  const unexportable_keys::UnexportableKeyId key_id_1;
  const std::vector<uint8_t> wrapped_key_1 = {9, 9, 7};
  const crypto::SignatureVerifier::SignatureAlgorithm algorithm_1 =
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  const std::string key_tag_1 = "key_tag_1";
  const base::Time creation_time_1 = base::Time::Now();

  const unexportable_keys::UnexportableKeyId key_id_2;
  const std::vector<uint8_t> wrapped_key_2 = {9, 9, 8};
  const crypto::SignatureVerifier::SignatureAlgorithm algorithm_2 =
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256;
  const std::string key_tag_2 = "key_tag_2";
  const base::Time creation_time_2 = base::Time::Now();

  EXPECT_CALL(mock_key_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector{
          key_id_1,
          key_id_2,
      }));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_1))
      .WillOnce(Return(wrapped_key_1));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_1))
      .WillOnce(Return(algorithm_1));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_1))
      .WillOnce(Return(key_tag_1));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_1))
      .WillOnce(Return(creation_time_1));

  EXPECT_CALL(mock_key_service(), GetWrappedKey(key_id_2))
      .WillOnce(Return(wrapped_key_2));
  EXPECT_CALL(mock_key_service(), GetAlgorithm(key_id_2))
      .WillOnce(Return(algorithm_2));
  EXPECT_CALL(mock_key_service(), GetKeyTag(key_id_2))
      .WillOnce(Return(key_tag_2));
  EXPECT_CALL(mock_key_service(), GetCreationTime(key_id_2))
      .WillOnce(Return(creation_time_2));

  base::test::TestFuture<
      std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>>
      get_future;
  handler().GetUnexportableKeysInfo(get_future.GetCallback());

  EXPECT_THAT(get_future.Get(),
              UnorderedElementsAre(
                  Pointee(AllOf(Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::key_id,
                                      Eq(key_id_1)),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::wrapped_key,
                                      Eq(base::Base64Encode(wrapped_key_1))),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::algorithm,
                                      Eq("ECDSA_SHA256")),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::key_tag,
                                      Eq(key_tag_1)),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::creation_time,
                                      Eq(creation_time_1)))),
                  Pointee(AllOf(Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::key_id,
                                      Eq(key_id_2)),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::wrapped_key,
                                      Eq(base::Base64Encode(wrapped_key_2))),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::algorithm,
                                      Eq("RSA_PSS_SHA256")),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::key_tag,
                                      Eq(key_tag_2)),
                                Field(&unexportable_keys_internals::mojom::
                                          UnexportableKeyInfo::creation_time,
                                      Eq(creation_time_2))))));
}

TEST_F(UnexportableKeysInternalsHandlerTest, DeleteKeyFails) {
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync)
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(unexportable_keys::ServiceError::kKeyNotFound)));

  base::test::TestFuture<bool> delete_future;
  handler().DeleteKey(unexportable_keys::UnexportableKeyId(),
                      delete_future.GetCallback());

  EXPECT_FALSE(delete_future.Get());
}

TEST_F(UnexportableKeysInternalsHandlerTest, DeleteKeySucceeds) {
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync)
      .WillOnce(RunOnceCallback<2>(1));

  base::test::TestFuture<bool> delete_future;
  handler().DeleteKey(unexportable_keys::UnexportableKeyId(),
                      delete_future.GetCallback());

  EXPECT_TRUE(delete_future.Get());
}

}  // namespace
