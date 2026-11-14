// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/persisted_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/test_activity_data_service.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TEST(PersistedDataTest, Simple) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      nullptr);
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid.withdot"));
  EXPECT_EQ("", metadata->GetInstallId("someappid"));
  metadata->SetInstallId("someappid", "installation 1");
  EXPECT_EQ("installation 1", metadata->GetInstallId("someappid"));
  std::vector<std::string> items;
  items.push_back("someappid");
  items.push_back("someappid.withdot");
  test::SetDateLastData(metadata.get(), items, 3383);
  EXPECT_EQ("installation 1", metadata->GetInstallId("someappid"));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid.withdot"));
  EXPECT_EQ(3383, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(3383, metadata->GetInstallDate("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someotherappid"));
  const std::string pf1 = metadata->GetPingFreshness("someappid");
  EXPECT_FALSE(pf1.empty());
  test::SetDateLastData(metadata.get(), items, 3386);
  EXPECT_EQ(3386, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(3383, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someotherappid"));

  EXPECT_EQ(-2, metadata->GetInstallDate("someotherappid"));
  metadata->SetInstallDate("someotherappid", 3386);
  EXPECT_EQ(3386, metadata->GetInstallDate("someotherappid"));

  const std::string pf2 = metadata->GetPingFreshness("someappid");
  EXPECT_FALSE(pf2.empty());
  // The following has a 1 / 2^128 chance of being flaky.
  EXPECT_NE(pf1, pf2);

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_EQ(base::Version("1.0"), metadata->GetProductVersion("someappid"));

  EXPECT_FALSE(metadata->GetMaxPreviousProductVersion("someappid").IsValid());
  metadata->SetMaxPreviousProductVersion("someappid", base::Version("1.0"));
  EXPECT_EQ(base::Version("1.0"),
            metadata->GetMaxPreviousProductVersion("someappid"));
  metadata->SetMaxPreviousProductVersion("someappid", base::Version("2.0"));
  EXPECT_EQ(base::Version("2.0"),
            metadata->GetMaxPreviousProductVersion("someappid"));
  metadata->SetMaxPreviousProductVersion("someappid", base::Version("1.5"));
  EXPECT_EQ(base::Version("2.0"),
            metadata->GetMaxPreviousProductVersion("someappid"));

  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  metadata->SetFingerprint("someappid", "somefingerprint");
  EXPECT_EQ("somefingerprint", metadata->GetFingerprint("someappid"));
}

TEST(PersistedDataTest, MixedCase) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      nullptr);
  std::vector<std::string> items;
  items.push_back("someappid");
  items.push_back("someAPPid.withdot");
  test::SetDateLastData(metadata.get(), items, 3383);
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid.withdot"));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("SOMEappid"));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappID.withDOT"));
}

TEST(PersistedDataTest, NullPtrPrefService) {
  base::test::TaskEnvironment env;
  auto metadata = CreatePersistedData(
      base::BindRepeating([]() -> PrefService* { return nullptr; }), nullptr);
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid.withdot"));
  std::vector<std::string> items;
  items.push_back("someappid");
  items.push_back("someappid.withdot");
  test::SetDateLastData(metadata.get(), items, 3383);
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid.withdot"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someotherappid"));
  const std::string pf1 = metadata->GetPingFreshness("someappid");
  EXPECT_TRUE(pf1.empty());
  test::SetDateLastData(metadata.get(), items, 3386);
  metadata->SetInstallDate("someappid", 3386);
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someotherappid"));

  const std::string pf2 = metadata->GetPingFreshness("someappid");
  EXPECT_TRUE(pf2.empty());
  EXPECT_TRUE(metadata->GetPingFreshnessWriteTime("someappid").is_null());
  metadata->RotatePingFreshness("someappid");
  EXPECT_TRUE(metadata->GetPingFreshness("someappid").empty());

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());

  EXPECT_FALSE(metadata->GetMaxPreviousProductVersion("someappid").IsValid());
  metadata->SetMaxPreviousProductVersion("someappid", base::Version("1.0"));
  EXPECT_FALSE(metadata->GetMaxPreviousProductVersion("someappid").IsValid());

  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  metadata->SetFingerprint("someappid", "somefingerprint");
  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
}

