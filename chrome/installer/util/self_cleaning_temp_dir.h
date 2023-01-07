// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SELF_CLEANING_TEMP_DIR_H_
#define CHROME_INSTALLER_UTIL_SELF_CLEANING_TEMP_DIR_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"

namespace installer {

// A helper class for managing a temporary directory.  In relation to
// base::ScopedTempDir, this class additionally cleans up all non-empty parent
// directories of the temporary directory that are created by an instance.
class SelfCleaningTempDir {
 public:
  typedef base::FilePath::StringType StringType;

  SelfCleaningTempDir();

  SelfCleaningTempDir(const SelfCleaningTempDir&) = delete;
  SelfCleaningTempDir& operator=(const SelfCleaningTempDir&) = delete;

  // Performs a Delete().
  ~SelfCleaningTempDir();

  // Creates a temporary directory named |temp_name| under |parent_dir|,
  // creating intermediate directories as needed.
  bool Initialize(const base::FilePath& parent_dir,
                  const StringType& temp_name);

  // Returns the temporary directory created in Initialize().
  const base::FilePath& path() const { return temp_dir_; }

  // Deletes the temporary directory created in Initialize() and all of its
  // contents, as well as all empty intermediate directories.  Any of these that
  // cannot be deleted immediately are scheduled for deletion upon reboot.
  bool Delete();

 private:
  static void GetTopDirToCreate(const base::FilePath& temp_parent_dir,
                                base::FilePath* base_dir);

  // The topmost directory created.
  base::FilePath base_dir_;

  // The temporary directory.
  base::FilePath temp_dir_;

  FRIEND_TEST_ALL_PREFIXES(SelfCleaningTempDirTest, TopLevel);
  FRIEND_TEST_ALL_PREFIXES(SelfCleaningTempDirTest, TopLevelPlusOne);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_SELF_CLEANING_TEMP_DIR_H_
