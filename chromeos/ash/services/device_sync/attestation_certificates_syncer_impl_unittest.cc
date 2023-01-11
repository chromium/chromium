// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeCert[] = "fake cert";

}  // namespace
class AttestationCertificatesSyncerImplTest : public testing::Test {
 public:
  AttestationCertificatesSyncerImplTest(
      const AttestationCertificatesSyncerImplTest&) = delete;
  AttestationCertificatesSyncerImplTest& operator=(
      const AttestationCertificatesSyncerImplTest&) = delete;

  void SaveCerts(const std::vector<std::string>& certs, bool valid) {
    result_ = certs;
  }

 protected:
  AttestationCertificatesSyncerImplTest() = default;

  ~AttestationCertificatesSyncerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /* enabled_features= */ {features::kEcheSWA,
                                 features::kCryptauthAttestationSyncing},
        /* disabled_features= */ {});

    AttestationCertificatesSyncerImpl::RegisterPrefs(pref_service_.registry());

    fake_cryptauth_scheduler_.StartDeviceSyncScheduling(
        fake_device_sync_delegate_.GetWeakPtr());

    attestation_syncer_ = AttestationCertificatesSyncerImpl::Factory::Create(
        &fake_cryptauth_scheduler_, &pref_service_,
        base::BindRepeating(
            [](AttestationCertificatesSyncer::NotifyCallback notify_callback,
               const std::string&) {
              std::move(notify_callback)
                  .Run(std::vector<std::string>{kFakeCert}, /*valid=*/true);
            }));
  }

  // testing::Test:
  void TearDown() override {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AttestationCertificatesSyncer> attestation_syncer_;
  FakeCryptAuthSchedulerDeviceSyncDelegate fake_device_sync_delegate_;
  FakeCryptAuthScheduler fake_cryptauth_scheduler_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::vector<std::string> result_;
};

TEST_F(AttestationCertificatesSyncerImplTest,
       UpdateCertsSuccessfulWithCorrectTimestamp) {
  attestation_syncer_->UpdateCerts(
      base::BindOnce(&AttestationCertificatesSyncerImplTest::SaveCerts,
                     base::Unretained(this)),
      "userkey");
  attestation_syncer_->SetLastSyncTimestamp();
  base::Time expected_timestamp = base::Time::Now();
  task_environment_.AdvanceClock(base::Hours(1));
  EXPECT_EQ(
      expected_timestamp,
      pref_service_.GetTime(
          prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp));
  EXPECT_EQ(kFakeCert, result_[0]);
}

TEST_F(AttestationCertificatesSyncerImplTest, FirstSyncScheduleAlwaysSyncs) {
  attestation_syncer_->ScheduleSyncForTest();
  EXPECT_EQ(1u, fake_cryptauth_scheduler_.num_sync_requests());
}

TEST_F(AttestationCertificatesSyncerImplTest, TwoHoursToExpiry) {
  pref_service_.SetTime(
      prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp,
      base::Time::Now());
  task_environment_.AdvanceClock(base::Hours(70));
  attestation_syncer_->ScheduleSyncForTest();
  EXPECT_EQ(0u, fake_cryptauth_scheduler_.num_sync_requests());
}

TEST_F(AttestationCertificatesSyncerImplTest, ThirtyMinutesToExpiry) {
  pref_service_.SetTime(
      prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp,
      base::Time::Now());
  task_environment_.AdvanceClock(base::Hours(71) + base::Minutes(30));
  attestation_syncer_->ScheduleSyncForTest();
  EXPECT_EQ(1u, fake_cryptauth_scheduler_.num_sync_requests());
}

}  // namespace device_sync

}  // namespace ash
