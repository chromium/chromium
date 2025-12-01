// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
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

testing::Matcher<BrowserBoundKeyMetadata> EqBrowserBoundKeyMetadata(
    std::string relying_party_id,
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> browser_bound_key_id,
    base::Time last_used) {
  return testing::AllOf(
      testing::Field(
          "passkey", &BrowserBoundKeyMetadata::passkey,
          EqRelyingPartyAndCredentialId(relying_party_id, credential_id)),
      testing::Field("browser_bound_key_id",
                     &BrowserBoundKeyMetadata::browser_bound_key_id,
                     browser_bound_key_id),
      testing::Field("last_used", &BrowserBoundKeyMetadata::last_used,
                     last_used));
}

BrowserBoundKeyMetadata MakeBrowserBoundKeyMetadata(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    std::vector<uint8_t> bbk_id,
    base::Time last_used) {
  BrowserBoundKeyMetadata meta;
  meta.passkey = BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
      std::move(relying_party_id), std::move(credential_id));
  meta.browser_bound_key_id = std::move(bbk_id);
  meta.last_used = std::move(last_used);
  return meta;
}

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::UnorderedElementsAre;

static const int32_t kCoseEs256 = -7;

class PasskeyBrowserBinderTest : public ::testing::Test {
 public:
  PasskeyBrowserBinderTest() {
    EXPECT_TRUE(
        base::Time::FromUTCString("24 Oct 2025 10:30", &fake_last_used_));
  }

 protected:
  std::unique_ptr<PasskeyBrowserBinder> CreatePasskeyBrowserBinder(
      bool is_new_bbk = true) {
    fake_browser_bound_key_store_->SetDeviceSupportsHardwareKeys(true);
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
  base::Time fake_last_used_;
  const int fake_web_data_service_handle_ = 1234;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<FakeBrowserBoundKeyStore> fake_browser_bound_key_store_ =
      base::MakeRefCounted<FakeBrowserBoundKeyStore>();
  scoped_refptr<MockWebPaymentsWebDataService> mock_web_data_service_ =
      base::MakeRefCounted<MockWebPaymentsWebDataService>();
};

TEST_F(PasskeyBrowserBinderTest, CreatesUnboundKey) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();

  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(fake_public_key_, key->Get().GetPublicKeyAsCoseKey());
  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest, CreatesUnboundKeyFailureWithHardwareSupport) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  fake_browser_bound_key_store_->DeleteBrowserBoundKey(fake_bbk_id_);

  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(key.has_value());
  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kFailureWithDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest,
       CreatesUnboundKeyFailureWithNoHardwareSupport) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  fake_browser_bound_key_store_->SetDeviceSupportsHardwareKeys(false);
  fake_browser_bound_key_store_->DeleteBrowserBoundKey(fake_bbk_id_);

  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(key.has_value());
  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kFailureWithoutDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest,
       CreatesUnboundKeyWhenKeyIdReturnedIsDifferentFromPassed) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();

  // Change the random bytes callback to return a different BBK ID.
  const std::vector<uint8_t> requested_fake_bbk_id = {111, 112, 113, 114};
  binder->SetRandomBytesAsVectorCallbackForTesting(
      base::BindLambdaForTesting([&requested_fake_bbk_id](size_t length) {
        return requested_fake_bbk_id;
      }));

  // Returns a BBK with `fake_bbk_id_` when
  // BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId is called
  // with`requested_fake_bbk_id`.
  fake_browser_bound_key_store_->PutFakeKey(
      FakeBrowserBoundKey(fake_bbk_id_, fake_public_key_,
                          /*signature=*/{}, kCoseEs256,
                          /*expected_client_data=*/{}, /*is_new=*/true),
      requested_fake_bbk_id);

  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Expect that the returned UnboundKey contains the BBK with `fake_bbk_id_`.
  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(key->GetBrowserBoundKeyIdForTesting(), fake_bbk_id_);
}

