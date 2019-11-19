// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"

#include <vector>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

constexpr wchar_t kFile1[] = L"file_one";
constexpr wchar_t kFile2[] = L"file_two";

class FileRemovalStatusUpdaterTest : public ::testing::Test {
 protected:
  FileRemovalStatusUpdaterTest() {
    // Start each test with an empty map.
    FileRemovalStatusUpdater::GetInstance()->Clear();
  }

  // Convenience accessors
  FileRemovalStatusUpdater* instance_ = FileRemovalStatusUpdater::GetInstance();
  const base::FilePath file_1_ = base::FilePath(kFile1);
  const base::FilePath file_2_ = base::FilePath(kFile2);
};

}  // namespace

TEST_F(FileRemovalStatusUpdaterTest, Clear) {
  // Map should start empty.
  EXPECT_TRUE(instance_->GetAllRemovalStatuses().empty());
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_1_));
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_2_));

  instance_->UpdateRemovalStatus(file_1_, REMOVAL_STATUS_MATCHED_ONLY);

  // Only file_1_ should be in the map.
  EXPECT_EQ(1U, instance_->GetAllRemovalStatuses().size());
  EXPECT_EQ(REMOVAL_STATUS_MATCHED_ONLY, instance_->GetRemovalStatus(file_1_));
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_2_));

  instance_->Clear();

  // Map should be empty again.
  EXPECT_TRUE(instance_->GetAllRemovalStatuses().empty());
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_1_));
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_2_));
}

TEST_F(FileRemovalStatusUpdaterTest, UpdateRemovalStatus) {
  // This function uses GetRemovalStatusOfSanitizedPath to cut down on the
  // number of times it calls SanitizePath on the same paths, which is slow.
  const base::string16 file_1_sanitized = SanitizePath(file_1_);
  const base::string16 file_2_sanitized = SanitizePath(file_2_);

  // Creates a vector of all RemovalStatus enum values to improve readability
  // of loops in this test and ensure that all RemovalStatus enumerators are
  // checked.
  std::vector<RemovalStatus> all_removal_status;
  for (int i = RemovalStatus_MIN; i <= RemovalStatus_MAX; ++i) {
    // Status cannot be set to REMOVAL_STATUS_UNSPECIFIED - this is guarded by
    // an assert.
    RemovalStatus status = static_cast<RemovalStatus>(i);
    if (status != REMOVAL_STATUS_UNSPECIFIED && RemovalStatus_IsValid(i))
      all_removal_status.push_back(status);
  }

  for (RemovalStatus removal_status : all_removal_status) {
    // Repeatedly update the removal status of file_2_. file_1_'s status should
    // not be touched.
    for (RemovalStatus new_removal_status : all_removal_status) {
      SCOPED_TRACE(::testing::Message()
                   << "removal_status " << removal_status
                   << ", new_removal_status " << new_removal_status);

      // Initially, files should have removal status "unspecified".
      ASSERT_TRUE(instance_->GetAllRemovalStatuses().empty());
      ASSERT_EQ(REMOVAL_STATUS_UNSPECIFIED,
                instance_->GetRemovalStatusOfSanitizedPath(file_1_sanitized));
      ASSERT_EQ(REMOVAL_STATUS_UNSPECIFIED,
                instance_->GetRemovalStatusOfSanitizedPath(file_2_sanitized));

      // Any removal status can override "unspecified".
      instance_->UpdateRemovalStatus(file_1_, removal_status);
      instance_->UpdateRemovalStatus(file_2_, removal_status);
      EXPECT_EQ(removal_status,
                instance_->GetRemovalStatusOfSanitizedPath(file_1_sanitized));
      EXPECT_EQ(removal_status,
                instance_->GetRemovalStatusOfSanitizedPath(file_2_sanitized));

      // Tests if attempts to override removal status obey the rules specified
      // by GetRemovalStatusOfSanitizedPathOverridePermissionMap().
      instance_->UpdateRemovalStatus(file_2_, new_removal_status);
      const internal::RemovalStatusOverridePermissionMap& decisions_map =
          internal::GetRemovalStatusOverridePermissionMap();
      const bool can_override = decisions_map.find(removal_status)
                                    ->second.find(new_removal_status)
                                    ->second == internal::kOkToOverride;
      EXPECT_EQ(can_override ? new_removal_status : removal_status,
                instance_->GetRemovalStatusOfSanitizedPath(file_2_sanitized));

      // Updating file_2_ should not have touched file_1_.
      EXPECT_EQ(removal_status,
                instance_->GetRemovalStatusOfSanitizedPath(file_1_sanitized));

      // GetAllRemovalStatuses should agree with GetRemovalStatusOfSanitizedPath
      // for all files.
      FileRemovalStatusUpdater::SanitizedPathToRemovalStatusMap all_statuses =
          instance_->GetAllRemovalStatuses();
      EXPECT_EQ(2U, all_statuses.size());
      for (const auto& path_and_status : all_statuses) {
        base::string16 sanitized_path = path_and_status.first;
        FileRemovalStatusUpdater::FileRemovalStatus status =
            path_and_status.second;
        EXPECT_EQ(instance_->GetRemovalStatusOfSanitizedPath(sanitized_path),
                  status.removal_status);
      }

      // Empty the map for the next loop.
      instance_->Clear();
    }
  }
}

