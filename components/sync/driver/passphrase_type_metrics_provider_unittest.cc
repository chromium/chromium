// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/passphrase_type_metrics_provider.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/metrics_features.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace syncer {

namespace {

using testing::Return;

class PassphraseTypeMetricsProviderTest
    : public testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  PassphraseTypeMetricsProviderTest() = default;
  ~PassphraseTypeMetricsProviderTest() override = default;

  bool ShouldEmitHistogramsEarlier() { return GetParam(); }

  void SetUp() override {
    // Using base::Unretained() here is safe, because |metrics_provider_| can't
    // outlive this and |sync_services_|.
    metrics_provider_ = std::make_unique<PassphraseTypeMetricsProvider>(
        base::BindRepeating(&PassphraseTypeMetricsProviderTest::GetSyncServices,
                            base::Unretained(this)));

    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }
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

  void ExpectRecordedPassphraseType(PassphraseTypeForMetrics expected) {
    base::HistogramTester histogram_tester;
    if (ShouldEmitHistogramsEarlier()) {
      metrics_provider_->OnDidCreateMetricsLog();
    } else {
      metrics::ChromeUserMetricsExtension uma_proto;
      metrics_provider_->ProvideCurrentSessionData(&uma_proto);
    }
    histogram_tester.ExpectUniqueSample("Sync.PassphraseType2", expected, 1);
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

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PassphraseTypeMetricsProviderTest,
                         testing::Bool());

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordNoSyncingProfiles) {
  ExpectRecordedPassphraseType(
      PassphraseTypeForMetrics::kNoActiveSyncingProfiles);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordSyncTransportInactive) {
  AddSyncService(PassphraseType::kKeystorePassphrase,
                 /*sync_transport_active=*/false);
  ExpectRecordedPassphraseType(
      PassphraseTypeForMetrics::kNoActiveSyncingProfiles);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordMultipleSyncingProfiles) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  ExpectRecordedPassphraseType(
      PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles);
}

TEST_P(PassphraseTypeMetricsProviderTest,
       ShouldRecordKeystorePassphraseWithMultipleProfiles) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  AddSyncService(PassphraseType::kKeystorePassphrase);
  ExpectRecordedPassphraseType(PassphraseTypeForMetrics::kKeystorePassphrase);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordImplicitPassphrase) {
  AddSyncService(PassphraseType::kImplicitPassphrase);
  ExpectRecordedPassphraseType(PassphraseTypeForMetrics::kImplicitPassphrase);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordKeystorePassphrase) {
  AddSyncService(PassphraseType::kKeystorePassphrase);
  ExpectRecordedPassphraseType(PassphraseTypeForMetrics::kKeystorePassphrase);
}

TEST_P(PassphraseTypeMetricsProviderTest,
       ShouldRecordFrozenImplicitPassphrase) {
  AddSyncService(PassphraseType::kFrozenImplicitPassphrase);
  ExpectRecordedPassphraseType(
      PassphraseTypeForMetrics::kFrozenImplicitPassphrase);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordCustomPassphrase) {
  AddSyncService(PassphraseType::kCustomPassphrase);
  ExpectRecordedPassphraseType(PassphraseTypeForMetrics::kCustomPassphrase);
}

TEST_P(PassphraseTypeMetricsProviderTest, ShouldRecordTrustedVaultPassphrase) {
  AddSyncService(PassphraseType::kTrustedVaultPassphrase);
  ExpectRecordedPassphraseType(
      PassphraseTypeForMetrics::kTrustedVaultPassphrase);
}

}  // namespace

}  // namespace syncer
