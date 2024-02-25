// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TEST_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TEST_UTILS_H_

#include "base/files/file_path.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"

namespace ash::report::utils {

// Number of test cases exist in the preserved file test data.
static const int kPreservedFileTestCaseSize = 9;

// This fake high entropy seed is used for testing purposes.
static constexpr char kFakeHighEntropySeed[] =
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

// Used to initialize clock time to midnight GMT for unit tests.
// Use cases are passed a GMT timestamp that is already adjusted to PST (GMT-8).
// Report controller is passed the unadjusted GMT timestamp for a new PST day.
static const char kFakeTimeNowString[] = "2023-01-01 00:00:00 GMT";
static const char kFakeTimeNowUnadjustedString[] = "2023-01-01 08:00:00 GMT";

// Fake UTC-based device activation date, formatted as YYYY-WW for privacy.
// https://crsrc.org/o/src/third_party/chromiumos-overlay/chromeos-base/chromeos-activate-date/files/activate_date;l=67
static const char kFakeFirstActivateDate[] = "2022-50";

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto);

private_computing::PrivateComputingClientRegressionTestData::TestCase
GetPreservedFileTestCase(
    private_computing::PrivateComputingClientRegressionTestData* test_data,
    private_computing::PrivateComputingClientRegressionTestData::TestName
        test_name);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TEST_UTILS_H_
