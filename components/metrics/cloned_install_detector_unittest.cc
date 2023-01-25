// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cloned_install_detector.h"

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "components/metrics/machine_id_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const std::string kTestRawId = "test";
// Hashed machine id for |kTestRawId|.
const int kTestHashedId = 2216819;

}  // namespace

// TODO(jwd): Change these test to test the full flow and histogram outputs. It
// should also remove the need to make the test a friend of
// ClonedInstallDetector.
TEST(ClonedInstallDetectorTest, SaveId) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  ClonedInstallDetector detector;
  detector.SaveMachineId(&prefs, kTestRawId);

  EXPECT_EQ(kTestHashedId, prefs.GetInteger(prefs::kMetricsMachineId));
}

TEST(ClonedInstallDetectorTest, DetectClone) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  // Save a machine id that will cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId + 1);

  ClonedInstallDetector detector;
  detector.SaveMachineId(&prefs, kTestRawId);

  EXPECT_TRUE(prefs.GetBoolean(prefs::kMetricsResetIds));
  EXPECT_TRUE(detector.ShouldResetClientIds(&prefs));
}

TEST(ClonedInstallDetectorTest, ShouldResetClientIds) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  ClonedInstallDetector detector;
  EXPECT_FALSE(detector.ShouldResetClientIds(&prefs));

  // Save a machine id that will cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId + 1);
  detector.SaveMachineId(&prefs, kTestRawId);

  // Multiple different services may call into the cloned install detector, it
  // needs to continue supporting giving the same answer more than once
  EXPECT_TRUE(detector.ShouldResetClientIds(&prefs));
  EXPECT_TRUE(detector.ShouldResetClientIds(&prefs));
}

TEST(ClonedInstallDetectorTest, ClonedInstallDetectedInCurrentSession) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  ClonedInstallDetector detector;
  EXPECT_FALSE(detector.ShouldResetClientIds(&prefs));
  EXPECT_FALSE(detector.ClonedInstallDetectedInCurrentSession());

  // Save a machine id that will cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId + 1);
  detector.SaveMachineId(&prefs, kTestRawId);

  // Ensure that the current session call returns true both before things are
  // modified by ShouldResetClientIds and after
  EXPECT_TRUE(detector.ClonedInstallDetectedInCurrentSession());
  EXPECT_TRUE(detector.ShouldResetClientIds(&prefs));
  EXPECT_TRUE(detector.ClonedInstallDetectedInCurrentSession());
}

TEST(ClonedInstallDetectorTest, ClonedInstallDetectedCallback) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  ClonedInstallDetector detector;

  // Set up a callback that will set |callback_called| to true when a cloned
  // install is detected.
  bool callback_called = false;
  base::CallbackListSubscription subscription =
      detector.AddOnClonedInstallDetectedCallback(base::BindLambdaForTesting(
          [&callback_called] { callback_called = true; }));

  // Save a machine id that will not cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId);
  detector.SaveMachineId(&prefs, kTestRawId);
  EXPECT_FALSE(detector.ClonedInstallDetectedInCurrentSession());
  EXPECT_FALSE(callback_called);

  // Save a machine id that will cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId + 1);
  detector.SaveMachineId(&prefs, kTestRawId);
  EXPECT_TRUE(detector.ClonedInstallDetectedInCurrentSession());
  EXPECT_TRUE(callback_called);

  // Verify that if a callback is added after the cloned install has been
  // detected, it is called immediately.
  bool callback_called2 = false;
  base::CallbackListSubscription subscription2 =
      detector.AddOnClonedInstallDetectedCallback(base::BindLambdaForTesting(
          [&callback_called2] { callback_called2 = true; }));
  EXPECT_TRUE(callback_called2);
}

}  // namespace metrics
