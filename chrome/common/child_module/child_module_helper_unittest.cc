// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include "base/files/file_path.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_path_override.h"
#include "base/win/sid.h"
#endif

namespace child_module {

#if BUILDFLAG(IS_WIN)
namespace {

// A user's SID.
constexpr wchar_t kSid[] = L"S-1-5-21-2127521184-1604012920-1887927527-1001";

// A User Data directory. It need not exist, since `ComputeUserPathComponent()`
// does not access the filesystem.
constexpr base::FilePath::CharType kUserDataDir[] = FILE_PATH_LITERAL(
    "C:\\Users\\Alice\\AppData\\Local\\Google\\Chrome\\User Data");

// The path component for `kSid` and `kUserDataDir`.
constexpr base::FilePath::CharType kUserPathComponent[] =
    FILE_PATH_LITERAL("o5av4ww5iyu66tvq");

base::win::Sid GetSid() {
  return *base::win::Sid::FromSddlString(kSid);
}

}  // namespace
#endif

TEST(ChildModuleHelperTest, GetManifestPath) {
  const base::FilePath version_dir =
      base::FilePath(FILE_PATH_LITERAL("modules")).AppendASCII("147.0.7727.51");
  EXPECT_EQ(GetManifestPath(version_dir),
            version_dir.Append(kManifestFilename));
  EXPECT_EQ(GetManifestPath(base::FilePath()), base::FilePath());
}

TEST(ChildModuleHelperTest, GetRendererBinaryPath) {
  const base::FilePath modules_dir(FILE_PATH_LITERAL("modules"));
  const base::Version version("147.0.7727.51");
  base::FilePath expected_path = modules_dir.AppendASCII("147.0.7727.51");
#if BUILDFLAG(IS_WIN)
  expected_path = expected_path.Append(chrome::kRendererDll);
#else
  expected_path = expected_path.Append(chrome::kRendererProcessExecutableName);
#endif
  EXPECT_EQ(GetRendererBinaryPath(modules_dir, version), expected_path);

  EXPECT_EQ(GetRendererBinaryPath(modules_dir, base::Version()),
            base::FilePath());
  EXPECT_EQ(GetRendererBinaryPath(base::FilePath(), version), base::FilePath());
}

#if BUILDFLAG(IS_WIN)
TEST(ChildModuleHelperTest, GetModulesDirForUser) {
  const base::FilePath user_path_component(kUserPathComponent);
  const base::FilePath base_version_dir(FILE_PATH_LITERAL(
      "C:\\Program Files\\Google\\Chrome\\Application\\147.0.7727.51"));
  EXPECT_EQ(
      GetModulesDirForUser(base_version_dir, user_path_component),
      base_version_dir.Append(kModulesDirName).Append(user_path_component));
  EXPECT_EQ(GetModulesDirForUser(base::FilePath(), user_path_component),
            base::FilePath());

  // Malformed components are rejected.
  for (base::FilePath::StringViewType component : {
           FILE_PATH_LITERAL(""),
           FILE_PATH_LITERAL("o5av4ww5iyu66tv"),        // Too short.
           FILE_PATH_LITERAL("o5av4ww5iyu66tvqq"),      // Too long.
           FILE_PATH_LITERAL("O5AV4WW5IYU66TVQ"),       // Uppercase.
           FILE_PATH_LITERAL("o5av4ww5iyu66tv1"),       // Not Base32.
           FILE_PATH_LITERAL("o5av4ww5iyu66tv8"),       // Not Base32.
           FILE_PATH_LITERAL("o5av4ww5iyu6\\tvq"),      // Path separator.
           FILE_PATH_LITERAL("..\\..\\..\\..\\..\\."),  // Parent references.
       }) {
    EXPECT_EQ(GetModulesDirForUser(base_version_dir, base::FilePath(component)),
              base::FilePath())
        << base::FilePath(component);
  }

  // The single-argument overload uses the running browser's base version.
  base::ScopedPathOverride exe_dir_override(base::DIR_EXE);
  const base::FilePath exe_dir = base::PathService::CheckedGet(base::DIR_EXE);
  EXPECT_EQ(GetModulesDirForUser(user_path_component),
            GetModulesDirForUser(exe_dir.AppendASCII(chrome::kChromeVersion),
                                 user_path_component));
  EXPECT_EQ(GetModulesDirForUser(base::FilePath()), base::FilePath());
}

TEST(ChildModuleHelperTest, GetUserPathComponentFromRendererBinaryPath) {
  const base::FilePath user_path_component(kUserPathComponent);
  const base::FilePath base_version_dir(FILE_PATH_LITERAL(
      "C:\\Program Files\\Google\\Chrome\\Application\\147.0.7727.51"));
  const base::Version version("147.0.7727.52");

  EXPECT_EQ(GetUserPathComponentFromRendererBinaryPath(GetRendererBinaryPath(
                GetModulesDirForUser(base_version_dir, user_path_component),
                version)),
            user_path_component);

  // Paths that do not contain a well-formed component are rejected.
  EXPECT_EQ(GetUserPathComponentFromRendererBinaryPath(GetRendererBinaryPath(
                base_version_dir.Append(kModulesDirName), version)),
            base::FilePath());
  EXPECT_EQ(GetUserPathComponentFromRendererBinaryPath(
                base::FilePath(chrome::kRendererDll)),
            base::FilePath());
  EXPECT_EQ(GetUserPathComponentFromRendererBinaryPath(base::FilePath()),
            base::FilePath());
}

// The component is persisted on disk by the installer and computed
// independently by the browser, so it must never change.
TEST(ComputeUserPathComponentTest, IsStable) {
  EXPECT_EQ(ComputeUserPathComponent(GetSid(), base::FilePath(kUserDataDir)),
            base::FilePath(kUserPathComponent));
}

// ASCII characters in the User Data directory are folded to lowercase.
TEST(ComputeUserPathComponentTest, FoldsCase) {
  const base::FilePath user_data_dir(FILE_PATH_LITERAL(
      "c:\\users\\alice\\appdata\\local\\google\\chrome\\user data"));
  EXPECT_EQ(ComputeUserPathComponent(GetSid(), user_data_dir),
            base::FilePath(kUserPathComponent));
}

// Forward slashes in the User Data directory are equivalent to backslashes.
TEST(ComputeUserPathComponentTest, NormalizesSeparators) {
  const base::FilePath user_data_dir(FILE_PATH_LITERAL(
      "C:/Users/Alice/AppData/Local/Google/Chrome/User Data"));
  EXPECT_EQ(ComputeUserPathComponent(GetSid(), user_data_dir),
            base::FilePath(kUserPathComponent));
}

// Trailing separators on the User Data directory are ignored.
TEST(ComputeUserPathComponentTest, IgnoresTrailingSeparators) {
  EXPECT_EQ(ComputeUserPathComponent(
                GetSid(), base::FilePath(kUserDataDir).AsEndingWithSeparator()),
            base::FilePath(kUserPathComponent));
  const base::FilePath user_data_dir(FILE_PATH_LITERAL(
      "C:/Users/Alice/AppData/Local/Google/Chrome/User Data/"));
  EXPECT_EQ(ComputeUserPathComponent(GetSid(), user_data_dir),
            base::FilePath(kUserPathComponent));
}

// Different users get different components for the same User Data directory.
TEST(ComputeUserPathComponentTest, DependsOnSid) {
  ASSERT_OK_AND_ASSIGN(const auto other_sid,
                       base::win::Sid::FromSddlString(
                           L"S-1-5-21-2127521184-1604012920-1887927527-1002"));
  const base::FilePath component =
      ComputeUserPathComponent(other_sid, base::FilePath(kUserDataDir));
  EXPECT_FALSE(component.empty());
  EXPECT_NE(component, base::FilePath(kUserPathComponent));
}

// A user gets different components for different User Data directories.
TEST(ComputeUserPathComponentTest, DependsOnUserDataDir) {
  const base::FilePath other_user_data_dir(FILE_PATH_LITERAL(
      "C:\\Users\\Alice\\AppData\\Local\\Google\\Chrome\\Profile 2"));
  const base::FilePath component =
      ComputeUserPathComponent(GetSid(), other_user_data_dir);
  EXPECT_FALSE(component.empty());
  EXPECT_NE(component, base::FilePath(kUserPathComponent));
}

// Empty and relative User Data directories are rejected.
TEST(ComputeUserPathComponentTest, RejectsEmptyOrRelativeUserDataDir) {
  EXPECT_EQ(ComputeUserPathComponent(GetSid(), base::FilePath()),
            base::FilePath());
  EXPECT_EQ(ComputeUserPathComponent(
                GetSid(), base::FilePath(FILE_PATH_LITERAL("User Data"))),
            base::FilePath());
}
#endif

}  // namespace child_module
