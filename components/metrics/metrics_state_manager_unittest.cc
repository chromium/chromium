// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_data_validation.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace metrics {
namespace {

// Verifies that the client id follows the expected pattern.
void VerifyClientId(const std::string& client_id) {
  EXPECT_EQ(36U, client_id.length());

  for (size_t i = 0; i < client_id.length(); ++i) {
    char current = client_id[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(absl::ascii_isxdigit(static_cast<unsigned char>(current)));
  }
}

MATCHER(HaveClonedInstallInfo, "") {
  return (
      !arg.FindPreference(prefs::kClonedResetCount)->IsDefaultValue() &&
      !arg.FindPreference(prefs::kFirstClonedResetTimestamp)
           ->IsDefaultValue() &&
      !arg.FindPreference(prefs::kLastClonedResetTimestamp)->IsDefaultValue());
}

MATCHER(HaveNoClonedInstallInfo, "") {
  return (
      arg.FindPreference(prefs::kClonedResetCount)->IsDefaultValue() &&
      arg.FindPreference(prefs::kFirstClonedResetTimestamp)->IsDefaultValue() &&
      arg.FindPreference(prefs::kLastClonedResetTimestamp)->IsDefaultValue());
}

}  // namespace

class MetricsStateManagerTest : public testing::Test {
 public:
  MetricsStateManagerTest()
      : test_begin_time_(base::Time::Now().ToTimeT()),
        enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    MetricsService::RegisterPrefs(prefs_.registry());
  }

  MetricsStateManagerTest(const MetricsStateManagerTest&) = delete;
  MetricsStateManagerTest& operator=(const MetricsStateManagerTest&) = delete;

  std::unique_ptr<MetricsStateManager> CreateStateManager(
      const std::string& external_client_id = "") {
    std::unique_ptr<MetricsStateManager> state_manager =
        MetricsStateManager::Create(
            &prefs_, enabled_state_provider_.get(), std::wstring(),
            base::FilePath(), StartupVisibility::kUnknown, {},
            base::BindRepeating(
                &MetricsStateManagerTest::MockStoreClientInfoBackup,
                base::Unretained(this)),
            base::BindRepeating(
                &MetricsStateManagerTest::LoadFakeClientInfoBackup,
                base::Unretained(this)),
            external_client_id);
    state_manager->InstantiateFieldTrialList();
    return state_manager;
  }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

  void SetClientInfoPrefs(const ClientInfo& client_info) {
    prefs_.SetString(prefs::kMetricsClientID, client_info.client_id);
    prefs_.SetInt64(prefs::kInstallDate, client_info.installation_date);
    prefs_.SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                    client_info.reporting_enabled_date);
  }

  void SetFakeClientInfoBackup(const ClientInfo& client_info) {
    fake_client_info_backup_ = std::make_unique<ClientInfo>();
    fake_client_info_backup_->client_id = client_info.client_id;
    fake_client_info_backup_->installation_date = client_info.installation_date;
    fake_client_info_backup_->reporting_enabled_date =
        client_info.reporting_enabled_date;
  }

  // The number of times that the code tries to load ClientInfo.
  int client_info_load_count_ = 0;

 protected:
  TestingPrefServiceSimple prefs_;

  // Last ClientInfo stored by the MetricsStateManager via
  // MockStoreClientInfoBackup.
  std::unique_ptr<ClientInfo> stored_client_info_backup_;

  // If set, will be returned via LoadFakeClientInfoBackup if requested by the
  // MetricsStateManager.
  std::unique_ptr<ClientInfo> fake_client_info_backup_;

  const int64_t test_begin_time_;

 private:
  // Stores the |client_info| in |stored_client_info_backup_| for verification
  // by the tests later.
  void MockStoreClientInfoBackup(const ClientInfo& client_info) {
    stored_client_info_backup_ = std::make_unique<ClientInfo>();
    stored_client_info_backup_->client_id = client_info.client_id;
    stored_client_info_backup_->installation_date =
        client_info.installation_date;
    stored_client_info_backup_->reporting_enabled_date =
        client_info.reporting_enabled_date;

    // Respect the contract that storing an empty client_id voids the existing
    // backup (required for the last section of the ForceClientIdCreation test
    // below).
    if (client_info.client_id.empty())
      fake_client_info_backup_.reset();
  }

  // Hands out a copy of |fake_client_info_backup_| if it is set.
  std::unique_ptr<ClientInfo> LoadFakeClientInfoBackup() {
    ++client_info_load_count_;
    if (!fake_client_info_backup_)
      return nullptr;

    std::unique_ptr<ClientInfo> backup_copy(new ClientInfo);
    backup_copy->client_id = fake_client_info_backup_->client_id;
    backup_copy->installation_date =
        fake_client_info_backup_->installation_date;
    backup_copy->reporting_enabled_date =
        fake_client_info_backup_->reporting_enabled_date;
    return backup_copy;
  }

  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;
};

