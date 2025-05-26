// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/public_key_credential_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

testing::Matcher<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
EqRelyingPartyAndCredentialId(std::string relying_party_id,
                              std::vector<uint8_t> credential_id) {
  return testing::AllOf(
      testing::Field("relying_party_id",
                     &BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
                         relying_party_id,
                     relying_party_id),
      testing::Field(
          "credential_id",
          &BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::credential_id,
          credential_id));
}

BrowserBoundKeyMetadata MakeBrowserBoundKeyMetadata(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    std::vector<uint8_t> bbk_id) {
  BrowserBoundKeyMetadata meta;
  meta.passkey = BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
      std::move(relying_party_id), std::move(credential_id));
  meta.browser_bound_key_id = std::move(bbk_id);
  return meta;
}

using GetMatchingCredentialIdsCallback = base::RepeatingCallback<void(
    const std::string& relying_party_id,
    const std::vector<std::vector<uint8_t>>& credential_ids,
    bool require_third_party_payment_bit_set,
    base::OnceCallback<void(std::vector<std::vector<uint8_t>>)>)>;

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;

static const int32_t kCoseEs256 = -7;

class PasskeyBrowserBinderTest : public ::testing::Test {
 protected:
  std::unique_ptr<PasskeyBrowserBinder> CreatePasskeyBrowserBinder(
      bool is_new_bbk = true) {
    auto binder = std::make_unique<PasskeyBrowserBinder>(
        fake_browser_bound_key_store_, mock_web_data_service_);
    binder->SetRandomBytesAsVectorCallbackForTesting(
        base::BindLambdaForTesting([this](size_t length) {
          EXPECT_EQ(length, 32u);
          return fake_bbk_id_;
        }));
    fake_browser_bound_key_store_->PutFakeKey(FakeBrowserBoundKey(
        fake_bbk_id_, fake_public_key_,
        /*signature=*/{}, kCoseEs256,
        /*expected_client_data=*/{}, /*is_new=*/is_new_bbk));
    return binder;
  }

  const std::vector<uint8_t> fake_bbk_id_ = {11, 12, 13, 14};
  const std::vector<uint8_t> fake_credential_id_ = {21, 22, 23, 24};
  const std::vector<uint8_t> fake_public_key_ = {31, 32, 33, 34};
  const std::string fake_relying_party_ = "relying.test";

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<FakeBrowserBoundKeyStore> fake_browser_bound_key_store_ =
      base::MakeRefCounted<FakeBrowserBoundKeyStore>();
  scoped_refptr<MockPaymentManifestWebDataService> mock_web_data_service_ =
      base::MakeRefCounted<MockPaymentManifestWebDataService>();
};

TEST_F(PasskeyBrowserBinderTest, CreatesUnboundKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();

  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_algorithms=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});

  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(fake_public_key_, key->Get().GetPublicKeyAsCoseKey());
}

TEST_F(PasskeyBrowserBinderTest, DeletesUnboundKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_algorithms=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});
  ASSERT_TRUE(key.has_value());
  std::vector<uint8_t> bbk_id = key->Get().GetIdentifier();

  // Let the key go out of scope by resetting the std::optional without calling
  // binder->BindKey(std::move(key), ...).
  key.reset();

  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(bbk_id));
}

TEST_F(PasskeyBrowserBinderTest, BindsBrowserBoundKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_algorithms=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});

  WebDataServiceBase::Handle web_data_service_handle = 1234;
  WebDataServiceConsumer* web_data_service_consumer_ = nullptr;
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<3>(&web_data_service_consumer_),
                      Return(web_data_service_handle)));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_);
  ASSERT_TRUE(web_data_service_consumer_);
  web_data_service_consumer_->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, true));

  key.reset();
  EXPECT_TRUE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest, DeletesBrowserBoundKeyIfBindingFails) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_algorithms=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});

  WebDataServiceBase::Handle web_data_service_handle = 1234;
  WebDataServiceConsumer* web_data_service_consumer_ = nullptr;
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<3>(&web_data_service_consumer_),
                      Return(web_data_service_handle)));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_);
  ASSERT_TRUE(web_data_service_consumer_);
  web_data_service_consumer_->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, false));

  key.reset();
  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRetrievesExistingKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder =
      CreatePasskeyBrowserBinder(/*is_new_bbk=*/false);
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));
  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))));

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest, GetBoundKeyForPasskeyRetrievesExistingKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder =
      CreatePasskeyBrowserBinder(/*is_new_bbk=*/false);
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));
  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))));

  binder->GetBoundKeyForPasskey(fake_credential_id_, fake_relying_party_,
                                mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRecreatesWhenEmpty) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));
  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_, fake_bbk_id_,
                         /*consumer=*/NotNull()));
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))));

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, std::vector<uint8_t>()));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRecreatesWhenNullopt) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));
  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_, fake_bbk_id_,
                         /*consumer=*/NotNull()));
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))));

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, std::nullopt));
}

