// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/dbus/biod/test_utils.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

using biod::ERROR_UNABLE_TO_PROCESS;
using biod::SCAN_RESULT_SUCCESS;

namespace ash {

namespace {

const char kTestUserId[] = "testuser@gmail.com";
const char kTestLabel[] = "testLabel";
// Template of a scan string to be used in GenerateTestFingerprint. The # and $
// are two wildcards that will be replaced by numbers to ensure unique scans.
const char kTestScan[] = "finger#scan$";

}  // namespace

class FakeBiodClientTest : public testing::Test {
 public:
  FakeBiodClientTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {}

  FakeBiodClientTest(const FakeBiodClientTest&) = delete;
  FakeBiodClientTest& operator=(const FakeBiodClientTest&) = delete;

  ~FakeBiodClientTest() override = default;

  // Returns the stored records for |user_id|. Verified to work in
  // TestGetRecordsForUser.
  std::vector<dbus::ObjectPath> GetRecordsForUser(const std::string& user_id) {
    std::vector<dbus::ObjectPath> enrollment_paths;
    bool enrollment_success = false;
    auto enrollment_callback =
        [](std::vector<dbus::ObjectPath>& enrollment_paths,
           bool& enrollment_success, const std::vector<dbus::ObjectPath>& paths,
           bool success) {
          test_utils::CopyObjectPathArray(&enrollment_paths, paths);
          enrollment_success = success;
        };

    fake_biod_client_.GetRecordsForUser(
        user_id, base::BindOnce(enrollment_callback, std::ref(enrollment_paths),
                                std::ref(enrollment_success)));
    task_runner_->RunUntilIdle();
    return enrollment_paths;
  }

  // Helper function which enrolls a fingerprint. Each element in
  // |fingerprint_data| corresponds to a finger tap.
  void EnrollFingerprint(const std::string& id,
                         const std::string& label,
                         const std::vector<std::string>& fingerprint_data) {
    ASSERT_FALSE(fingerprint_data.empty());

    dbus::ObjectPath returned_path;
    fake_biod_client_.StartEnrollSession(
        id, label, base::BindOnce(&test_utils::CopyObjectPath, &returned_path));
    task_runner_->RunUntilIdle();
    EXPECT_NE(dbus::ObjectPath(), returned_path);

    // Send |fingerprint_data| size - 1 incomplete scans, then finish the
    // enrollment by sending a complete scan signal.
    for (size_t i = 0; i < fingerprint_data.size(); ++i) {
      fake_biod_client_.SendEnrollScanDone(
          fingerprint_data[i], SCAN_RESULT_SUCCESS,
          i == fingerprint_data.size() - 1 /* is_complete */,
          -1 /* percent_complete */);
    }
  }

  // Helper function which enrolls |n| fingerprints with the same |id|, |label|
  // and |fingerprint_data|.
  void EnrollNTestFingerprints(const std::string& id,
                               const std::string& label,
                               const std::vector<std::string>& fingerprint_data,
                               int n) {
    for (int i = 0; i < n; ++i)
      EnrollFingerprint(id, label, fingerprint_data);
  }

  // Creates a new unique fingerprint consisting of unique scans.
  std::vector<std::string> GenerateTestFingerprint(int scans) {
    EXPECT_GE(scans, 0);
    num_test_fingerprints_++;

    std::vector<std::string> fingerprint;
    for (int i = 0; i < scans; ++i) {
      std::string scan = kTestScan;
      base::ReplaceSubstringsAfterOffset(
          &scan, 0, "#", base::NumberToString(num_test_fingerprints_));
      base::ReplaceSubstringsAfterOffset(&scan, 0, "$",
                                         base::NumberToString(i));
      fingerprint.push_back(scan);
    }
    return fingerprint;
  }

 protected:
  FakeBiodClient fake_biod_client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;