TEST_F(MetricsStateManagerTest, ClientIdCorrectlyFormatted_ConsentInitially) {
  // With consent set initially, client id should be created in the constructor.
  EnableMetricsReporting();
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

  const std::string client_id = state_manager->client_id();
  VerifyClientId(client_id);
}

TEST_F(MetricsStateManagerTest, ClientIdCorrectlyFormatted_ConsentLater) {
  // With consent set initially, client id should be created on consent grant.
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  EXPECT_EQ(std::string(), state_manager->client_id());

  EnableMetricsReporting();
  state_manager->ForceClientIdCreation();
  const std::string client_id = state_manager->client_id();
  VerifyClientId(client_id);
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_Low) {
  // Set the install date pref, which makes sure we don't trigger the first run
  // behavior where a provisional client id is generated and used to return a
  // high entropy source.
  prefs_.SetInt64(prefs::kInstallDate, base::Time::Now().ToTimeT());

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  // |enable_limited_entropy_mode| is irrelevant but is set for test coverage.
  state_manager->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
  EXPECT_EQ(state_manager->entropy_source_returned(),
            MetricsStateManager::ENTROPY_SOURCE_LOW);
  EXPECT_EQ(state_manager->initial_client_id_for_testing(), "");
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_High) {
  EnableMetricsReporting();
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  // |enable_limited_entropy_mode| is irrelevant but is set for test coverage.
  state_manager->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
  EXPECT_EQ(state_manager->entropy_source_returned(),
            MetricsStateManager::ENTROPY_SOURCE_HIGH);
  EXPECT_EQ(state_manager->initial_client_id_for_testing(),
            state_manager->client_id());
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_High_ExternalClientId) {
  EnableMetricsReporting();
  const std::string kExternalClientId = "abc";
  std::unique_ptr<MetricsStateManager> state_manager(
      CreateStateManager(kExternalClientId));
  // |enable_limited_entropy_mode| is irrelevant but is set for test coverage.
  state_manager->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
  EXPECT_EQ(state_manager->entropy_source_returned(),
            MetricsStateManager::ENTROPY_SOURCE_HIGH);
  EXPECT_EQ(state_manager->client_id(), kExternalClientId);
  EXPECT_EQ(state_manager->initial_client_id_for_testing(), kExternalClientId);
}

TEST_F(MetricsStateManagerTest,
       EntropySourceUsed_High_ExternalClientId_MetricsReportingDisabled) {
  const std::string kExternalClientId = "abc";
  std::unique_ptr<MetricsStateManager> state_manager(
      CreateStateManager(kExternalClientId));
  // |enable_limited_entropy_mode| is irrelevant but is set for test coverage.
  state_manager->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
  EXPECT_TRUE(state_manager->client_id().empty());
  EXPECT_EQ(state_manager->entropy_source_returned(),
            MetricsStateManager::ENTROPY_SOURCE_HIGH);
  EXPECT_EQ(state_manager->initial_client_id_for_testing(), kExternalClientId);
}

