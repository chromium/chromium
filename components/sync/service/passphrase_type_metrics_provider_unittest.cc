// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/passphrase_type_metrics_provider.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Return;

class PassphraseTypeMetricsProviderTest : public testing::Test {
 public:
  PassphraseTypeMetricsProviderTest() = default;
  ~PassphraseTypeMetricsProviderTest() override = default;

  void SetUp() override {
    // Using base::Unretained() here is safe, because |metrics_provider_| can't
    // outlive this and |sync_services_|.
    metrics_provider_ = std::make_unique<PassphraseTypeMetricsProvider>(
        base::BindRepeating(&PassphraseTypeMetricsProviderTest::GetSyncServices,
                            base::Unretained(this)));
  }

  // Adds sync service, which will be provided to |metrics_provider_|.
  void AddSyncService(PassphraseType passphrase_type,
                      bool sync_transport_active = true) {
    sync_services_.emplace_back(
        std::make_unique<testing::NiceMock<MockSyncService>>());
    if (sync_transport_active) {
      // Will return DISABLED otherwise.
      ON_CALL(*sync_services_.back(), GetTransportState())
          .WillByDefault(Return(SyncService::TransportState::ACTIVE));
    }
    ON_CALL(*sync_services_.back()->GetMockUserSettings(), GetPassphraseType())
        .WillByDefault(Return(passphrase_type));
  }

  PassphraseTypeMetricsProvider* metrics_provider() {
    return metrics_provider_.get();
  }

 private:
  std::vector<const SyncService*> GetSyncServices() const {
    std::vector<const SyncService*> result;
    for (const auto& sync_service : sync_services_) {
      result.push_back(sync_service.get());
    }
    return result;
  }

  std::vector<std::unique_ptr<MockSyncService>> sync_services_;
  std::unique_ptr<PassphraseTypeMetricsProvider> metrics_provider_;
};

// Corresponds to a user in SyncService::TransportState::DISABLED, for whom the
// engine will never initialize.
TEST_F(PassphraseTypeMetricsProviderTest, TransportDisabled) {
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample("Sync.PassphraseType2",
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectUniqueSample("Sync.PassphraseType3",
                                      PassphraseTypeForMetrics::kUnknown, 1);
}

// The sync engine wasn't initialized yet, but will eventually be. This test
// captures the difference between Sync.PassphraseType2 (should record kUnknown)
// and Sync.PassphraseType3 (should check the cached passphrase type and
// record the correct value).
TEST_F(PassphraseTypeMetricsProviderTest, TransportInitializing) {
  AddSyncService(PassphraseType::kKeystorePassphrase,
                 /*sync_transport_active=*/false);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample("Sync.PassphraseType2",
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordMultipleSyncingProfiles) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2",
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3",
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordKeystorePassphraseWithMultipleProfiles) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2", PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordImplicitPassphrase) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2", PassphraseTypeForMetrics::kImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kImplicitPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordKeystorePassphrase) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2", PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordFrozenImplicitPassphrase) {
  AddSyncService(PassphraseType::kFrozenImplicitPassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2",
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3",
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordCustomPassphrase) {
  AddSyncService(PassphraseType::kCustomPassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2", PassphraseTypeForMetrics::kCustomPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kCustomPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordTrustedVaultPassphrase) {
  AddSyncService(PassphraseType::kTrustedVaultPassphrase);
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType2", PassphraseTypeForMetrics::kTrustedVaultPassphrase,
      1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseType3", PassphraseTypeForMetrics::kTrustedVaultPassphrase,
      1);
}

}  // namespace

}  // namespace syncer