TEST_F(PasskeyBrowserBinderTest, DeletesUnboundKey) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(key.has_value());
  std::vector<uint8_t> bbk_id = key->Get().GetIdentifier();

  // Let the key go out of scope by resetting the std::optional without calling
  // binder->BindKey(std::move(key), ...).
  key.reset();

  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(bbk_id));
}

TEST_F(PasskeyBrowserBinderTest, BindsBrowserBoundKey) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  WebDataServiceRequestCallback web_data_service_callback;
  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_, fake_bbk_id_,
                         Eq(fake_last_used_), /*callback=*/_))
      .WillOnce(MoveArgAndReturn<4>(&web_data_service_callback,
                                    fake_web_data_service_handle_));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_, fake_last_used_);
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, true));

  key.reset();
  EXPECT_TRUE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest, BindsBrowserBoundKeyWithoutLastUsedTimestamp) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  WebDataServiceRequestCallback web_data_service_callback;
  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_, fake_bbk_id_,
                         /*last_used=*/Eq(std::nullopt),
                         /*callback=*/_))
      .WillOnce(MoveArgAndReturn<4>(&web_data_service_callback,
                                    fake_web_data_service_handle_));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_, /*last_used=*/std::nullopt);
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, true));

  key.reset();
  EXPECT_TRUE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest, DeletesBrowserBoundKeyIfBindingFails) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::optional<PasskeyBrowserBinder::UnboundKey> key;
  binder->CreateUnboundKey(
      /*allowed_algorithms=*/{device::PublicKeyCredentialParams::CredentialInfo{
          .algorithm = kCoseEs256}},
      base::BindLambdaForTesting(
          [&](std::optional<PasskeyBrowserBinder::UnboundKey> unbound_key) {
            key = std::move(unbound_key);
            run_loop.Quit();
          }));
  run_loop.Run();

  WebDataServiceRequestCallback web_data_service_callback;
  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_, fake_bbk_id_,
                         Eq(fake_last_used_), /*callback=*/_))
      .WillOnce(MoveArgAndReturn<4>(&web_data_service_callback,
                                    fake_web_data_service_handle_));

  binder->BindKey(std::move(key.value()), fake_credential_id_,
                  fake_relying_party_, fake_last_used_);
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, false));

  key.reset();
  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRetrievesExistingKey) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder =
      CreatePasskeyBrowserBinder(/*is_new_bbk=*/false);
  WebDataServiceRequestCallback web_data_service_callback;
  base::MockCallback<
      base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&web_data_service_callback,
                                    fake_web_data_service_handle_));
  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
  EXPECT_CALL(mock_callback,
              Run(
                  /*is_new=*/false,
                  AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      fake_last_used_, mock_callback.Get());
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, fake_bbk_id_));
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreRetrieve",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest, GetBoundKeyForPasskeyRetrievesExistingKey) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder =
      CreatePasskeyBrowserBinder(/*is_new_bbk=*/false);
  WebDataServiceRequestCallback web_data_service_callback;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&web_data_service_callback,
                                    fake_web_data_service_handle_));
  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
  EXPECT_CALL(mock_callback,
              Run(AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  binder->GetBoundKeyForPasskey(fake_credential_id_, fake_relying_party_,
                                mock_callback.Get());
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, fake_bbk_id_));
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreRetrieve",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRecreatesWhenEmpty) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback get_callback;
  WebDataServiceBase::Handle get_handle = 1234;
  WebDataServiceRequestCallback set_callback;
  WebDataServiceBase::Handle set_handle = 5678;
  base::MockCallback<
      base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&get_callback, get_handle));
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, Eq(fake_last_used_),
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<4>(&set_callback, set_handle));
  EXPECT_CALL(mock_callback,
              Run(
                  /*is_new=*/true,
                  AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      fake_last_used_, mock_callback.Get());
  ASSERT_FALSE(get_callback.is_null());
  std::move(get_callback)
      .Run(get_handle,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, std::vector<uint8_t>()));
  run_loop.Run();

  ASSERT_FALSE(set_callback.is_null());
  std::move(set_callback)
      .Run(set_handle,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, true));
  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
  EXPECT_TRUE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyDeletestBrowserBoundKeyWhenBindingFails) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback get_callback;
  WebDataServiceBase::Handle get_handle = 1234;
  WebDataServiceRequestCallback set_callback;
  WebDataServiceBase::Handle set_handle = 5678;
  base::MockCallback<
      base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&get_callback, get_handle));
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, Eq(fake_last_used_),
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<4>(&set_callback, set_handle));
  EXPECT_CALL(mock_callback,
              Run(
                  /*is_new=*/true,
                  AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      fake_last_used_, mock_callback.Get());
  ASSERT_FALSE(get_callback.is_null());
  std::move(get_callback)
      .Run(get_handle,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, std::vector<uint8_t>()));
  run_loop.Run();

  ASSERT_FALSE(set_callback.is_null());
  std::move(set_callback)
      .Run(set_handle,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, false));
  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyRecreatesWhenNullopt) {
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback get_callback;
  WebDataServiceRequestCallback set_callback;
  base::MockCallback<
      base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(
          MoveArgAndReturn<2>(&get_callback, fake_web_data_service_handle_));
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 fake_bbk_id_, Eq(fake_last_used_),
                                 /*callback=*/_));
  EXPECT_CALL(mock_callback,
              Run(
                  /*is_new=*/true,
                  AllOf(NotNull(), Pointee(Property(
                                       &BrowserBoundKey::GetPublicKeyAsCoseKey,
                                       fake_public_key_)))))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      fake_last_used_, mock_callback.Get());
  ASSERT_FALSE(get_callback.is_null());
  std::move(get_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, std::nullopt));
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
          kSuccessWithDeviceHardware,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyBrowserBinderTest,
       GetOrCreateBoundKeyForPasskeyWhenKeyIdReturnedIsDifferentFromPassed) {
  base::RunLoop run_loop;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback get_callback;
  WebDataServiceBase::Handle get_handle = 1234;
  base::MockCallback<
      base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  // Change the random bytes callback to return a different BBK ID.
  const std::vector<uint8_t> requested_fake_bbk_id = {111, 112, 113, 114};
  binder->SetRandomBytesAsVectorCallbackForTesting(
      base::BindLambdaForTesting([&requested_fake_bbk_id](size_t length) {
        return requested_fake_bbk_id;
      }));

  // Returns a BBK with `fake_bbk_id_` when
  // BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId is called
  // with`requested_fake_bbk_id`.
  fake_browser_bound_key_store_->PutFakeKey(
      FakeBrowserBoundKey(fake_bbk_id_, fake_public_key_,
                          /*signature=*/{}, kCoseEs256,
                          /*expected_client_data=*/{}, /*is_new=*/true),
      requested_fake_bbk_id);

  EXPECT_CALL(*mock_web_data_service_, GetBrowserBoundKey)
      .WillOnce(MoveArgAndReturn<2>(&get_callback, get_handle));

  binder->GetOrCreateBoundKeyForPasskey(
      fake_credential_id_, fake_relying_party_, /*allowed_algorithms=*/
      {device::PublicKeyCredentialParams::CredentialInfo{.algorithm =
                                                             kCoseEs256}},
      fake_last_used_, mock_callback.Get());

  // Expect that the BBK ID passed to SetBrowserBoundKey is what was returned
  // from BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId
  // (`fake_bbk_id_`) rather than the one generated (`requested_fake_bbk_id`).
  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(_, _, fake_bbk_id_, _, _))
      .WillOnce(testing::DoAll([&run_loop] { run_loop.Quit(); }, Return(5678)));

  std::move(get_callback)
      .Run(get_handle,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, std::vector<uint8_t>()));
  run_loop.Run();
}