// Check that setting the kMetricsResetIds pref to true causes the client id to
// be reset. We do not check that the low entropy source is reset because we
// cannot ensure that metrics state manager won't generate the same id again.
TEST_F(MetricsStateManagerTest, ResetMetricsIDs) {
  // Set an initial client id in prefs. It should not be possible for the
  // metrics state manager to generate this id randomly.
  const std::string kInitialClientId = "initial client id";
  prefs_.SetString(prefs::kMetricsClientID, kInitialClientId);

  EnableMetricsReporting();

  // No cloned install info should have been stored.
  EXPECT_THAT(prefs_, HaveNoClonedInstallInfo());

  // Make sure the initial client id isn't reset by the metrics state manager.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_EQ(state_manager->client_id(), kInitialClientId);
    EXPECT_FALSE(state_manager->metrics_ids_were_reset_);
    EXPECT_THAT(prefs_, HaveNoClonedInstallInfo());
  }

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  // Cause the actual reset to happen.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_NE(state_manager->client_id(), kInitialClientId);
    EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
    EXPECT_EQ(state_manager->previous_client_id_, kInitialClientId);
    EXPECT_EQ(client_info_load_count_, 0);

    state_manager->GetLowEntropySource();

    EXPECT_FALSE(prefs_.GetBoolean(prefs::kMetricsResetIds));

    EXPECT_THAT(prefs_, HaveClonedInstallInfo());
    EXPECT_EQ(prefs_.GetInteger(prefs::kClonedResetCount), 1);
    EXPECT_EQ(prefs_.GetInt64(prefs::kFirstClonedResetTimestamp),
              prefs_.GetInt64(prefs::kLastClonedResetTimestamp));
  }

  EXPECT_NE(prefs_.GetString(prefs::kMetricsClientID), kInitialClientId);
}

TEST_F(MetricsStateManagerTest, LogHasSessionShutdownCleanly) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  prefs_.SetBoolean(prefs::kStabilityExitedCleanly, false);
  state_manager->LogHasSessionShutdownCleanly(
      /*has_session_shutdown_cleanly=*/true);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
}

TEST_F(MetricsStateManagerTest, LogSessionHasNotYetShutdownCleanly) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  ASSERT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  state_manager->LogHasSessionShutdownCleanly(
      /*has_session_shutdown_cleanly=*/false);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
}

TEST_F(MetricsStateManagerTest, ForceClientIdCreation) {
  const int64_t kFakeInstallationDate = 12345;
  prefs_.SetInt64(prefs::kInstallDate, kFakeInstallationDate);

  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id shouldn't be auto-generated if metrics reporting is not
    // enabled.
    EXPECT_EQ(state_manager->client_id(), std::string());
    EXPECT_EQ(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp), 0);

    // Confirm that the initial ForceClientIdCreation call creates the client id
    // and backs it up via MockStoreClientInfoBackup.
    EXPECT_FALSE(stored_client_info_backup_);
    EnableMetricsReporting();
    state_manager->ForceClientIdCreation();
    EXPECT_NE(state_manager->client_id(), std::string());
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time_);

    ASSERT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(client_info_load_count_, 1);
    EXPECT_EQ(state_manager->client_id(),
              stored_client_info_backup_->client_id);
    EXPECT_EQ(stored_client_info_backup_->installation_date,
              kFakeInstallationDate);
    EXPECT_EQ(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              stored_client_info_backup_->reporting_enabled_date);
  }
}

TEST_F(MetricsStateManagerTest,
       ForceClientIdCreation_ConsentIntitially_NoInstallDate) {
  // Confirm that the initial ForceClientIdCreation call creates the install
  // date and then backs it up via MockStoreClientInfoBackup.
  EXPECT_FALSE(stored_client_info_backup_);
  EnableMetricsReporting();
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

  ASSERT_TRUE(stored_client_info_backup_);
  EXPECT_NE(stored_client_info_backup_->installation_date, 0);
  EXPECT_EQ(client_info_load_count_, 1);
}

