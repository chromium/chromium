// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/public_key_credential_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;

static const int32_t kCoseEs256 = -7;

class PasskeyBrowserBinderTest : public ::testing::Test {
 protected:
  std::unique_ptr<PasskeyBrowserBinder> CreatePasskeyBrowserBinder() {
    auto binder = std::make_unique<PasskeyBrowserBinder>(
        CreateFakeBrowserBoundKeyStore(), mock_web_data_service_);
    binder->SetRandomBytesAsVectorCallbackForTesting(
        base::BindLambdaForTesting([this](size_t length) {
          EXPECT_EQ(length, 32u);
          return fake_bbk_id_;
        }));
    fake_browser_bound_key_store_->PutFakeKey(
        fake_bbk_id_,
        FakeBrowserBoundKey(fake_public_key_, /*signature=*/{}, kCoseEs256,
                            /*expected_client_data=*/{}));
    return binder;
  }

  std::unique_ptr<BrowserBoundKeyStore> CreateFakeBrowserBoundKeyStore() {
    auto key_store = std::make_unique<FakeBrowserBoundKeyStore>();
    fake_browser_bound_key_store_ = key_store->GetWeakPtr();
    return key_store;
  }

  const std::vector<uint8_t> fake_bbk_id_ = {11, 12, 13, 14};
  const std::vector<uint8_t> fake_credential_id_ = {21, 22, 23, 24};
  const std::vector<uint8_t> fake_public_key_ = {31, 32, 33, 34};
  const std::string fake_relying_party_ = "relying.test";

  content::BrowserTaskEnvironment task_environment_;
  base::WeakPtr<FakeBrowserBoundKeyStore> fake_browser_bound_key_store_;
  scoped_refptr<MockPaymentManifestWebDataService> mock_web_data_service_ =
      base::MakeRefCounted<MockPaymentManifestWebDataService>();
};

TEST_F(PasskeyBrowserBinderTest, CreatesUnboundKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();

  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_credentials=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});

  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(fake_public_key_, key->Get().GetPublicKeyAsCoseKey());
}

TEST_F(PasskeyBrowserBinderTest, BindsBrowserBoundKey) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key =
      binder->CreateUnboundKey(/*allowed_credentials=*/{
          device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                                kCoseEs256}});

  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, /*consumer=*/NotNull()));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_);
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRetrievesExistingKey) {
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
  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))));

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_credentials=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
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
      fake_credential_id_, fake_relying_party_, /*allowed_credentials=*/
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
      fake_credential_id_, fake_relying_party_, /*allowed_credentials=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      mock_callback.Get());
  ASSERT_TRUE(web_data_service_consumer);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle,
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, std::nullopt));
}

}  // namespace
}  // namespace payments