TEST_F(PasskeyBrowserBinderTest, GetBoundKeyForPasskeyReturnsNullWhenNullOpt) {
  base::HistogramTester histograms;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback web_data_service_callback;
  base::MockCallback<base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)>>
      mock_callback;

  EXPECT_CALL(*mock_web_data_service_,
              GetBrowserBoundKey(fake_credential_id_, fake_relying_party_,
                                 /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&web_data_service_callback,
                                    fake_web_data_service_handle_));
  EXPECT_CALL(mock_callback, Run(IsNull()));

  binder->GetBoundKeyForPasskey(fake_credential_id_, fake_relying_party_,
                                mock_callback.Get());
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
               WDResultType::BROWSER_BOUND_KEY, std::nullopt));
  histograms.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreRetrieve",
      /*expected_count=*/0);
  histograms.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      /*expected_count=*/0);
}

TEST_F(PasskeyBrowserBinderTest, UpdateKeyLastUsedToNow) {
  base::HistogramTester histograms;
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  WebDataServiceRequestCallback web_data_service_callback;

  EXPECT_CALL(
      *mock_web_data_service_,
      UpdateBrowserBoundKeyLastUsed(fake_credential_id_, fake_relying_party_,
                                    base::Time::NowFromSystemTime(),
                                    /*callback=*/_))
      .WillOnce(MoveArgAndReturn<3>(&web_data_service_callback,
                                    fake_web_data_service_handle_));

  binder->UpdateKeyLastUsedToNow(fake_credential_id_, fake_relying_party_);
  EXPECT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<bool>>(WDResultType::BOOL_RESULT, true));

  histograms.ExpectUniqueSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyMetdataUpdate",
      true, 1);
}

