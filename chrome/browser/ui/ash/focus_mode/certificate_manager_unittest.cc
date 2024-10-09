// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/certificate_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::Eq;
using testing::Ne;

const AccountId kTestAccount = AccountId::FromUserEmail("user@example.com");
const base::TimeDelta kTestBuffer = base::Hours(1);

class CertificateManagerTest : public testing::Test {
 public:
  CertificateManagerTest() = default;
  ~CertificateManagerTest() override = default;

  void SetUp() override {
    auto mock_attestation =
        std::make_unique<ash::attestation::MockAttestationFlow>();
    mock_attestation_flow_ = mock_attestation.get();
    certificate_manager_ = CertificateManager::CreateForTesting(
        kTestAccount, kTestBuffer, std::move(mock_attestation),
        &fake_attestation_client_);
  }

  void TearDown() override {
    mock_attestation_flow_ = nullptr;
    certificate_manager_.reset();
  }

  ash::StubCrosSettingsProvider* cros_settings() {
    return test_cros_settings_.device_settings();
  }

  CertificateManager* certificate_manager() {
    return certificate_manager_.get();
  }

  ash::attestation::MockAttestationFlow* mock_attestation_flow() {
    return mock_attestation_flow_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  ash::ScopedTestingCrosSettings test_cros_settings_;
  raw_ptr<ash::attestation::MockAttestationFlow> mock_attestation_flow_;
  ash::FakeAttestationClient fake_attestation_client_;
  std::unique_ptr<CertificateManager> certificate_manager_;
};

// Verifies that if content protection is disabled, no requests are made.
TEST_F(CertificateManagerTest, GetCertificate_PolicyDenied) {
  cros_settings()->SetBoolean(ash::kAttestationForContentProtectionEnabled,
                              false);

  // If policy is not allowed, `GetCertificate()` returns immediately and does
  // not invoke the attestation flow.
  EXPECT_CALL(*mock_attestation_flow(),
              GetCertificate(/*profile=*/_,
                             /*account_id=*/_,
                             /*request_origin=*/_,
                             /*force_new_key=*/_,
                             /*key_crypto_type=*/_,
                             /*key_name=*/_,
                             /*profile_specific_data=*/_,
                             /*callback=*/_))
      .Times(0);

  // `GetCertificate()` does nothing and returns false if the policy is
  // disabled.
  EXPECT_FALSE(certificate_manager()->GetCertificate(false, base::DoNothing()));
}

// Verifies that a certificate is requested via the attestation flow.
TEST_F(CertificateManagerTest, GetCertificate) {
  cros_settings()->SetBoolean(ash::kAttestationForContentProtectionEnabled,
                              true);

  EXPECT_CALL(
      *mock_attestation_flow(),
      GetCertificate(ash::attestation::PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                     /*account_id=*/_,
                     /*request_origin=*/"youtubemediaconnect.googleapis.com",
                     /*force_new_key=*/false,
                     /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
                     /*key_name=*/_,
                     /*profile_specific_data=*/_,
                     /*callback=*/_));

  ASSERT_TRUE(certificate_manager()->GetCertificate(false, base::DoNothing()));
}

// Request signing but denied by policy.
TEST_F(CertificateManagerTest, Sign_Denied) {
  cros_settings()->SetBoolean(ash::kAttestationForContentProtectionEnabled,
                              false);

  // Pick an expiration arbitrarily far in the future.
  base::Time expiration = base::Time::Now() + base::Days(14);
  CertificateManager::Key key("CrOSFocusMode", expiration);

  auto status =
      certificate_manager()->Sign(key, "TEST_PAYLOAD", base::DoNothing());
  EXPECT_THAT(status,
              Eq(CertificateManager::CertificateResult::kDisallowedByPolicy));
}

// Request signing with an expired certificate.
TEST_F(CertificateManagerTest, Sign_Expired) {
  cros_settings()->SetBoolean(ash::kAttestationForContentProtectionEnabled,
                              true);

  // Pick an expiration in the past.
  base::Time expiration = base::Time::Now() - base::Days(14);
  CertificateManager::Key key("CrOSFocusMode", expiration);

  auto status =
      certificate_manager()->Sign(key, "TEST_PAYLOAD", base::DoNothing());
  EXPECT_THAT(status,
              Eq(CertificateManager::CertificateResult::kCertificateExpired));
}

// Request signing with a key that is not from `GetCertificate()`.
TEST_F(CertificateManagerTest, Sign_InvalidKey) {
  cros_settings()->SetBoolean(ash::kAttestationForContentProtectionEnabled,
                              true);

  // Pick an arbitrary date in the future that does not match the cached
  // certificate.
  base::Time expiration = base::Time::Now() + base::Days(14);
  CertificateManager::Key key("CrOSFocusMode", expiration);

  auto status =
      certificate_manager()->Sign(key, "TEST_PAYLOAD", base::DoNothing());
  EXPECT_THAT(status, Eq(CertificateManager::CertificateResult::kInvalidKey));
}

// Request for signing is fulfilled.
TEST_F(CertificateManagerTest, Sign) {
  ash::attestation::AttestationFlow::CertificateCallback certificate_callback;
  EXPECT_CALL(*mock_attestation_flow(), GetCertificate)
      .WillOnce(
          [&](ash::attestation::AttestationCertificateProfile, const AccountId&,
              const std::string&, bool, ::attestation::KeyType,
              const std::string&,
              const std::optional<
                  ash::attestation::AttestationFlow::CertProfileSpecificData>&,
              ash::attestation::AttestationFlow::CertificateCallback callback) {
            certificate_callback = std::move(callback);
          });

  std::optional<CertificateManager::Key> certificate_key;
  bool cert_status = certificate_manager()->GetCertificate(
      false, base::BindLambdaForTesting(
                 [&](const std::optional<CertificateManager::Key>& key) {
                   // Check that the key is not null since retrieval should be
                   // successful.
                   ASSERT_THAT(key, Ne(std::nullopt));
                   certificate_key.emplace(*key);
                 }));
  ASSERT_TRUE(cert_status);

  std::string certificate;
  ASSERT_TRUE(
      ash::attestation::GetFakeCertificatePEM(base::Days(30), &certificate));

  // Fulfill the request for a certificate.
  std::move(certificate_callback)
      .Run(ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
           certificate);

  // Verify that we received a key.
  ASSERT_THAT(certificate_key, Ne(std::nullopt));

  base::RunLoop run_loop;
  auto status = certificate_manager()->Sign(
      *certificate_key, "TEST_PAYLOAD",
      base::IgnoreArgs<bool, const std::string&, const std::string&,
                       const std::vector<std::string>&>(
          run_loop.QuitClosure()));
  EXPECT_THAT(status, Eq(CertificateManager::CertificateResult::kSuccess));

  // Wait for the `FakeAttestationClient` to finish.
  run_loop.Run();
}

}  // namespace