TEST_F(PasskeyBrowserBinderTest, GetBoundKeyForPasskeyReturnsNullWhenNullOpt) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*consumer=*/_))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));
  EXPECT_CALL(mock_callback, Run(IsNull()));

  binder->GetBoundKeyForPasskey(fake_credential_id_, fake_relying_party_,
                                mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, std::nullopt));
}

class PasskeyBrowserBinderDeletionTest : public PasskeyBrowserBinderTest {
 public:
  void RespondToGetAllBrowserBoundKeys(
      std::vector<BrowserBoundKeyMetadata> bbk_metadatas) {
    std::move(captured_get_all_browser_bound_keys_callback_)
        .Run(/*handle=*/1234,
             std::make_unique<WDResult<std::vector<BrowserBoundKeyMetadata>>>(
                 WDResultType::BROWSER_BOUND_KEY_METADATA,
                 std::move(bbk_metadatas)));
  }

  // Implements a fake version of
  // InternalAuthenticator::GetMatchingCredentialIds() which is passed as a
  // callback to PasskeyBrowserBinder::DeleteAllUnknownBrowserBoundKeys.
  void GetMatchingCredentialIds(
      const std::string& relying_party_id,
      const std::vector<std::vector<uint8_t>>& credential_ids,
      bool require_third_party_payment_bit_set,
      base::OnceCallback<void(std::vector<std::vector<uint8_t>>)> callback) {
    if (require_third_party_payment_bit_set) {
      // PasskeyBrowserBinder::DeleteAllUnknownBrowserBoundKeys must call
      // with `require_third_party_payment_bit_set` set to false, since BBKs
      // can be created for passkeys in the first party context through the
      // PaymentRequest API.
      FAIL();
    }
    std::vector<std::vector<uint8_t>> stored_credential_ids =
        fake_matching_credential_ids_[relying_party_id];
    std::erase_if(stored_credential_ids,
                  [&credential_ids](const std::vector<uint8_t>& credential) {
                    return !base::Contains(credential_ids, credential);
                  });
    std::move(callback).Run(std::move(stored_credential_ids));
  }

 protected:
  void SetUp() override {
    PasskeyBrowserBinderTest::SetUp();
    EXPECT_CALL(*mock_web_data_service_, GetAllBrowserBoundKeys)
        .WillOnce(MoveArgAndReturn<0>(
            &captured_get_all_browser_bound_keys_callback_, 1234));
  }

  WebDataServiceRequestCallback captured_get_all_browser_bound_keys_callback_;
  base::flat_map</*relying_party*/ std::string,
                 /*credential_ids*/ std::vector<std::vector<uint8_t>>>
      fake_matching_credential_ids_;
};

TEST_F(PasskeyBrowserBinderDeletionTest,
       DeleteAllUnknownBrowserBoundKeysWhenNoBBKStored) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;

  binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &PasskeyBrowserBinderDeletionTest::GetMatchingCredentialIds,
          base::Unretained(this)),
      mock_callback.Get());
  RespondToGetAllBrowserBoundKeys(/*bbk_metadatas=*/{});
}

