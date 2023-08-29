// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_key_backup.h"

#include <windows.h>

#include <memory>

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/registry_test_data.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

class RegistryKeyBackupTest : public testing::Test {
 protected:
  static void TearDownTestCase() { logging::CloseLogFile(); }

  void SetUp() override {
    ASSERT_TRUE(test_data_.Initialize(HKEY_CURRENT_USER, L"SOFTWARE\\TmpTmp"));
    destination_path_.assign(test_data_.base_path()).append(L"\\Destination");
  }

  RegistryTestData test_data_;
  std::wstring destination_path_;
};

// Test that writing an uninitialized backup does nothing.
TEST_F(RegistryKeyBackupTest, Uninitialized) {
  RegistryKeyBackup backup;

  EXPECT_TRUE(backup.WriteTo(test_data_.root_key(), destination_path_.c_str(),
                             WorkItem::kWow64Default));
  EXPECT_FALSE(
      RegKey(test_data_.root_key(), destination_path_.c_str(), KEY_READ)
          .Valid());
}

// Test that initializing a backup with a non-existent key works, and that
// writing it back out does nothing.
TEST_F(RegistryKeyBackupTest, MissingKey) {
  std::wstring non_existent_key_path(test_data_.base_path() + L"\\NoKeyHere");
  RegistryKeyBackup backup;

  EXPECT_TRUE(backup.Initialize(test_data_.root_key(),
                                non_existent_key_path.c_str(),
                                WorkItem::kWow64Default));
  EXPECT_TRUE(backup.WriteTo(test_data_.root_key(), destination_path_.c_str(),
                             WorkItem::kWow64Default));
  EXPECT_FALSE(
      RegKey(test_data_.root_key(), destination_path_.c_str(), KEY_READ)
          .Valid());
}

// Test that reading some data then writing it out does the right thing.
TEST_F(RegistryKeyBackupTest, ReadWrite) {
  RegistryKeyBackup backup;

  EXPECT_TRUE(backup.Initialize(test_data_.root_key(),
                                test_data_.non_empty_key_path().c_str(),
                                WorkItem::kWow64Default));
  EXPECT_TRUE(backup.WriteTo(test_data_.root_key(), destination_path_.c_str(),
                             WorkItem::kWow64Default));
  test_data_.ExpectMatchesNonEmptyKey(test_data_.root_key(),
                                      destination_path_.c_str());
}

// Test that reading some data, swapping, then writing it out does the right
// thing.
TEST_F(RegistryKeyBackupTest, Swap) {
  RegistryKeyBackup backup;
  RegistryKeyBackup other_backup;

  EXPECT_TRUE(backup.Initialize(test_data_.root_key(),
                                test_data_.non_empty_key_path().c_str(),
                                WorkItem::kWow64Default));
  backup.swap(other_backup);
  EXPECT_TRUE(other_backup.WriteTo(test_data_.root_key(),
                                   destination_path_.c_str(),
                                   WorkItem::kWow64Default));

  // Now make sure the one we started with is truly empty.
  EXPECT_EQ(ERROR_SUCCESS, RegKey(test_data_.root_key(),
                                  destination_path_.c_str(), KEY_QUERY_VALUE)
                               .DeleteKey(L""));
  EXPECT_TRUE(backup.WriteTo(test_data_.root_key(), destination_path_.c_str(),
                             WorkItem::kWow64Default));
  EXPECT_FALSE(
      RegKey(test_data_.root_key(), destination_path_.c_str(), KEY_READ)
          .Valid());
}