#if !BUILDFLAG(IS_WIN)
TEST_F(MetricsStateManagerTest, ProvisionalClientId_PromotedToClientId) {
  // Force enable the creation of a provisional client ID on first run for
  // consistency between Chromium and Chrome builds.
  MetricsStateManager::enable_provisional_client_id_for_testing_ = true;

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

  // Verify that there was a provisional client id created.
  std::string provisional_client_id =
      prefs_.GetString(prefs::kMetricsProvisionalClientID);
  VerifyClientId(provisional_client_id);
  // No client id should have been stored.
  EXPECT_TRUE(prefs_.FindPreference(prefs::kMetricsClientID)->IsDefaultValue());
  int low_entropy_source = state_manager->GetLowEntropySource();
  // The default entropy provider should be the high entropy one since we have a
  // provisional client ID. |enable_limited_entropy_mode| is irrelevant but is
  // set to true for test coverage.
  state_manager->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
  EXPECT_EQ(state_manager->entropy_source_returned(),
            MetricsStateManager::ENTROPY_SOURCE_HIGH);
  // The high entropy source used should be the provisional client ID.
  EXPECT_EQ(state_manager->initial_client_id_for_testing(),
            provisional_client_id);

  // Forcing client id creation should promote the provisional client id to
  // become the real client id and keep the low entropy source.
  EnableMetricsReporting();
  state_manager->ForceClientIdCreation();
  std::string client_id = state_manager->client_id();
  EXPECT_EQ(provisional_client_id, client_id);
  EXPECT_EQ(prefs_.GetString(prefs::kMetricsClientID), client_id);
  EXPECT_TRUE(prefs_.FindPreference(prefs::kMetricsProvisionalClientID)
                  ->IsDefaultValue());
  EXPECT_TRUE(prefs_.GetString(prefs::kMetricsProvisionalClientID).empty());
  EXPECT_EQ(state_manager->GetLowEntropySource(), low_entropy_source);
  EXPECT_EQ(client_info_load_count_, 1);
}

TEST_F(MetricsStateManagerTest, ProvisionalClientId_PersistedAcrossFirstRuns) {
  // Force enable the creation of a provisional client ID on first run for
  // consistency between Chromium and Chrome builds.
  MetricsStateManager::enable_provisional_client_id_for_testing_ = true;

  std::string provisional_client_id;

  // Simulate a first run, and verify that a provisional client id is generated.
  // We also do not enable nor disable UMA in order to simulate exiting during
  // the first run flow.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    // Verify that there was a provisional client id created.
    provisional_client_id =
        prefs_.GetString(prefs::kMetricsProvisionalClientID);
    VerifyClientId(provisional_client_id);
    // No client id should have been stored.
    EXPECT_TRUE(
        prefs_.FindPreference(prefs::kMetricsClientID)->IsDefaultValue());
    // The default entropy provider should be the high entropy one since we have
    // a provisional client ID. |enable_limited_entropy_mode| is irrelevant but
    // is set to true for test coverage.
    state_manager->CreateEntropyProviders(
        /*enable_limited_entropy_mode=*/true);
    EXPECT_EQ(state_manager->entropy_source_returned(),
              MetricsStateManager::ENTROPY_SOURCE_HIGH);
    // The high entropy source used should be the provisional client ID.
    EXPECT_EQ(state_manager->initial_client_id_for_testing(),
              provisional_client_id);
  }

  // Now, simulate a second run, and verify that the provisional client ID is
  // the same.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    // Verify that the same provisional client ID as the first run is used.
    EXPECT_EQ(provisional_client_id,
              prefs_.GetString(prefs::kMetricsProvisionalClientID));
    // There still should not be any stored client ID.
    EXPECT_TRUE(
        prefs_.FindPreference(prefs::kMetricsClientID)->IsDefaultValue());
    // The default entropy provider should be the high entropy one since we have
    // a provisional client ID. |enable_limited_entropy_mode| is irrelevant but
    // is set to true for test coverage.
    state_manager->CreateEntropyProviders(
        /*enable_limited_entropy_mode=*/true);
    EXPECT_EQ(state_manager->entropy_source_returned(),
              MetricsStateManager::ENTROPY_SOURCE_HIGH);
    // The high entropy source used should be the provisional client ID.
    EXPECT_EQ(state_manager->initial_client_id_for_testing(),
              provisional_client_id);
  }
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_F(MetricsStateManagerTest, LoadPrefs) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  EnableMetricsReporting();
  {
    EXPECT_FALSE(fake_client_info_backup_);
    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id should be auto-obtained from the constructor when metrics
    // reporting is enabled.
    EXPECT_EQ(state_manager->client_id(), client_info.client_id);

    // The backup should not be modified.
    ASSERT_FALSE(stored_client_info_backup_);

    // Re-forcing client id creation shouldn't cause another backup and
    // shouldn't affect the existing client id.
    state_manager->ForceClientIdCreation();
    EXPECT_FALSE(stored_client_info_backup_);
    EXPECT_EQ(state_manager->client_id(), client_info.client_id);
    EXPECT_EQ(client_info_load_count_, 0);
  }
}

