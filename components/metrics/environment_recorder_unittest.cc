// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/environment_recorder.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class EnvironmentRecorderTest : public testing::Test {
 public:
  EnvironmentRecorderTest() {
    EnvironmentRecorder::RegisterPrefs(prefs_.registry());
  }

  EnvironmentRecorderTest(const EnvironmentRecorderTest&) = delete;
  EnvironmentRecorderTest& operator=(const EnvironmentRecorderTest&) = delete;

  ~EnvironmentRecorderTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(EnvironmentRecorderTest, LoadEnvironmentFromPrefs) {
  const char* kSystemProfilePref = prefs::kStabilitySavedSystemProfile;
  const char* kSystemProfileHashPref = prefs::kStabilitySavedSystemProfileHash;

  // The pref value is empty, so loading it from prefs should fail.
  {
    EnvironmentRecorder recorder(&prefs_);
    SystemProfileProto system_profile;
    EXPECT_FALSE(recorder.LoadEnvironmentFromPrefs(&system_profile));
    EXPECT_FALSE(system_profile.has_app_version());
  }

  // Do a RecordEnvironment() call and check whether the pref is recorded.
  {
    EnvironmentRecorder recorder(&prefs_);
    SystemProfileProto system_profile;
    system_profile.set_app_version("bogus version");
    std::string serialized_profile =
        recorder.SerializeAndRecordEnvironmentToPrefs(system_profile);
    EXPECT_FALSE(serialized_profile.empty());
    EXPECT_FALSE(prefs_.GetString(kSystemProfilePref).empty());
    EXPECT_FALSE(prefs_.GetString(kSystemProfileHashPref).empty());
  }

  // Load it and check that it has the right value.
  {
    EnvironmentRecorder recorder(&prefs_);
    SystemProfileProto system_profile;
    EXPECT_TRUE(recorder.LoadEnvironmentFromPrefs(&system_profile));
    EXPECT_EQ("bogus version", system_profile.app_version());
    // Ensure that the call did not clear the prefs.
    EXPECT_FALSE(prefs_.GetString(kSystemProfilePref).empty());
    EXPECT_FALSE(prefs_.GetString(kSystemProfileHashPref).empty());
  }

  // Ensure that a non-matching hash results in the pref being invalid.
  {
    // Set the hash to a bad value.
    prefs_.SetString(kSystemProfileHashPref, "deadbeef");
    EnvironmentRecorder recorder(&prefs_);
    SystemProfileProto system_profile;
    EXPECT_FALSE(recorder.LoadEnvironmentFromPrefs(&system_profile));
    EXPECT_FALSE(system_profile.has_app_version());
  }
}

}  // namespace metrics
