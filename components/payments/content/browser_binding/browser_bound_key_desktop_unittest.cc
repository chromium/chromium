// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_desktop.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/cose.h"
#include "crypto/test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using crypto::SignatureVerifier;
using testing::DoAll;
using testing::Return;
using unexportable_keys::MockUnexportableKey;

static const SignatureVerifier::SignatureAlgorithm kAllSignatureAlgorithms[] = {
    SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
    SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
    SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256};
}  // namespace

namespace payments {

class BrowserBoundKeyDesktopTest : public ::testing::Test {
 public:
  BrowserBoundKeyDesktopTest() {
    auto key = std::make_unique<MockUnexportableKey>();
    EXPECT_CALL(*key, Algorithm())
        .WillRepeatedly(
            Return(SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
    key_ = key.get();
    browser_bound_key_ =
        std::make_unique<BrowserBoundKeyDesktop>(std::move(key));
  }

  ~BrowserBoundKeyDesktopTest() override = default;

  MockUnexportableKey* key() { return key_; }

  BrowserBoundKeyDesktop* browser_bound_key() {
    return browser_bound_key_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<BrowserBoundKeyDesktop> browser_bound_key_;
  raw_ptr<MockUnexportableKey> key_;
};

TEST_F(BrowserBoundKeyDesktopTest, UnexportableSigningKey_AlgorithmValidation) {
  std::unique_ptr<MockUnexportableKey> key;
  for (const auto algorithm : kAllSignatureAlgorithms) {
    key = std::make_unique<MockUnexportableKey>();
    EXPECT_CALL(*key, Algorithm()).WillRepeatedly(Return(algorithm));

    if (algorithm == SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256 ||
        algorithm == SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256) {
      EXPECT_NO_FATAL_FAILURE(BrowserBoundKeyDesktop(std::move(key)));
    } else {
      EXPECT_CHECK_DEATH(BrowserBoundKeyDesktop(std::move(key)));
    }
  }
}

TEST_F(BrowserBoundKeyDesktopTest, GetIdentifier) {
  const std::vector<uint8_t> wrapped_key = {
      0, 1, 2, 3, 4,
  };

  EXPECT_CALL(*key(), GetWrappedKey()).WillRepeatedly(Return(wrapped_key));
  EXPECT_EQ(browser_bound_key()->GetIdentifier(), wrapped_key);
}

TEST_F(BrowserBoundKeyDesktopTest, Sign) {
  const std::vector<uint8_t> signed_data = {
      0, 1, 2, 3, 4,
  };
  const std::vector<uint8_t> client_data = {
      5, 6, 7, 8, 9,
  };

  EXPECT_CALL(*key(),
              SignSlowly(static_cast<base::span<const uint8_t>>(client_data)))
      .WillRepeatedly(Return(signed_data));
  EXPECT_EQ(browser_bound_key()->Sign(client_data), signed_data);
}

TEST_F(BrowserBoundKeyDesktopTest, Sign_SignSlowlyReturnsNullopt) {
  const std::vector<uint8_t> client_data = {
      0, 1, 2, 3, 4,
  };

  EXPECT_CALL(*key(),
              SignSlowly(static_cast<base::span<const uint8_t>>(client_data)))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_EQ(browser_bound_key()->Sign(client_data), std::vector<uint8_t>());
}

TEST_F(BrowserBoundKeyDesktopTest, GetPublicKeyAsCoseKey) {
  crypto::keypair::PublicKey public_key =
      crypto::test::FixedRsa2048PublicKeyForTesting();

  EXPECT_CALL(*key(), GetSubjectPublicKeyInfo())
      .WillRepeatedly(Return(public_key.ToSubjectPublicKeyInfo()));
  EXPECT_EQ(browser_bound_key()->GetPublicKeyAsCoseKey(),
            crypto::PublicKeyToCoseKey(public_key));
}

TEST_F(BrowserBoundKeyDesktopTest, Metrics_Sign) {
  base::HistogramTester histogram_tester;
  base::TimeDelta sign_latency = base::Microseconds(10);

  const std::vector<uint8_t> signed_data = {
      0, 1, 2, 3, 4,
  };
  const std::vector<uint8_t> client_data = {
      5, 6, 7, 8, 9,
  };

  EXPECT_CALL(*key(),
              SignSlowly(static_cast<base::span<const uint8_t>>(client_data)))
      .WillRepeatedly(DoAll(
          [this, &sign_latency] {
            task_environment_.FastForwardBy(sign_latency);
          },
          Return(signed_data)));
  browser_bound_key()->Sign(client_data);

  histogram_tester.ExpectUniqueTimeSample(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKey.SignLatency",
      sign_latency,
      /*expected_bucket_count=*/1);
}

}  // namespace payments
