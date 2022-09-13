// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_SERVICE_TEST_UTIL_H_
#define COMPONENTS_DRIVE_SERVICE_TEST_UTIL_H_

#include <string>

namespace drive {

class FakeDriveService;

namespace test_util {

bool SetUpTestEntries(FakeDriveService* drive_service);

bool SetUpTeamDriveTestEntries(FakeDriveService* drive_service,
                               const std::string& team_drive_id,
                               const std::string& team_drive_name);

bool SetUpTestEntries(FakeDriveService* drive_service,
                      const std::string& parent_resource_id);

}  // namespace test_util
}  // namespace drive

#endif  // COMPONENTS_DRIVE_SERVICE_TEST_UTIL_H_
