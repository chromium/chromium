// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_TEST_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_TEST_UTILS_H_

#include <string>
#include <vector>

#include "chromeos/ash/components/dbus/biod/biod_client.h"

namespace dbus {
class ObjectPath;
}

namespace ash {
namespace test_utils {

// Copies |src_path| to |dest_path|.
void CopyObjectPath(dbus::ObjectPath* dest_path,
                    const dbus::ObjectPath& src_path);

// Copies |src_object_paths| to |dst_object_paths|.
void CopyObjectPathArray(std::vector<dbus::ObjectPath>* dest_object_paths,
                         const std::vector<dbus::ObjectPath>& src_object_paths);

// Copies |src_str| to |dest_str|.
void CopyString(std::string* dest_str, const std::string& src_str);

// Copies |src_status| to |dest_status|.
void CopyDBusMethodCallResult(bool* dest_result, bool src_result);

// Implementation of BiodClient::Observer for testing.
class TestBiodObserver : public BiodClient::Observer {
 public:
  TestBiodObserver();

  TestBiodObserver(const TestBiodObserver&) = delete;
  TestBiodObserver& operator=(const TestBiodObserver&) = delete;

  ~TestBiodObserver() override;

  int num_complete_enroll_scans_received() const {
    return num_complete_enroll_scans_received_;
  }
  int num_incomplete_enroll_scans_received() const {
    return num_incomplete_enroll_scans_received_;
  }
  int num_matched_auth_scans_received() const {
    return num_matched_auth_scans_received_;
  }
  int num_unmatched_auth_scans_received() const {
    return num_unmatched_auth_scans_received_;
  }
  int num_failures_received() const { return num_failures_received_; }
  const AuthScanMatches& last_auth_scan_matches() const {
    return last_auth_scan_matches_;
  }

  int NumEnrollScansReceived() const;
  int NumAuthScansReceived() const;

  void ResetAllCounts();

  // BiodClient::Observer:
  void BiodServiceRestarted() override;
  void BiodServiceStatusChanged(biod::BiometricsManagerStatus status) override;
  void BiodEnrollScanDoneReceived(biod::ScanResult scan_result,
                                  bool is_complete,
                                  int percent_complete) override;
  void BiodAuthScanDoneReceived(const biod::FingerprintMessage& msg,
                                const AuthScanMatches& matches) override;
  void BiodSessionFailedReceived() override;

 private:
  int num_complete_enroll_scans_received_ = 0;
  int num_incomplete_enroll_scans_received_ = 0;
  int num_matched_auth_scans_received_ = 0;
  int num_unmatched_auth_scans_received_ = 0;
  int num_failures_received_ = 0;

  // When auth scan is received, store the result.
  AuthScanMatches last_auth_scan_matches_;
};

}  // namespace test_utils
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_TEST_UTILS_H_