TEST_F(FileRemovalStatusUpdaterTest, UpdateQuarantineStatus) {
  // Creates a vector of all QuarantineStatus enum values to improve readability
  // of loops in this test and ensure that all QuarantineStatus enumerators are
  // checked.
  std::vector<QuarantineStatus> all_quarantine_status;
  for (int i = QuarantineStatus_MIN; i <= QuarantineStatus_MAX; ++i) {
    // Status cannot be set to QUARANTINE_STATUS_UNSPECIFIED - this is guarded
    // by an assert.
    QuarantineStatus status = static_cast<QuarantineStatus>(i);
    if (QuarantineStatus_IsValid(status) &&
        status != QUARANTINE_STATUS_UNSPECIFIED)
      all_quarantine_status.push_back(status);
  }

  for (QuarantineStatus status : all_quarantine_status) {
    for (QuarantineStatus new_status : all_quarantine_status) {
      // Empty the map for the next test.
      instance_->Clear();

      // Quarantine status should be |QUARANTINE_STATUS_UNSPECIFIED| if the file
      // is not in the map.
      EXPECT_EQ(QUARANTINE_STATUS_UNSPECIFIED,
                instance_->GetQuarantineStatus(file_1_));

      instance_->UpdateQuarantineStatus(file_1_, status);
      // It should always succeed to override |QUARANTINE_STATUS_UNSPECIFIED|.
      EXPECT_EQ(status, instance_->GetQuarantineStatus(file_1_));

      instance_->UpdateQuarantineStatus(file_1_, new_status);
      // It should always succeed to override the old quarantine status.
      EXPECT_EQ(new_status, instance_->GetQuarantineStatus(file_1_));
    }
  }
}

TEST_F(FileRemovalStatusUpdaterTest, MixedRemovalQuarantineUpdate) {
  instance_->UpdateRemovalStatus(file_1_, REMOVAL_STATUS_REMOVED);
  // The initial quarantine status should be |QUARANTINE_STATUS_UNSPECIFIED|.
  EXPECT_EQ(QUARANTINE_STATUS_UNSPECIFIED,
            instance_->GetQuarantineStatus(file_1_));

  instance_->UpdateQuarantineStatus(file_1_, QUARANTINE_STATUS_QUARANTINED);
  // |UpdateQuarantineStatus| should not change removal status.
  EXPECT_EQ(REMOVAL_STATUS_REMOVED, instance_->GetRemovalStatus(file_1_));

  instance_->UpdateQuarantineStatus(file_2_, QUARANTINE_STATUS_SKIPPED);
  // The initial removal status should be |REMOVAL_STATUS_UNSPECIFIED|.
  EXPECT_EQ(REMOVAL_STATUS_UNSPECIFIED, instance_->GetRemovalStatus(file_2_));

  instance_->UpdateRemovalStatus(file_2_, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  // |UpdateRemovalStatus| should not change quarantine status.
  EXPECT_EQ(QUARANTINE_STATUS_SKIPPED, instance_->GetQuarantineStatus(file_2_));
}

TEST_F(FileRemovalStatusUpdaterTest, PathSanitization) {
  base::FilePath home_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_HOME, &home_dir));
  base::FilePath path = home_dir.Append(L"UPPER_CASE_FILENAME");

  instance_->UpdateRemovalStatus(path, REMOVAL_STATUS_REMOVED);

  // Path should be accessible with any capitalization, sanitized or
  // unsanitized.
  base::string16 lowercase_path = base::ToLowerASCII(path.value());
  EXPECT_EQ(REMOVAL_STATUS_REMOVED, instance_->GetRemovalStatus(path));
  EXPECT_EQ(REMOVAL_STATUS_REMOVED,
            instance_->GetRemovalStatus(base::FilePath(lowercase_path)));

  base::string16 sanitized_path = SanitizePath(path);
  EXPECT_EQ(REMOVAL_STATUS_REMOVED,
            instance_->GetRemovalStatusOfSanitizedPath(sanitized_path));

  FileRemovalStatusUpdater::SanitizedPathToRemovalStatusMap all_statuses =
      instance_->GetAllRemovalStatuses();
  EXPECT_EQ(1U, all_statuses.size());
  EXPECT_EQ(sanitized_path, all_statuses.begin()->first);
  EXPECT_EQ(path, all_statuses.begin()->second.path);
  EXPECT_EQ(REMOVAL_STATUS_REMOVED,
            all_statuses.begin()->second.removal_status);
}

}  // namespace chrome_cleaner
