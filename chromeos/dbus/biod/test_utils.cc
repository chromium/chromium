// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/biod/test_utils.h"

#include "base/check.h"
#include "dbus/object_path.h"

namespace chromeos {
namespace test_utils {

void CopyObjectPath(dbus::ObjectPath* dest_path,
                    const dbus::ObjectPath& src_path) {
  CHECK(dest_path);
  *dest_path = src_path;
}

void CopyObjectPathArray(
    std::vector<dbus::ObjectPath>* dest_object_paths,
    const std::vector<dbus::ObjectPath>& src_object_paths) {
  CHECK(dest_object_paths);
  *dest_object_paths = src_object_paths;
}

void CopyString(std::string* dest_str, const std::string& src_str) {
  CHECK(dest_str);
  *dest_str = src_str;
}

void CopyDBusMethodCallResult(bool* dest_result, bool src_result) {
  CHECK(dest_result);
  *dest_result = src_result;
}

TestBiodObserver::TestBiodObserver() = default;

TestBiodObserver::~TestBiodObserver() = default;

int TestBiodObserver::NumEnrollScansReceived() const {
  return num_complete_enroll_scans_received_ +
         num_incomplete_enroll_scans_received_;
}

int TestBiodObserver::NumAuthScansReceived() const {
  return num_matched_auth_scans_received_ + num_unmatched_auth_scans_received_;
}

void TestBiodObserver::ResetAllCounts() {
  num_complete_enroll_scans_received_ = 0;
  num_incomplete_enroll_scans_received_ = 0;
  num_matched_auth_scans_received_ = 0;
  num_unmatched_auth_scans_received_ = 0;
  num_failures_received_ = 0;
}

void TestBiodObserver::BiodServiceRestarted() {}

void TestBiodObserver::BiodEnrollScanDoneReceived(biod::ScanResult scan_result,
                                                  bool is_complete,
                                                  int percent_complete) {
  is_complete ? num_complete_enroll_scans_received_++
              : num_incomplete_enroll_scans_received_++;
}

void TestBiodObserver::BiodAuthScanDoneReceived(
    biod::ScanResult scan_result,
    const AuthScanMatches& matches) {
  matches.empty() ? num_unmatched_auth_scans_received_++
                  : num_matched_auth_scans_received_++;
  last_auth_scan_matches_ = matches;
}

void TestBiodObserver::BiodSessionFailedReceived() {
  num_failures_received_++;
}

}  // namespace test_utils
}  // namespace chromeos