TEST_F(MetricsStateManagerTest, PreferPrefs) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  ClientInfo client_info2;
  client_info2.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info2.installation_date = 1111;
  client_info2.reporting_enabled_date = 2222;
  SetFakeClientInfoBackup(client_info2);

  EnableMetricsReporting();
  {
    // The backup should be ignored if we already have a client id.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(state_manager->client_id(), client_info.client_id);

    // The backup should not be modified.
    ASSERT_FALSE(stored_client_info_backup_);
    EXPECT_EQ(client_info_load_count_, 0);
  }
}

TEST_F(MetricsStateManagerTest, RestoreBackup) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  ClientInfo client_info2;
  client_info2.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info2.installation_date = 1111;
  client_info2.reporting_enabled_date = 2222;
  SetFakeClientInfoBackup(client_info2);

  prefs_.ClearPref(prefs::kMetricsClientID);
  prefs_.ClearPref(prefs::kMetricsReportingEnabledTimestamp);

  EnableMetricsReporting();
  {
    // The backup should kick in if the client id has gone missing. It should
    // replace remaining and missing dates as well.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(state_manager->client_id(), client_info2.client_id);
    EXPECT_EQ(prefs_.GetInt64(prefs::kInstallDate),
              client_info2.installation_date);
    EXPECT_EQ(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              client_info2.reporting_enabled_date);

    EXPECT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(client_info_load_count_, 1);
  }
}

TEST_F(MetricsStateManagerTest, ResetBackup) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = 1111;
  client_info.reporting_enabled_date = 2222;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  EnableMetricsReporting();
  {
    // Upon request to reset metrics ids, the existing backup should not be
    // restored.

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // A brand new client id should have been generated.
    EXPECT_NE(state_manager->client_id(), std::string());
    EXPECT_NE(state_manager->client_id(), client_info.client_id);
    EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
    EXPECT_EQ(state_manager->previous_client_id_, client_info.client_id);
    EXPECT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(client_info_load_count_, 0);

    // The installation date should not have been affected.
    EXPECT_EQ(prefs_.GetInt64(prefs::kInstallDate),
              client_info.installation_date);

    // The metrics-reporting-enabled date will be reset to Now().
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time_);
  }
}

TEST_F(MetricsStateManagerTest, CheckProvider) {
  int64_t kInstallDate = 1373051956;
  int64_t kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
  int64_t kEnabledDate = 1373001211;
  int64_t kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.

  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = kInstallDate;
  client_info.reporting_enabled_date = kEnabledDate;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(system_profile.install_date(), kInstallDateExpected);
  EXPECT_EQ(system_profile.uma_enabled_date(), kEnabledDateExpected);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  provider->ProvidePreviousSessionData(&uma_proto);
  // The client_id field in the proto should not be overwritten.
  EXPECT_FALSE(uma_proto.has_client_id());
  // Nothing should have been emitted to the cloned install histogram.
  histogram_tester.ExpectTotalCount("UMA.IsClonedInstall", 0);
}

