// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_store_desktop.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_desktop.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/signature_verifier.h"
#include "device/fido/public_key_credential_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using crypto::SignatureVerifier;
using device::CoseAlgorithmIdentifier;
using device::PublicKeyCredentialParams;
using testing::_;
using testing::DoAll;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using unexportable_keys::MockUnexportableKey;
using unexportable_keys::MockUnexportableKeyProvider;
}  // namespace

namespace payments {
class BrowserBoundKeyStoreDesktopTest : public ::testing::Test {
 public:
  BrowserBoundKeyStoreDesktopTest() {
    auto key_provider = std::make_unique<MockUnexportableKeyProvider>();
    key_provider_ = key_provider.get();
    key_store_ = base::MakeRefCounted<BrowserBoundKeyStoreDesktop>(
        std::move(key_provider));
  }

  ~BrowserBoundKeyStoreDesktopTest() override = default;

  scoped_refptr<BrowserBoundKeyStore> key_store() { return key_store_; }
  MockUnexportableKeyProvider* key_provider() { return key_provider_; }

  const std::vector<uint8_t> kCredentialId = {0, 1, 2, 3, 4};
  const std::vector<PublicKeyCredentialParams::CredentialInfo>
      kAllowedCredentials = {
          PublicKeyCredentialParams::CredentialInfo{
              .algorithm =
                  base::strict_cast<int32_t>(CoseAlgorithmIdentifier::kEs256)},
          PublicKeyCredentialParams::CredentialInfo{
              .algorithm =
                  base::strict_cast<int32_t>(CoseAlgorithmIdentifier::kEdDSA)},
          PublicKeyCredentialParams::CredentialInfo{
              .algorithm =
                  base::strict_cast<int32_t>(CoseAlgorithmIdentifier::kRs256)}};

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<BrowserBoundKeyStore> key_store_;
  raw_ptr<MockUnexportableKeyProvider> key_provider_;
};

TEST_F(BrowserBoundKeyStoreDesktopTest,
       GetOrCreateBrowserBoundKeyForCredentialId_Get) {
  std::unique_ptr<MockUnexportableKey> key =
      std::make_unique<MockUnexportableKey>();
  EXPECT_CALL(*key, Algorithm())
      .WillRepeatedly(
          Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  MockUnexportableKey* key_ptr = key.get();

  EXPECT_CALL(*key_provider(), FromWrappedSigningKeySlowly(
                                   base::span<const uint8_t>(kCredentialId)))
      .WillOnce(Return(std::move(key)));

  std::unique_ptr<BrowserBoundKey> browser_bound_key =
      key_store()->GetOrCreateBrowserBoundKeyForCredentialId(
          kCredentialId, kAllowedCredentials);
  EXPECT_THAT(browser_bound_key, NotNull());
  EXPECT_EQ(static_cast<BrowserBoundKeyDesktop*>(browser_bound_key.get())
                ->GetKeyForTesting(),
            key_ptr);
}

TEST_F(BrowserBoundKeyStoreDesktopTest,
       GetOrCreateBrowserBoundKeyForCredentialId_Create) {
  std::unique_ptr<MockUnexportableKey> key =
      std::make_unique<MockUnexportableKey>();
  EXPECT_CALL(*key, Algorithm())
      .WillRepeatedly(
          Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  const std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algorithms =
      {SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
       SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  MockUnexportableKey* key_ptr = key.get();

  EXPECT_CALL(*key_provider(), FromWrappedSigningKeySlowly(
                                   base::span<const uint8_t>(kCredentialId)))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(
      *key_provider(),
      GenerateSigningKeySlowly(
          base::span<const SignatureVerifier::SignatureAlgorithm>(algorithms)))
      .WillOnce(Return(std::move(key)));

  std::unique_ptr<BrowserBoundKey> browser_bound_key =
      key_store()->GetOrCreateBrowserBoundKeyForCredentialId(
          kCredentialId, kAllowedCredentials);
  EXPECT_THAT(browser_bound_key, NotNull());
  EXPECT_EQ(static_cast<BrowserBoundKeyDesktop*>(browser_bound_key.get())
                ->GetKeyForTesting(),
            key_ptr);
}

TEST_F(BrowserBoundKeyStoreDesktopTest,
       GetOrCreateBrowserBoundKeyForCredentialId_NullKeyProvider) {
  scoped_refptr<BrowserBoundKeyStore> key_store =
      base::MakeRefCounted<BrowserBoundKeyStoreDesktop>(nullptr);
  EXPECT_THAT(key_store->GetOrCreateBrowserBoundKeyForCredentialId(
                  kCredentialId, kAllowedCredentials),
              IsNull());
}

TEST_F(BrowserBoundKeyStoreDesktopTest, DeleteBrowserBoundKey) {
  EXPECT_CALL(*key_provider(),
              DeleteSigningKeySlowly(base::span<const uint8_t>(kCredentialId)));
  key_store()->DeleteBrowserBoundKey(kCredentialId);
}

TEST_F(BrowserBoundKeyStoreDesktopTest, DeleteBrowserBoundKey_NullKeyProvider) {
  scoped_refptr<BrowserBoundKeyStore> key_store =
      base::MakeRefCounted<BrowserBoundKeyStoreDesktop>(nullptr);

  EXPECT_CALL(*key_provider(), DeleteSigningKeySlowly(_)).Times(0);
  key_store->DeleteBrowserBoundKey(kCredentialId);
}

TEST_F(BrowserBoundKeyStoreDesktopTest, GetDeviceSupportsHardwareKeys) {
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(key_store()->GetDeviceSupportsHardwareKeys());
#elif BUILDFLAG(IS_WIN)
  EXPECT_CALL(*key_provider(), SelectAlgorithm(_))
      .WillRepeatedly(
          Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_TRUE(key_store()->GetDeviceSupportsHardwareKeys());
#else  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  EXPECT_FALSE(key_store()->GetDeviceSupportsHardwareKeys());
#endif
}

TEST_F(BrowserBoundKeyStoreDesktopTest,
       GetDeviceSupportsHardwareKeys_NullKeyProvider) {
  scoped_refptr<BrowserBoundKeyStore> key_store =
      base::MakeRefCounted<BrowserBoundKeyStoreDesktop>(nullptr);
  EXPECT_FALSE(key_store->GetDeviceSupportsHardwareKeys());
}

#if BUILDFLAG(IS_WIN)
TEST_F(BrowserBoundKeyStoreDesktopTest,
       GetDeviceSupportsHardwareKeys_NullOptAlgorithm) {
  EXPECT_CALL(*key_provider(), SelectAlgorithm(_))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_FALSE(key_store()->GetDeviceSupportsHardwareKeys());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(BrowserBoundKeyStoreDesktopTest,
       Metrics_GetOrCreateBrowserBoundKeyForCredentialId_Get) {
  base::HistogramTester histogram_tester;
  base::TimeDelta get_key_latency = base::Microseconds(10);

  std::unique_ptr<MockUnexportableKey> key =
      std::make_unique<MockUnexportableKey>();

  EXPECT_CALL(*key, Algorithm())
      .WillRepeatedly(
          Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(*key_provider(), FromWrappedSigningKeySlowly(
                                   base::span<const uint8_t>(kCredentialId)))
      .WillOnce(DoAll(
          [this, &get_key_latency] {
            task_environment_.FastForwardBy(get_key_latency);
          },
          Return(std::move(key))));

  key_store()->GetOrCreateBrowserBoundKeyForCredentialId(kCredentialId,
                                                         kAllowedCredentials);

  histogram_tester.ExpectUniqueTimeSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "GetKeyLatency.KeyFound",
      get_key_latency,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "GetKeyLatency.KeyNotFound",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "CreateKeyLatency",
      /*expected_count=*/0);
}

TEST_F(BrowserBoundKeyStoreDesktopTest,
       Metrics_GetOrCreateBrowserBoundKeyForCredentialId_Create) {
  base::HistogramTester histogram_tester;
  base::TimeDelta get_key_latency = base::Microseconds(10);
  base::TimeDelta create_key_latency = base::Microseconds(20);

  const std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algorithms =
      {SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
       SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  std::unique_ptr<MockUnexportableKey> key =
      std::make_unique<MockUnexportableKey>();

  EXPECT_CALL(*key, Algorithm())
      .WillRepeatedly(
          Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
  EXPECT_CALL(*key_provider(), FromWrappedSigningKeySlowly(
                                   base::span<const uint8_t>(kCredentialId)))
      .WillOnce(DoAll(
          [this, &get_key_latency] {
            task_environment_.FastForwardBy(get_key_latency);
          },
          Return(nullptr)));
  EXPECT_CALL(
      *key_provider(),
      GenerateSigningKeySlowly(
          base::span<const SignatureVerifier::SignatureAlgorithm>(algorithms)))
      .WillOnce(DoAll(
          [this, &create_key_latency] {
            task_environment_.FastForwardBy(create_key_latency);
          },
          Return(std::move(key))));

  key_store()->GetOrCreateBrowserBoundKeyForCredentialId(kCredentialId,
                                                         kAllowedCredentials);

  histogram_tester.ExpectUniqueTimeSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "GetKeyLatency.KeyNotFound",
      get_key_latency,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueTimeSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "CreateKeyLatency",
      create_key_latency,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore."
      "GetKeyLatency.KeyFound",
      /*expected_count=*/0);
}

}  // namespace payments
