// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_UTIL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_command_line.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

// Setup all configs required by tests (like disabling path caching). This
// should be done in the main function of the test binary, not by individual
// tests.
//
// Returns false if setup fails. Tests shouldn't be run if the setup fails.
bool SetupTestConfigs();

// Sets up all test configs, as SetupTestConfigs, using the given list of
// |catalogs| instead of TestUwSCatalog.
bool SetupTestConfigsWithCatalogs(const PUPData::UwSCatalogs& catalogs);

// Launch Chrome Cleaner unit tests in the given test suite and given list of
// UwSCatalogs. Returns the exit code.
int RunChromeCleanerTestSuite(int argc,
                              char** argv,
                              const PUPData::UwSCatalogs& catalogs);

// While this class is in scope, Rebooter::IsPostReboot will return true.
class ScopedIsPostReboot {
 public:
  ScopedIsPostReboot();

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

class LoggingOverride {
 public:
  LoggingOverride();
  ~LoggingOverride();

  // Intercepts all log messages.
  static bool LogMessageHandler(int severity,
                                const char* file,
                                int line,
                                size_t message_start,
                                const std::string& str) {
    DCHECK(active_logging_messages_);
    active_logging_messages_->push_back(str);
    return false;
  }

  // Returns true if one of the messages contains |sub_string|.
  bool LoggingMessagesContain(const std::string& sub_string);

  // Returns true if one of the messages contains both |sub_string| and
  // |sub_string2|.
  bool LoggingMessagesContain(const std::string& sub_string1,
                              const std::string& sub_string2);

  // Remove all the log messages.
  void FlushMessages() { logging_messages_.clear(); }

  std::vector<std::string> logging_messages_;
  static std::vector<std::string>* active_logging_messages_;
};

// Validate that the run once on restart registry value contains the given
// |sub_string|.
bool RunOnceCommandLineContains(const std::wstring& product_shortname,
                                const wchar_t* sub_string);

// Validate that the run once on restart switch-containing registry value
// contains the given |sub_string|.
bool RunOnceOverrideCommandLineContains(const std::string& cleanup_id,
                                        const wchar_t* sub_string);

// Register a task with the given task scheduler. If the task is successfully
// added, |task_info| will contain all the information about the task.
// Callers are responsible for deleting the test task.
bool RegisterTestTask(TaskScheduler* task_scheduler,
                      TaskScheduler::TaskInfo* task_info);

// Append switches to the command line that is used to run cleaner or reporter
// in tests. Switches will disable logs upload, profile reset and other side
// effects.
void AppendTestSwitches(const base::ScopedTempDir& temp_dir,
                        base::CommandLine* command_line);

// Expect the |expected_path| to be found in expanded disk footprint of |pup|.
void ExpectDiskFootprint(const PUPData::PUP& pup,
                         const base::FilePath& expected_path);

// Expect the scheduled task footprint to be found in |pup|.
void ExpectScheduledTaskFootprint(const PUPData::PUP& pup,
                                  const wchar_t* task_name);

// This function is the 8 bits version of WStringContainsCaseInsensitive in
// chrome_cleaner/string_util. Since it's only used in tests, we decided not to
// move it to the main lib.
bool StringContainsCaseInsensitive(const std::string& value,
                                   const std::string& substring);

// Expect two FilePathSets to be equal. Log files that are matched in excess
// or expected files that are missing.
void ExpectEqualFilePathSets(const FilePathSet& matched_files,
                             const FilePathSet& expected_files);

// Returns the path that base::DIR_SYSTEM is transparently redirected to under
// Wow64. Note that on 32-bit Windows this will always return the empty string
// because base::DIR_SYSTEM is not redirected.
base::FilePath GetWow64RedirectedSystemPath();

// Returns the path to a sample DLL file that can be used in tests.
base::FilePath GetSampleDLLPath();

// Returns the path to a signed sample DLL file that can be used in tests.
base::FilePath GetSignedSampleDLLPath();

// A ScopedTempDir with the ability to create and delete subdirs of
// c:\windows\system32, which are Wow64-redirected by default. This turns off
// Wow64 redirection so the subdir is created in the real c:\windows\system32
// even in binaries that would normally be redirected to c:\windows\SysWOW64.
class ScopedTempDirNoWow64 : protected base::ScopedTempDir {
 public:
  ScopedTempDirNoWow64();
  ~ScopedTempDirNoWow64();

  // Creates a unique subdirectory under system32, bypassing Wow64 redirection,
  // and takes ownership of it.
  [[nodiscard]] bool CreateUniqueSystem32TempDir();

  // Convenience function to call CreateUniqueSystem32TempDir and create an
  // empty file with the given |file_name| in the resulting directory.
  [[nodiscard]] bool CreateEmptyFileInUniqueSystem32TempDir(
      const std::wstring& file_name);

  using base::ScopedTempDir::Delete;
  using base::ScopedTempDir::GetPath;
  using base::ScopedTempDir::IsValid;
  using base::ScopedTempDir::Take;

  // Do not give access to CreateUniqueTempDir, CreateUniqueTempDirUnderPath,
  // or Set because they can set the temp dir path to a non-Wow64-redirected
  // directory.
};


// Check that the test has administrator privileges, but not debug privileges.
// This function drops unneeded privileges if possible, but won't try to raise
// privileges. Returns false if the privileges could not be made correct.
bool CheckTestPrivileges();

// On Windows, sometimes the copied files don't have correct ACLs.
// So we reset ACL before running the test.
// For debug, it will reset ucrtbased.dll. For release, it does nothing.
// See crbug.com/956016.
bool ResetAclForUcrtbase();

// Accepts PUPData::PUP parameters with id equals to |expected_id|.
MATCHER_P(PupHasId, expected_id, "") {
  return arg->signature().id == expected_id;
}

// Accepts PUPData::PUP parameters with |size| expanded disk footprints.
MATCHER_P(PupHasFileListSize, size, "") {
  return arg->expanded_disk_footprints.size() == static_cast<size_t>(size);
}

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_UTIL_H_