TEST_F(MetricsStateManagerTest, CheckProviderLogNormal) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  // Set the random seed to have a deterministic test.
  std::unique_ptr<MetricsProvider> provider =
      state_manager->GetProviderAndSetRandomSeedForTesting(42);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  provider->ProvideCurrentSessionData(&uma_proto);
  histogram_tester.ExpectUniqueSample("UMA.DataValidation.LogNormal", 189, 1);
}

TEST_F(MetricsStateManagerTest, CheckProviderLogNormalWithParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kNonUniformityValidationFeature, {{"delta", "10.0"}});
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  // Set the random seed to have a deterministic test.
  std::unique_ptr<MetricsProvider> provider =
      state_manager->GetProviderAndSetRandomSeedForTesting(42);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  provider->ProvideCurrentSessionData(&uma_proto);
  histogram_tester.ExpectUniqueSample("UMA.DataValidation.LogNormal", 2081, 1);
}

TEST_F(MetricsStateManagerTest, CheckClientIdWasNotUsedToAssignFieldTrial) {
  EnableMetricsReporting();
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = 1373051956;
  client_info.reporting_enabled_date = 1373001211;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  // The client_id in the new log doesn't match the initial_client_id we used to
  // assign field trials.
  prefs_.SetString(prefs::kMetricsClientID, "New client id");
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_TRUE(system_profile.has_client_id_was_used_for_trial_assignment());
  EXPECT_FALSE(system_profile.client_id_was_used_for_trial_assignment());
}

TEST_F(MetricsStateManagerTest, CheckClientIdWasUsedToAssignFieldTrial) {
  EnableMetricsReporting();
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = 1373051956;
  client_info.reporting_enabled_date = 1373001211;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_TRUE(system_profile.client_id_was_used_for_trial_assignment());
}

TEST_F(MetricsStateManagerTest, CheckProviderResetIds) {
  int64_t kInstallDate = 1373001211;
  int64_t kInstallDateExpected = 1373000400;  // Computed from kInstallDate.
  int64_t kEnabledDate = 1373051956;
  int64_t kEnabledDateExpected = 1373050800;  // Computed from kEnabledDate.

  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = kInstallDate;
  client_info.reporting_enabled_date = kEnabledDate;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  // Verify that MetricsStateManager has the a new client_id after reset and has
  // the right previous_client_id (equals to the client_id before being reset).
  EXPECT_NE(state_manager->client_id(), client_info.client_id);
  EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
  EXPECT_EQ(state_manager->previous_client_id_, client_info.client_id);
  EXPECT_EQ(client_info_load_count_, 0);

  uint64_t hashed_previous_client_id =
      MetricsLog::Hash(state_manager->previous_client_id_);
  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(system_profile.install_date(), kInstallDateExpected);
  EXPECT_EQ(system_profile.uma_enabled_date(), kEnabledDateExpected);
  auto cloned_install_info = system_profile.cloned_install_info();
  EXPECT_EQ(cloned_install_info.count(), 1);
  EXPECT_EQ(cloned_install_info.cloned_from_client_id(),
            hashed_previous_client_id);
  // Make sure the first_timestamp is updated and is the same as the
  // last_timestamp.
  EXPECT_EQ(cloned_install_info.last_timestamp(),
            cloned_install_info.first_timestamp());
  EXPECT_NE(cloned_install_info.first_timestamp(), 0);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  // The system_profile in the |uma_proto| is provided in
  // https://source.chromium.org/chromium/chromium/src/+/main:components/metrics/metrics_service.cc;drc=4b86ff6c58f5651a4e2f44abb22d93c3593155cb;l=759
  // and it's hard to be tested here. For logs from the previous session:
  // 1. if the previous session is the detection session, the
  // |uma_proto.system_profile| won't contain the latest cloned_install_info
  // message.
  // 2. if the previous session is a normal session, the
  // |uma_proto.system_profile| should contain the cloned_install_info message
  // as long as it's saved in prefs.
  provider->ProvidePreviousSessionData(&uma_proto);
  EXPECT_EQ(uma_proto.client_id(), hashed_previous_client_id);
  histogram_tester.ExpectUniqueSample("UMA.IsClonedInstall", 1, 1);

  // Since we set the pref and didn't call SaveMachineId(), this should do
  // nothing
  provider->ProvideCurrentSessionData(&uma_proto);
  histogram_tester.ExpectUniqueSample("UMA.IsClonedInstall", 1, 1);

  // Set the pref through SaveMachineId and expect previous to do nothing and
  // current to log the histogram
  prefs_.SetInteger(prefs::kMetricsMachineId, 2216820);
  state_manager->cloned_install_detector_.SaveMachineId(&prefs_, "test");
  provider->ProvideCurrentSessionData(&uma_proto);
  histogram_tester.ExpectUniqueSample("UMA.IsClonedInstall", 1, 2);
}

