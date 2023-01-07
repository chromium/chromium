// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/process.h"

#include <windows.h>

#include <ctype.h>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

bool PathIsInSet(const base::FilePath& path,
                 const std::set<std::wstring>& path_set) {
  std::wstring (*to_lower)(base::WStringPiece) = &base::ToLowerASCII;
  return base::Contains(path_set, base::ToLowerASCII(path.value()), to_lower);
}

}  // namespace

TEST(ProcessTest, LoadedModules_Self) {
  std::set<std::wstring> names;
  ASSERT_TRUE(
      GetLoadedModuleFileNames(base::GetCurrentProcessHandle(), &names));

  // Check that the list of modules includes at least the executable.
  const base::FilePath exe_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_TRUE(PathIsInSet(exe_path, names));
}

TEST(ProcessTest, LoadedModules_InvalidProcess) {
  std::set<std::wstring> names;
  // INVALID_HANDLE_VALUE is actually -1, the same as the pseudo-handle that
  // represents the current process, so in this context it's not invalid. Test
  // null instead. Windows, everyone!
  EXPECT_FALSE(GetLoadedModuleFileNames(nullptr, &names));
}

TEST(ProcessTest, ProcessExecutablePath_Self) {
  std::wstring path;
  ASSERT_TRUE(GetProcessExecutablePath(base::GetCurrentProcessHandle(), &path));

  const base::FilePath self_exe_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_EQ(base::ToLowerASCII(self_exe_path.value()),
            base::ToLowerASCII(path));
}

TEST(ProcessTest, ProcessExecutablePath_InvalidProcess) {
  std::wstring path;
  EXPECT_FALSE(GetProcessExecutablePath(nullptr, &path));
}

}  // namespace chrome_cleaner
