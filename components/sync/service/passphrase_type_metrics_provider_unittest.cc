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

const char kMetricWithoutCaching[] = "Sync.PassphraseType2";
const char kMetricWithCaching[] = "Sync.PassphraseType4";

using testing::Return;

class PassphraseTypeMetricsProviderTest : public testing::Test {
 public:
  PassphraseTypeMetricsProviderTest() = default;
  ~PassphraseTypeMetricsProviderTest() override = default;

  // The returned object must not outlive `this`.
  std::unique_ptr<PassphraseTypeMetricsProvider> CreateProvider(
      bool use_cached_passphrase_type) {
    // base::Unretained() is safe, because the provider must not outlive `this`.
    return std::make_unique<PassphraseTypeMetricsProvider>(
        use_cached_passphrase_type,
        base::BindRepeating(&PassphraseTypeMetricsProviderTest::GetSyncServices,
                            base::Unretained(this)));
  }

  // Adds sync service, which will be passed to the provider returned by
  // CreateProvider().
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

// Corresponds to the case where profiles were not initialized yet, and
// consequently there is no SyncService.
TEST_F(PassphraseTypeMetricsProviderTest, NoSyncService) {
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(kMetricWithoutCaching,
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectTotalCount(kMetricWithCaching, 0);
}

// The sync engine wasn't initialized yet, but will eventually be. This test
// captures the difference between Sync.PassphraseType2 (should record kUnknown)
// and Sync.PassphraseType4 (should check the cached passphrase type and
// record the correct value).
TEST_F(PassphraseTypeMetricsProviderTest, TransportInitializing) {
  AddSyncService(PassphraseType::kKeystorePassphrase,
                 /*sync_transport_active=*/false);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(kMetricWithoutCaching,
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordMultipleSyncingProfiles) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching,
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching,
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordKeystorePassphraseWithMultipleProfiles) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordImplicitPassphrase) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kImplicitPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordKeystorePassphrase) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordFrozenImplicitPassphrase) {
  AddSyncService(PassphraseType::kFrozenImplicitPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching,
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kFrozenImplicitPassphrase,
      1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordCustomPassphrase) {
  AddSyncService(PassphraseType::kCustomPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kCustomPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kCustomPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordTrustedVaultPassphrase) {
  AddSyncService(PassphraseType::kTrustedVaultPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(/*use_cached_passphrase_type=*/false)->OnDidCreateMetricsLog();
  CreateProvider(/*use_cached_passphrase_type=*/true)->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kTrustedVaultPassphrase,
      1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kTrustedVaultPassphrase, 1);
}

}  // namespace

}  // namespace syncer