TEST_F(PasskeyBrowserBinderDeletionTest,
       DeleteAllUnknownBrowserBoundKeysWithInvalidBbkMetadata) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;
  std::vector<BrowserBoundKeyMetadata> bbk_metadatas;
  bbk_metadatas.push_back(MakeBrowserBoundKeyMetadata(
      fake_credential_id_, fake_relying_party_, fake_bbk_id_));
  // This test does not insert any entries into `fake_matching_credential_ids_`

  testing::Sequence sequence;
  EXPECT_CALL(*mock_web_data_service_,
              DeleteBrowserBoundKeys(
                  testing::UnorderedElementsAre(EqRelyingPartyAndCredentialId(
                      fake_relying_party_, fake_credential_id_)),
                  _))
      .InSequence(sequence)
      .WillOnce(base::test::RunOnceCallbackRepeatedly<1>());
  EXPECT_CALL(mock_callback, Run()).InSequence(sequence);

  binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &PasskeyBrowserBinderDeletionTest::GetMatchingCredentialIds,
          base::Unretained(this)),
      mock_callback.Get());
  RespondToGetAllBrowserBoundKeys(std::move(bbk_metadatas));
}

TEST_F(PasskeyBrowserBinderDeletionTest,
       DeleteAllUnknownBrowserBoundKeysWithValidBbkMetadata) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;
  std::vector<BrowserBoundKeyMetadata> bbk_metadatas;
  bbk_metadatas.push_back(MakeBrowserBoundKeyMetadata(
      fake_credential_id_, fake_relying_party_, fake_bbk_id_));
  fake_matching_credential_ids_[fake_relying_party_].push_back(
      fake_credential_id_);

  EXPECT_CALL(*mock_web_data_service_, DeleteBrowserBoundKeys(_, _)).Times(0);

  binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &PasskeyBrowserBinderDeletionTest::GetMatchingCredentialIds,
          base::Unretained(this)),
      mock_callback.Get());
  RespondToGetAllBrowserBoundKeys(std::move(bbk_metadatas));
}

TEST_F(
    PasskeyBrowserBinderDeletionTest,
    DeleteAllUnknownBrowserBoundKeysWithInvalidBbkMetadataWhenRelyingPartyIsDifferent) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;
  std::vector<BrowserBoundKeyMetadata> bbk_metadatas;
  bbk_metadatas.push_back(MakeBrowserBoundKeyMetadata(
      fake_credential_id_, fake_relying_party_, fake_bbk_id_));
  fake_matching_credential_ids_["another." + fake_relying_party_].push_back(
      fake_credential_id_);

  EXPECT_CALL(*mock_web_data_service_,
              DeleteBrowserBoundKeys(
                  testing::UnorderedElementsAre(EqRelyingPartyAndCredentialId(
                      fake_relying_party_, fake_credential_id_)),
                  _))
      .WillOnce(base::test::RunOnceCallbackRepeatedly<1>());

  binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &PasskeyBrowserBinderDeletionTest::GetMatchingCredentialIds,
          base::Unretained(this)),
      mock_callback.Get());
  RespondToGetAllBrowserBoundKeys(std::move(bbk_metadatas));
}

TEST_F(PasskeyBrowserBinderDeletionTest,
       DeleteAllUnknownBrowserBoundKeysWithMultipleRelyingParties) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;
  std::vector<BrowserBoundKeyMetadata> bbk_metadatas;
  bbk_metadatas.push_back(MakeBrowserBoundKeyMetadata(
      fake_credential_id_, fake_relying_party_, fake_bbk_id_));
  fake_matching_credential_ids_["another." + fake_relying_party_].push_back(
      fake_credential_id_);

  EXPECT_CALL(*mock_web_data_service_,
              DeleteBrowserBoundKeys(
                  testing::UnorderedElementsAre(EqRelyingPartyAndCredentialId(
                      fake_relying_party_, fake_credential_id_)),
                  _))
      .WillOnce(base::test::RunOnceCallbackRepeatedly<1>());

  binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &PasskeyBrowserBinderDeletionTest::GetMatchingCredentialIds,
          base::Unretained(this)),
      mock_callback.Get());
  RespondToGetAllBrowserBoundKeys(std::move(bbk_metadatas));
}

}  // namespace
}  // namespace payments
