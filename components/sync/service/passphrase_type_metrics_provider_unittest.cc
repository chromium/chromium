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

// Original metric, only recorded once Sync has become active.
const char kMetricWithoutCaching[] = "Sync.PassphraseType2";
// Supposed replacement metric, based on cached state in prefs, which however
// counts signed-out users into the kImplicitPassphrase bucket.
const char kMetricWithCaching[] = "Sync.PassphraseType4";
// Replacement metric, based on cached state in prefs.
const char kMetricWithCachingAndUnknown[] = "Sync.PassphraseType5";

using testing::Return;

class PassphraseTypeMetricsProviderTest : public testing::Test {
 public:
  PassphraseTypeMetricsProviderTest() = default;
  ~PassphraseTypeMetricsProviderTest() override = default;

  // The returned object must not outlive `this`.
  std::unique_ptr<PassphraseTypeMetricsProvider> CreateProvider(
      PassphraseTypeMetricsProvider::HistogramVersion histogram_version) {
    // base::Unretained() is safe, because the provider must not outlive `this`.
    return std::make_unique<PassphraseTypeMetricsProvider>(
        histogram_version,
        base::BindRepeating(&PassphraseTypeMetricsProviderTest::GetSyncServices,
                            base::Unretained(this)));
  }

  // Adds sync service, which will be passed to the provider returned by
  // CreateProvider().
  void AddSyncService(std::optional<PassphraseType> passphrase_type,
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
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(kMetricWithoutCaching,
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectTotalCount(kMetricWithCaching, 0);
  histogram_tester.ExpectTotalCount(kMetricWithCachingAndUnknown, 0);
}

// The sync engine wasn't initialized yet, but will eventually be. This test
// captures the difference between Sync.PassphraseType2 (should record kUnknown)
// and Sync.PassphraseType4 (should check the cached passphrase type and
// record the correct value).
TEST_F(PassphraseTypeMetricsProviderTest, TransportInitializing) {
  AddSyncService(PassphraseType::kKeystorePassphrase,
                 /*sync_transport_active=*/false);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(kMetricWithoutCaching,
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

// There is no signed-in users, so the passphrase type is unknown. This test
// captures the difference between Sync.PassphraseType4 (confusingly records
// kImplicitPassphrase) and Sync.PassphraseType5 (correctly records kUnknown).
TEST_F(PassphraseTypeMetricsProviderTest, SignedOut) {
  AddSyncService(/*passphrase_type=*/std::nullopt);

  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(kMetricWithoutCaching,
                                      PassphraseTypeForMetrics::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(kMetricWithCachingAndUnknown,
                                      PassphraseTypeForMetrics::kUnknown, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordMultipleSyncingProfiles) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching,
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching,
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordKeystorePassphraseWithMultipleProfiles) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordImplicitPassphrase) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kImplicitPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordKeystorePassphrase) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kKeystorePassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kKeystorePassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest,
       ShouldRecordFrozenImplicitPassphrase) {
  AddSyncService(PassphraseType::kFrozenImplicitPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching,
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kFrozenImplicitPassphrase,
      1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase, 1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordCustomPassphrase) {
  AddSyncService(PassphraseType::kCustomPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kCustomPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kCustomPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown, PassphraseTypeForMetrics::kCustomPassphrase,
      1);
}

TEST_F(PassphraseTypeMetricsProviderTest, ShouldRecordTrustedVaultPassphrase) {
  AddSyncService(PassphraseType::kTrustedVaultPassphrase);
  base::HistogramTester histogram_tester;
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV2)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV4)
      ->OnDidCreateMetricsLog();
  CreateProvider(PassphraseTypeMetricsProvider::HistogramVersion::kV5)
      ->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kMetricWithoutCaching, PassphraseTypeForMetrics::kTrustedVaultPassphrase,
      1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCaching, PassphraseTypeForMetrics::kTrustedVaultPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricWithCachingAndUnknown,
      PassphraseTypeForMetrics::kTrustedVaultPassphrase, 1);
}

}  // namespace

}  // namespace syncer