TEST(PersistedDataTest, SharedPref) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      nullptr);
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someappid"));
  std::vector<std::string> items;
  items.push_back("someappid");
  test::SetDateLastData(metadata.get(), items, 3383);

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      nullptr);
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someappid"));
  EXPECT_EQ(3383, metadata->GetInstallDate("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall("someotherappid"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("someotherappid"));
  EXPECT_EQ(-2, metadata->GetInstallDate("someotherappid"));
}

TEST(PersistedDataTest, SimpleCohort) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      nullptr);
  EXPECT_EQ("", metadata->GetCohort("someappid"));
  EXPECT_EQ("", metadata->GetCohort("someotherappid"));
  EXPECT_EQ("", metadata->GetCohortHint("someappid"));
  EXPECT_EQ("", metadata->GetCohortHint("someotherappid"));
  EXPECT_EQ("", metadata->GetCohortName("someappid"));
  EXPECT_EQ("", metadata->GetCohortName("someotherappid"));
  metadata->SetCohort("someappid", "c1");
  metadata->SetCohort("someotherappid", "c2");
  metadata->SetCohortHint("someappid", "ch1");
  metadata->SetCohortHint("someotherappid", "ch2");
  metadata->SetCohortName("someappid", "cn1");
  metadata->SetCohortName("someotherappid", "cn2");
  EXPECT_EQ("c1", metadata->GetCohort("someappid"));
  EXPECT_EQ("c2", metadata->GetCohort("someotherappid"));
  EXPECT_EQ("ch1", metadata->GetCohortHint("someappid"));
  EXPECT_EQ("ch2", metadata->GetCohortHint("someotherappid"));
  EXPECT_EQ("cn1", metadata->GetCohortName("someappid"));
  EXPECT_EQ("cn2", metadata->GetCohortName("someotherappid"));
  metadata->SetCohort("someappid", "oc1");
  metadata->SetCohort("someotherappid", "oc2");
  metadata->SetCohortHint("someappid", "och1");
  metadata->SetCohortHint("someotherappid", "och2");
  metadata->SetCohortName("someappid", "ocn1");
  metadata->SetCohortName("someotherappid", "ocn2");
  EXPECT_EQ("oc1", metadata->GetCohort("someappid"));
  EXPECT_EQ("oc2", metadata->GetCohort("someotherappid"));
  EXPECT_EQ("och1", metadata->GetCohortHint("someappid"));
  EXPECT_EQ("och2", metadata->GetCohortHint("someotherappid"));
  EXPECT_EQ("ocn1", metadata->GetCohortName("someappid"));
  EXPECT_EQ("ocn2", metadata->GetCohortName("someotherappid"));
}

