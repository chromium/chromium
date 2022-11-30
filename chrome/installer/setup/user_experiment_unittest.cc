// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/user_experiment.h"

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "chrome/installer/util/experiment_storage.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class UserExperimentTest : public ::testing::TestWithParam<bool> {
 public:
  UserExperimentTest(const UserExperimentTest&) = delete;
  UserExperimentTest& operator=(const UserExperimentTest&) = delete;

 protected:
  UserExperimentTest()
      : system_level_(GetParam()),
        root_(system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER),
        install_details_(system_level_) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root_));

    // Create the ClientState key.
    base::win::RegKey key;
    ASSERT_EQ(key.Create(root_, install_static::GetClientStateKeyPath().c_str(),
                         KEY_WOW64_64KEY | KEY_SET_VALUE),
              ERROR_SUCCESS);
  }

  void SetProductVersion(const wchar_t* version) {
    SetClientsValue(google_update::kRegVersionField, version);
  }

  void SetOldProductVersion(const wchar_t* version) {
    SetClientsValue(google_update::kRegOldVersionField, version);
  }

 private:
  void SetClientsValue(const wchar_t* value_name, const wchar_t* value_data) {
    base::win::RegKey key(
        root_,
        install_static::GetClientsKeyPath(install_static::GetAppGuid()).c_str(),
        KEY_WOW64_64KEY | KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(key.WriteValue(value_name, value_data), ERROR_SUCCESS);
  }

  const bool system_level_;
  const HKEY root_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  install_static::ScopedInstallDetails install_details_;
};

TEST_P(UserExperimentTest, WriteInitialStateNoData) {
  ExperimentStorage storage;

  // A first call should write the desired state.
  WriteInitialState(&storage, ExperimentMetrics::kWaitingForUserLogon);

  ExperimentMetrics metrics = ExperimentMetrics();
  EXPECT_TRUE(storage.AcquireLock()->LoadMetrics(&metrics));
  EXPECT_EQ(metrics.state, ExperimentMetrics::kWaitingForUserLogon);

  // A subsequent should update it state.
  WriteInitialState(&storage, ExperimentMetrics::kSingletonWaitTimeout);

  metrics = ExperimentMetrics();
  EXPECT_TRUE(storage.AcquireLock()->LoadMetrics(&metrics));
  EXPECT_EQ(metrics.state, ExperimentMetrics::kSingletonWaitTimeout);
}

// Nothing should be written if the experiment is underway.
TEST_P(UserExperimentTest, WriteInitialStateInExperiment) {
  ExperimentStorage storage;

  {
    ExperimentMetrics metrics = ExperimentMetrics();
    metrics.state = ExperimentMetrics::kGroupAssigned;
    storage.AcquireLock()->StoreMetrics(metrics);
  }

  WriteInitialState(&storage, ExperimentMetrics::kSingletonWaitTimeout);

  ExperimentMetrics metrics = ExperimentMetrics();
  EXPECT_TRUE(storage.AcquireLock()->LoadMetrics(&metrics));
  EXPECT_EQ(metrics.state, ExperimentMetrics::kGroupAssigned);
}

TEST_P(UserExperimentTest, IsDomainJoined) {
  // Just make sure it doesn't crash or leak.
  IsDomainJoined();
}

TEST_P(UserExperimentTest, IsSelectedForStudyFirstCall) {
  ExperimentStorage storage;
  auto lock = storage.AcquireLock();

  // The first call will pick a study.
  bool is_selected =
      IsSelectedForStudy(lock.get(), ExperimentStorage::kStudyOne);

  // A value must have been written.
  ExperimentStorage::Study participation = ExperimentStorage::kNoStudySelected;
  ASSERT_TRUE(lock->ReadParticipation(&participation));
  EXPECT_GE(participation, ExperimentStorage::kStudyOne);
  EXPECT_LE(participation, ExperimentStorage::kStudyTwo);

  // is_selected should be set based on the value that was written.
  if (participation == ExperimentStorage::kStudyOne)
    EXPECT_TRUE(is_selected);
  else
    EXPECT_FALSE(is_selected);
}

// A user selected into study one participates in both studies.
TEST_P(UserExperimentTest, IsSelectedForStudyOne) {
  ExperimentStorage storage;
  auto lock = storage.AcquireLock();

  ASSERT_TRUE(lock->WriteParticipation(ExperimentStorage::kStudyOne));

  EXPECT_TRUE(IsSelectedForStudy(lock.get(), ExperimentStorage::kStudyOne));
  EXPECT_TRUE(IsSelectedForStudy(lock.get(), ExperimentStorage::kStudyTwo));
}

// A user selected into study two only participates in that study.
TEST_P(UserExperimentTest, IsSelectedForStudyTwo) {
  ExperimentStorage storage;
  auto lock = storage.AcquireLock();

  ASSERT_TRUE(lock->WriteParticipation(ExperimentStorage::kStudyTwo));

  EXPECT_FALSE(IsSelectedForStudy(lock.get(), ExperimentStorage::kStudyOne));
  EXPECT_TRUE(IsSelectedForStudy(lock.get(), ExperimentStorage::kStudyTwo));
}

// Ensure that group selection is within bounds.
TEST_P(UserExperimentTest, PickGroupStudyOne) {
  int group = PickGroup(ExperimentStorage::kStudyOne);
  EXPECT_GE(group, 0);
  EXPECT_LT(group, ExperimentMetrics::kNumGroups);
}

// Ensure that group selection is within bounds.
TEST_P(UserExperimentTest, PickGroupStudyTwo) {
  int group = PickGroup(ExperimentStorage::kStudyTwo);
  EXPECT_GE(group, 0);
  EXPECT_LT(group, ExperimentMetrics::kNumGroups);
}

// When there's nothing in the registry, default to false.
TEST_P(UserExperimentTest, IsUpdateRenamePendingNoRegistration) {
  EXPECT_FALSE(IsUpdateRenamePending());
}

// No update is pending if "pv" matches the current version.
TEST_P(UserExperimentTest, IsUpdateRenamePendingNo) {
  ASSERT_NO_FATAL_FAILURE(SetProductVersion(TEXT(CHROME_VERSION_STRING)));
  EXPECT_FALSE(IsUpdateRenamePending());
}

// An update is pending if an old version needs to be restarted to be the
// current.
TEST_P(UserExperimentTest, IsUpdateRenamePendingYes) {
  static constexpr wchar_t kSillyOldVersion[] = L"47.0.1.0";
  ASSERT_STRNE(kSillyOldVersion, TEXT(CHROME_VERSION_STRING));

  ASSERT_NO_FATAL_FAILURE(SetProductVersion(TEXT(CHROME_VERSION_STRING)));
  ASSERT_NO_FATAL_FAILURE(SetOldProductVersion(kSillyOldVersion));
  EXPECT_TRUE(IsUpdateRenamePending());
}

INSTANTIATE_TEST_SUITE_P(UserLevel,
                         UserExperimentTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SystemLevel,
                         UserExperimentTest,
                         ::testing::Values(true));

}  // namespace installer