  // This number is incremented each time GenerateTestFingerprint is called to
  // ensure each fingerprint is unique.
  int num_test_fingerprints_ = 0;
};

TEST_F(FakeBiodClientTest, TestEnrollSessionWorkflow) {
  test_utils::TestBiodObserver observer;
  fake_biod_client_.AddObserver(&observer);

  const std::vector<std::string>& kTestFingerprint = GenerateTestFingerprint(2);
  // Verify that successful enrollments get stored as expected. A fingerprint
  // that was created with 2 scans should have 1 incomplete scan and 1 complete
  // scan.
  EnrollFingerprint(kTestUserId, kTestLabel, kTestFingerprint);
  EXPECT_EQ(1u, GetRecordsForUser(kTestUserId).size());
  EXPECT_EQ(1, observer.num_incomplete_enroll_scans_received());
  EXPECT_EQ(1, observer.num_complete_enroll_scans_received());

  // Verify that the enroll session worflow can be used repeatedly by enrolling
  // 3 more fingerprints (each with 1 incomplete and 1 complete scan).
  observer.ResetAllCounts();
  EnrollNTestFingerprints(kTestUserId, kTestLabel, kTestFingerprint, 3);
  EXPECT_EQ(4u, GetRecordsForUser(kTestUserId).size());
  EXPECT_EQ(3, observer.num_incomplete_enroll_scans_received());
  EXPECT_EQ(3, observer.num_complete_enroll_scans_received());
}

// Test authentication when one user has one or more fingerprints registered.
// This should be the normal scenario.
TEST_F(FakeBiodClientTest, TestAuthSessionWorkflowSingleUser) {
  test_utils::TestBiodObserver observer;
  fake_biod_client_.AddObserver(&observer);
  EXPECT_EQ(0u, GetRecordsForUser(kTestUserId).size());

  const std::vector<std::string>& kTestFingerprint1 =
      GenerateTestFingerprint(2);
  const std::vector<std::string>& kTestFingerprint2 =
      GenerateTestFingerprint(2);
  const std::vector<std::string>& kTestFingerprint3 =
      GenerateTestFingerprint(2);
  const std::vector<std::string>& kTestFingerprint4 =
      GenerateTestFingerprint(2);

  // Add two fingerprints |kTestFingerprint1| and |kTestFingerprint2| and start
  // an auth session.
  EnrollFingerprint(kTestUserId, kTestLabel, kTestFingerprint1);
  EnrollFingerprint(kTestUserId, kTestLabel, kTestFingerprint2);
  dbus::ObjectPath returned_path;
  fake_biod_client_.StartAuthSession(
      base::BindOnce(&test_utils::CopyObjectPath, &returned_path));
  task_runner_->RunUntilIdle();
  EXPECT_NE(returned_path, dbus::ObjectPath());

  biod::FingerprintMessage msg;
  // Verify that by sending two attempt signals of fingerprints that have been
  // enrolled, the observer should receive two matches and zero non-matches.
  msg.set_scan_result(SCAN_RESULT_SUCCESS);
  fake_biod_client_.SendAuthScanDone(kTestFingerprint1[0], msg);
  fake_biod_client_.SendAuthScanDone(kTestFingerprint2[0], msg);
  EXPECT_EQ(2, observer.num_matched_auth_scans_received());
  EXPECT_EQ(0, observer.num_unmatched_auth_scans_received());

  // Verify that by sending two attempt signals of fingerprints that have not
  // been enrolled, the observer should receive two non-matches and zero
  // matches.
  observer.ResetAllCounts();
  fake_biod_client_.SendAuthScanDone(kTestFingerprint3[0], msg);
  fake_biod_client_.SendAuthScanDone(kTestFingerprint4[0], msg);
  EXPECT_EQ(0, observer.num_matched_auth_scans_received());
  EXPECT_EQ(2, observer.num_unmatched_auth_scans_received());

  // Verify that by sending two attempt signals of failure during match
  // (with enrolled finger or not), the observer should receive two
  // non-matches and zero matches.
  observer.ResetAllCounts();
  // Error and ScanResult are in oneof field, so setting Error member here
  // will automatically clear ScanResult member. For more information please
  // check https://developers.google.com/protocol-buffers/docs/proto3#oneof
  msg.set_error(ERROR_UNABLE_TO_PROCESS);
  fake_biod_client_.SendAuthScanDone(kTestFingerprint1[0], msg);
  fake_biod_client_.SendAuthScanDone(kTestFingerprint3[0], msg);
  EXPECT_EQ(0, observer.num_matched_auth_scans_received());
  EXPECT_EQ(2, observer.num_unmatched_auth_scans_received());
}

// Test authentication when multiple users have fingerprints registered. Cover
// cases such as when both users use the same labels, a user had registered the
// same fingerprint multiple times, or two users use the same fingerprint.
TEST_F(FakeBiodClientTest, TestAuthenticateWorkflowMultipleUsers) {
  test_utils::TestBiodObserver observer;
  fake_biod_client_.AddObserver(&observer);
  EXPECT_EQ(0u, GetRecordsForUser(kTestUserId).size());

  // Add two users, who have scanned three fingers between the two of them.
  const std::string kUserOne = std::string(kTestUserId) + "1";
  const std::string kUserTwo = std::string(kTestUserId) + "2";

  const std::string kLabelOne = std::string(kTestLabel) + "1";
  const std::string kLabelTwo = std::string(kTestLabel) + "2";
  const std::string kLabelThree = std::string(kTestLabel) + "3";

  // Generate 2 test fingerprints per user.
  const std::vector<std::string>& kUser1Finger1 = GenerateTestFingerprint(2);
  const std::vector<std::string>& kUser1Finger2 = GenerateTestFingerprint(2);
  const std::vector<std::string>& kUser2Finger1 = GenerateTestFingerprint(2);
  const std::vector<std::string>& kUser2Finger2 = GenerateTestFingerprint(2);

  EnrollFingerprint(kUserOne, kLabelOne, kUser1Finger1);
  EnrollFingerprint(kUserOne, kLabelTwo, kUser1Finger2);
  // User one has registered finger two twice.
  EnrollFingerprint(kUserOne, kLabelThree, kUser1Finger2);
  EnrollFingerprint(kUserTwo, kLabelOne, kUser2Finger1);
  EnrollFingerprint(kUserTwo, kLabelTwo, kUser2Finger2);
  // User two has allowed user one to unlock their account with their first
  // finger.
  EnrollFingerprint(kUserTwo, kLabelThree, kUser1Finger1);

  dbus::ObjectPath returned_path;
  fake_biod_client_.StartAuthSession(
      base::BindOnce(&test_utils::CopyObjectPath, &returned_path));
  task_runner_->RunUntilIdle();
  EXPECT_NE(returned_path, dbus::ObjectPath());

  // Verify that if a user registers the same finger to two different labels,
  // both ObjectPath that maps to the labels are returned as matches.
  std::vector<dbus::ObjectPath> record_paths_user1 =
      GetRecordsForUser(kUserOne);
  EXPECT_EQ(3u, record_paths_user1.size());

  AuthScanMatches expected_auth_scans_matches;
  biod::FingerprintMessage msg;
  expected_auth_scans_matches[kUserOne] = {record_paths_user1[1],
                                           record_paths_user1[2]};
  msg.set_scan_result(SCAN_RESULT_SUCCESS);
  fake_biod_client_.SendAuthScanDone(kUser1Finger2[0], msg);
  EXPECT_EQ(expected_auth_scans_matches, observer.last_auth_scan_matches());

  // Verify that a fingerprint associated with one user and one label returns a
  // match with one user and one ObjectPath that maps to that label.
  std::vector<dbus::ObjectPath> record_paths_user2 =
      GetRecordsForUser(kUserTwo);
  EXPECT_EQ(3u, record_paths_user2.size());

  expected_auth_scans_matches.clear();
  expected_auth_scans_matches[kUserTwo] = {record_paths_user2[0]};
  fake_biod_client_.SendAuthScanDone(kUser2Finger1[0], msg);
  EXPECT_EQ(expected_auth_scans_matches, observer.last_auth_scan_matches());

  // Verify if two users register the same fingerprint, the matches contain
  // both users.
  expected_auth_scans_matches.clear();
  expected_auth_scans_matches[kUserOne] = {record_paths_user1[0]};
  expected_auth_scans_matches[kUserTwo] = {record_paths_user2[2]};
  fake_biod_client_.SendAuthScanDone(kUser1Finger1[0], msg);
  EXPECT_EQ(expected_auth_scans_matches, observer.last_auth_scan_matches());

  // Verify if a unregistered finger is scanned, the matches are empty.
  expected_auth_scans_matches.clear();
  fake_biod_client_.SendAuthScanDone("Unregistered", msg);
  EXPECT_EQ(expected_auth_scans_matches, observer.last_auth_scan_matches());

  // Verify if error is returned, the matches are empty.
  // Error and ScanResult are in oneof field, so setting Error member here
  // will automatically clear ScanResult member. For more information please
  // check https://developers.google.com/protocol-buffers/docs/proto3#oneof
  msg.set_error(ERROR_UNABLE_TO_PROCESS);
  fake_biod_client_.SendAuthScanDone(kUser1Finger1[0], msg);
  EXPECT_EQ(expected_auth_scans_matches, observer.last_auth_scan_matches());
}

TEST_F(FakeBiodClientTest, TestGetRecordsForUser) {
  // Verify that initially |kTestUserId| will have no fingerprints.
  EXPECT_EQ(0u, GetRecordsForUser(kTestUserId).size());

  // Verify that after enrolling 2 fingerprints, a GetRecords call return 2
  // items.
  EnrollNTestFingerprints(kTestUserId, kTestLabel, GenerateTestFingerprint(2),
                          2);
  EXPECT_EQ(2u, GetRecordsForUser(kTestUserId).size());

  // Verify that GetRecords call for a user with no registered fingerprints
  // should return 0 items.
  EXPECT_EQ(0u, GetRecordsForUser("noRegisteredFingerprints@gmail.com").size());
}

TEST_F(FakeBiodClientTest, TestDestroyingRecords) {
  // Verify that after enrolling 2 fingerprints and destroying them, 0
  // fingerprints will remain.
  EnrollNTestFingerprints(kTestUserId, kTestLabel, GenerateTestFingerprint(2),
                          2);
  EXPECT_EQ(2u, GetRecordsForUser(kTestUserId).size());
  fake_biod_client_.DestroyAllRecords(base::DoNothing());
  EXPECT_EQ(0u, GetRecordsForUser(kTestUserId).size());
}

TEST_F(FakeBiodClientTest, TestGetAndSetRecordLabels) {
  const std::string kLabelOne = "Finger 1";
  const std::string kLabelTwo = "Finger 2";

  EnrollFingerprint(kTestUserId, kLabelOne, GenerateTestFingerprint(2));
  EnrollFingerprint(kTestUserId, kLabelTwo, GenerateTestFingerprint(2));
  std::vector<dbus::ObjectPath> enrollment_paths =
      GetRecordsForUser(kTestUserId);
  EXPECT_EQ(2u, enrollment_paths.size());

  // Verify the labels we get using GetLabel are the same as the one we
  // originally set.
  std::string returned_str;
  fake_biod_client_.RequestRecordLabel(
      enrollment_paths[0],
      base::BindOnce(&test_utils::CopyString, &returned_str));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(kLabelOne, returned_str);

  returned_str = "";
  fake_biod_client_.RequestRecordLabel(
      enrollment_paths[1],
      base::BindOnce(&test_utils::CopyString, &returned_str));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(kLabelTwo, returned_str);

  // Verify that by setting a new label, getting the label will return the value
  // of the new label.
  const std::string kNewLabelTwo = "Finger 2 New";
  fake_biod_client_.SetRecordLabel(enrollment_paths[1], kNewLabelTwo,
                                   base::DoNothing());
  fake_biod_client_.RequestRecordLabel(
      enrollment_paths[1],
      base::BindOnce(&test_utils::CopyString, &returned_str));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(kNewLabelTwo, returned_str);
}

}  // namespace ash