TEST(PersistedDataTest, ActivityData) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  auto activity_service_unique = std::make_unique<TestActivityDataService>();
  TestActivityDataService* activity_service = activity_service_unique.get();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      std::move(activity_service_unique));

  std::vector<std::string> items({"id1", "id2", "id3"});

  for (const auto& item : items) {
    EXPECT_EQ(-2, metadata->GetDateLastActive(item));
    EXPECT_EQ(-2, metadata->GetDateLastRollCall(item));
    EXPECT_EQ(-2, metadata->GetDaysSinceLastActive(item));
    EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall(item));
    EXPECT_EQ(-2, metadata->GetInstallDate(item));
    EXPECT_EQ(false, test::GetActiveBit(metadata.get(), item));
  }

  test::SetDateLastData(metadata.get(), items, 1234);
  for (const auto& item : items) {
    EXPECT_EQ(false, test::GetActiveBit(metadata.get(), item));
    EXPECT_EQ(-2, metadata->GetDateLastActive(item));
    EXPECT_EQ(1234, metadata->GetDateLastRollCall(item));
    EXPECT_EQ(1234, metadata->GetInstallDate(item));
    EXPECT_EQ(-2, metadata->GetDaysSinceLastActive(item));
    EXPECT_EQ(-2, metadata->GetDaysSinceLastRollCall(item));
  }

  activity_service->SetActiveBit("id1", true);
  activity_service->SetDaysSinceLastActive("id1", 3);
  activity_service->SetDaysSinceLastRollCall("id1", 2);
  activity_service->SetDaysSinceLastRollCall("id2", 3);
  activity_service->SetDaysSinceLastRollCall("id3", 4);
  EXPECT_EQ(true, test::GetActiveBit(metadata.get(), "id1"));
  EXPECT_EQ(false, test::GetActiveBit(metadata.get(), "id2"));
  EXPECT_EQ(false, test::GetActiveBit(metadata.get(), "id2"));
  EXPECT_EQ(3, metadata->GetDaysSinceLastActive("id1"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("id2"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("id3"));
  EXPECT_EQ(2, metadata->GetDaysSinceLastRollCall("id1"));
  EXPECT_EQ(3, metadata->GetDaysSinceLastRollCall("id2"));
  EXPECT_EQ(4, metadata->GetDaysSinceLastRollCall("id3"));

  metadata->SetInstallId("id2", "iid2");
  test::SetDateLastData(metadata.get(), items, 5678);
  EXPECT_EQ("iid2", metadata->GetInstallId("id2"));
  activity_service->SetActiveBit("id2", true);
  test::SetDateLastData(metadata.get(), items, 6789);
  EXPECT_EQ("", metadata->GetInstallId("id2"));
  EXPECT_EQ(false, test::GetActiveBit(metadata.get(), "id1"));
  EXPECT_EQ(false, test::GetActiveBit(metadata.get(), "id2"));
  EXPECT_EQ(false, test::GetActiveBit(metadata.get(), "id3"));
  EXPECT_EQ(3, metadata->GetDaysSinceLastActive("id1"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("id2"));
  EXPECT_EQ(-2, metadata->GetDaysSinceLastActive("id3"));
  EXPECT_EQ(2, metadata->GetDaysSinceLastRollCall("id1"));
  EXPECT_EQ(3, metadata->GetDaysSinceLastRollCall("id2"));
  EXPECT_EQ(4, metadata->GetDaysSinceLastRollCall("id3"));
  EXPECT_EQ(5678, metadata->GetDateLastActive("id1"));
  EXPECT_EQ(6789, metadata->GetDateLastActive("id2"));
  EXPECT_EQ(-2, metadata->GetDateLastActive("id3"));
  EXPECT_EQ(6789, metadata->GetDateLastRollCall("id1"));
  EXPECT_EQ(6789, metadata->GetDateLastRollCall("id2"));
  EXPECT_EQ(6789, metadata->GetDateLastRollCall("id3"));
  EXPECT_EQ(1234, metadata->GetInstallDate("id1"));
  EXPECT_EQ(1234, metadata->GetInstallDate("id2"));
  EXPECT_EQ(1234, metadata->GetInstallDate("id3"));
  test::SetDateLastData(metadata.get(), items, 6790);
}

TEST(PersistedDataTest, LastUpdateCheckError) {
  base::test::TaskEnvironment env;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref.get()),
      std::make_unique<TestActivityDataService>());

  metadata->SetLastUpdateCheckError(
      {.category = ErrorCategory::kDownload, .code = 5, .extra = 10});
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorCategoryPreference), 1);
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorPreference), 5);
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorExtraCode1Preference), 10);

  metadata->SetLastUpdateCheckError(
      {.category = ErrorCategory::kNone, .code = 0, .extra = 0});
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorCategoryPreference), 0);
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorPreference), 0);
  EXPECT_EQ(pref->GetInteger(kLastUpdateCheckErrorExtraCode1Preference), 0);
}

class PersistedDataPingFreshnessTest : public testing::Test {
 protected:
  PersistedDataPingFreshnessTest() {
    RegisterPersistedDataPrefs(pref_service_.registry());
    metadata_ = CreatePersistedData(
        base::BindRepeating([](PrefService* pref) { return pref; },
                            &pref_service_),
        nullptr);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PersistedData> metadata_;
};

TEST_F(PersistedDataPingFreshnessTest, WriteTime) {
  EXPECT_TRUE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());

  // SetDateLastData() writes a PingFreshness and its write time for each app.
  const base::Time first_write_time = base::Time::Now();
  test::SetDateLastData(metadata_.get(), {"someappid", "someotherappid"}, 3383);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someappid"),
            first_write_time);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someotherappid"),
            first_write_time);

  task_environment_.FastForwardBy(base::Hours(1));
  test::SetDateLastData(metadata_.get(), {"someappid"}, 3384);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someappid"),
            base::Time::Now());
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someotherappid"),
            first_write_time);
}

TEST_F(PersistedDataPingFreshnessTest, CorruptedWriteTime) {
  test::SetDateLastData(metadata_.get(), {"someappid"}, 3383);
  ASSERT_FALSE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());
  {
    ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
    base::DictValue* app = update->FindDictByDottedPath("apps.someappid");
    ASSERT_TRUE(app);
    app->Set("pf_time", "not a time");
  }
  EXPECT_TRUE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());
  EXPECT_FALSE(metadata_->GetPingFreshness("someappid").empty());
}