TEST_F(PasskeyBrowserBinderTest, GetAllBrowserBoundKeys) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  std::vector<BrowserBoundKeyMetadata> bbk_metadatas;
  bbk_metadatas.push_back(MakeBrowserBoundKeyMetadata(
      fake_credential_id_, fake_relying_party_, fake_bbk_id_, fake_last_used_));
  base::MockOnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      mock_callback;
  WebDataServiceRequestCallback captured_get_all_browser_bound_keys_callback;

  EXPECT_CALL(*mock_web_data_service_, GetAllBrowserBoundKeys)
      .WillOnce(
          MoveArgAndReturn<0>(&captured_get_all_browser_bound_keys_callback,
                              fake_web_data_service_handle_));
  binder->GetAllBrowserBoundKeys(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(UnorderedElementsAre(EqBrowserBoundKeyMetadata(
                                 fake_relying_party_, fake_credential_id_,
                                 fake_bbk_id_, fake_last_used_))));
  std::move(captured_get_all_browser_bound_keys_callback)
      .Run(fake_web_data_service_handle_,
           std::make_unique<WDResult<std::vector<BrowserBoundKeyMetadata>>>(
               WDResultType::BROWSER_BOUND_KEY_METADATA,
               std::move(bbk_metadatas)));
}

TEST_F(PasskeyBrowserBinderTest, DeleteBrowserBoundKeys) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;

  Sequence sequence;
  EXPECT_CALL(
      *mock_web_data_service_,
      DeleteBrowserBoundKeys(UnorderedElementsAre(EqRelyingPartyAndCredentialId(
                                 fake_relying_party_, fake_credential_id_)),
                             _))
      .InSequence(sequence)
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(mock_callback, Run()).InSequence(sequence);

  EXPECT_TRUE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
  binder->DeleteBrowserBoundKeys(
      mock_callback.Get(),
      {MakeBrowserBoundKeyMetadata(fake_credential_id_, fake_relying_party_,
                                   fake_bbk_id_, fake_last_used_)});
  EXPECT_FALSE(fake_browser_bound_key_store_->ContainsFakeKey(fake_bbk_id_));
}

TEST_F(PasskeyBrowserBinderTest,
       DeleteBrowserBoundKeysWhenZeroBbkMetasProvided) {
  std::unique_ptr<PasskeyBrowserBinder> binder = CreatePasskeyBrowserBinder();
  base::MockOnceClosure mock_callback;

  EXPECT_CALL(*mock_web_data_service_, DeleteBrowserBoundKeys(_, _)).Times(0);
  EXPECT_CALL(mock_callback, Run());

  binder->DeleteBrowserBoundKeys(mock_callback.Get(), {});
}

}  // namespace
}  // namespace payments