TEST_F(MetricsStateManagerTest,
       CheckProviderResetIds_PreviousIdOnlyReportInResetSession) {
  int64_t kInstallDate = 1373001211;
  int64_t kInstallDateExpected = 1373000400;  // Computed from kInstallDate.
  int64_t kEnabledDate = 1373051956;
  int64_t kEnabledDateExpected = 1373050800;  // Computed from kEnabledDate.

  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = kInstallDate;
  client_info.reporting_enabled_date = kEnabledDate;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  // In the reset session:
  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_NE(state_manager->client_id(), client_info.client_id);
    EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
    // Verify that MetricsStateManager has the right previous_client_id (the ID
    // that was there before being reset).
    EXPECT_EQ(state_manager->previous_client_id_, client_info.client_id);
    EXPECT_EQ(client_info_load_count_, 0);

    std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
    SystemProfileProto system_profile;
    provider->ProvideSystemProfileMetrics(&system_profile);
    EXPECT_EQ(system_profile.install_date(), kInstallDateExpected);
    EXPECT_EQ(system_profile.uma_enabled_date(), kEnabledDateExpected);
    auto cloned_install_info = system_profile.cloned_install_info();
    // |cloned_from_client_id| should be uploaded in the reset session.
    EXPECT_EQ(cloned_install_info.cloned_from_client_id(),
              MetricsLog::Hash(state_manager->previous_client_id_));
    // Make sure the first_timestamp is updated and is the same as the
    // last_timestamp.
    EXPECT_EQ(cloned_install_info.count(), 1);
    EXPECT_EQ(cloned_install_info.last_timestamp(),
              cloned_install_info.first_timestamp());
    EXPECT_NE(cloned_install_info.last_timestamp(), 0);
  }
  // In the normal session:
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_FALSE(state_manager->metrics_ids_were_reset_);
    std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
    SystemProfileProto system_profile;
    provider->ProvideSystemProfileMetrics(&system_profile);

    auto cloned_install_info = system_profile.cloned_install_info();
    // |cloned_from_client_id| shouldn't be reported in the normal session.
    EXPECT_FALSE(cloned_install_info.has_cloned_from_client_id());
    // Other cloned_install_info fields should continue be reported once set.
    EXPECT_EQ(cloned_install_info.count(), 1);
    EXPECT_EQ(cloned_install_info.last_timestamp(),
              cloned_install_info.first_timestamp());
    EXPECT_NE(cloned_install_info.last_timestamp(), 0);
  }
}

TEST_F(MetricsStateManagerTest, UseExternalClientId) {
  base::HistogramTester histogram_tester;
  std::string external_client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  std::unique_ptr<MetricsStateManager> state_manager(
      CreateStateManager(external_client_id));
  EnableMetricsReporting();
  state_manager->ForceClientIdCreation();
  EXPECT_EQ(external_client_id, state_manager->client_id());
  histogram_tester.ExpectUniqueSample("UMA.ClientIdSource", 5, 1);
}

}  // namespace metrics