TEST_F(PersistedDataPingFreshnessTest, Rotate) {
  // There is nothing to rotate until a PingFreshness is written. Rotation
  // doesn't create an entry for the app either.
  metadata_->RotatePingFreshness("someappid");
  EXPECT_TRUE(metadata_->GetPingFreshness("someappid").empty());
  EXPECT_TRUE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());
  EXPECT_EQ(metadata_->GetInstallDate("someappid"), -2);

  test::SetDateLastData(metadata_.get(), {"someappid", "someotherappid"}, 3383);
  const std::string pf1 = metadata_->GetPingFreshness("someappid");
  ASSERT_FALSE(pf1.empty());
  const std::string other_pf = metadata_->GetPingFreshness("someotherappid");
  ASSERT_FALSE(other_pf.empty());
  const base::Time other_write_time =
      metadata_->GetPingFreshnessWriteTime("someotherappid");

  // Rotation replaces even a fresh PingFreshness.
  task_environment_.FastForwardBy(base::Hours(1));
  metadata_->RotatePingFreshness("someappid");
  const std::string pf2 = metadata_->GetPingFreshness("someappid");
  EXPECT_FALSE(pf2.empty());
  EXPECT_NE(pf2, pf1);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someappid"),
            base::Time::Now());

  // Rotation leaves the dates and other apps unchanged.
  EXPECT_EQ(metadata_->GetDateLastRollCall("someappid"), 3383);
  EXPECT_EQ(metadata_->GetInstallDate("someappid"), 3383);
  EXPECT_EQ(metadata_->GetPingFreshness("someotherappid"), other_pf);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someotherappid"),
            other_write_time);

  // Rotating a PingFreshness without a write time gives it one.
  SetPingFreshnessWriteTimeForTesting(pref_service_, "someappid", std::nullopt);
  task_environment_.FastForwardBy(base::Hours(1));
  metadata_->RotatePingFreshness("someappid");
  EXPECT_NE(metadata_->GetPingFreshness("someappid"), pf2);
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someappid"),
            base::Time::Now());
}

TEST_F(PersistedDataPingFreshnessTest, AppDataWithoutPingFreshness) {
  // The app has data, but no update check has succeeded yet.
  metadata_->SetCohort("someappid", "c1");
  EXPECT_TRUE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid", base::Time::Now()),
            PingFreshnessState::kNone);

  metadata_->RotatePingFreshness("someappid");
  EXPECT_TRUE(metadata_->GetPingFreshness("someappid").empty());
  EXPECT_TRUE(metadata_->GetPingFreshnessWriteTime("someappid").is_null());
}

TEST_F(PersistedDataPingFreshnessTest, State) {
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid", base::Time::Now()),
            PingFreshnessState::kNone);

  test::SetDateLastData(metadata_.get(), {"someappid"}, 3383);
  const base::Time write_time =
      metadata_->GetPingFreshnessWriteTime("someappid");
  ASSERT_FALSE(write_time.is_null());
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid", write_time),
            PingFreshnessState::kFresh);
  EXPECT_EQ(GetPingFreshnessState(
                *metadata_, "someappid",
                write_time + kPingFreshnessMaxAge - base::Seconds(1)),
            PingFreshnessState::kFresh);
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid",
                                  write_time + kPingFreshnessMaxAge),
            PingFreshnessState::kExpired);
}

TEST_F(PersistedDataPingFreshnessTest, StateOfUnknownOrFutureWriteTime) {
  test::SetDateLastData(metadata_.get(), {"someappid"}, 3383);
  const base::Time now = base::Time::Now();

  // Older versions don't record the write time.
  SetPingFreshnessWriteTimeForTesting(pref_service_, "someappid", std::nullopt);
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid", now),
            PingFreshnessState::kUnknownWriteTime);

  // The clock was set back after the PingFreshness was written.
  SetPingFreshnessWriteTimeForTesting(pref_service_, "someappid",
                                      now + base::Minutes(1));
  EXPECT_EQ(metadata_->GetPingFreshnessWriteTime("someappid"),
            now + base::Minutes(1));
  EXPECT_EQ(GetPingFreshnessState(*metadata_, "someappid", now),
            PingFreshnessState::kFutureWriteTime);
}

}  // namespace update_client
