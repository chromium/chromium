// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include <stdlib.h>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <string>

#include "base/test/file_path_reparse_point_win.h"
#include "base/test/test_file_util.h"
#endif

namespace chrome {

// Test the behavior of chrome::GetUserCacheDirectory.
// See that function's comments for discussion of the subtleties.
TEST(ChromePaths, UserCacheDir) {
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAndroidKeepProfilePartitionDirsInCacheDir);
#endif
  base::FilePath test_profile_dir;  // Platform-specific profile directory path.
  base::FilePath expected_cache_dir;

#if BUILDFLAG(IS_WIN)
  test_profile_dir = base::FilePath(FILE_PATH_LITERAL("C:\\Users\\Foo\\Bar"));
  expected_cache_dir = base::FilePath(FILE_PATH_LITERAL("C:\\Users\\Foo\\Bar"));
#elif BUILDFLAG(IS_MAC)
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &test_profile_dir));
  test_profile_dir = test_profile_dir.Append("foobar");
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &expected_cache_dir));
  expected_cache_dir = expected_cache_dir.Append("foobar");
#elif BUILDFLAG(IS_ANDROID)
  // No matter what the test_profile_dir is, Android always uses the
  // application's cache directory since multiple profiles are not supported.
  test_profile_dir = base::FilePath("\\Not a valid path");
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &expected_cache_dir));
#elif BUILDFLAG(IS_POSIX)
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  // Note: we assume XDG_CACHE_HOME/XDG_CONFIG_HOME are at their
  // default settings.
  test_profile_dir = homedir.Append(".config/foobar");
  expected_cache_dir = homedir.Append(".cache/foobar");
#else
#error Unsupported platform
#endif  // BUILDFLAG(IS_WIN)

  // Verify the expectations above for the platform-specific profile directory.
  // On Linux and Mac the platform-specific profile directory is in a special
  // platform-specific location, and the cache directory also ends up in in a
  // special target location.
  base::FilePath cache_dir;
  GetUserCacheDirectory(test_profile_dir, &cache_dir);
  EXPECT_EQ(expected_cache_dir.value(), cache_dir.value());

  // Verify that a profile in some other random directory doesn't use
  // the special cache directory.
  base::FilePath non_special_profile_dir =
      base::FilePath(FILE_PATH_LITERAL("/some/other/path"));
  GetUserCacheDirectory(non_special_profile_dir, &cache_dir);
#if BUILDFLAG(IS_ANDROID)
  // Android always uses the same application cache directory.
  EXPECT_EQ(expected_cache_dir.value(), cache_dir.value());
#else
  EXPECT_EQ(non_special_profile_dir.value(), cache_dir.value());
#endif
}

#if BUILDFLAG(IS_ANDROID)
class ChromePathsAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &cache_dir_base_));
    default_profile_dir_ = user_data_dir_.AppendASCII(chrome::kInitialProfile);
    partition_profile_dir_ = default_profile_dir_.AppendASCII("Storage")
                                 .AppendASCII("ext")
                                 .AppendASCII("glic");
  }

  base::FilePath user_data_dir_;
  base::FilePath cache_dir_base_;
  base::FilePath default_profile_dir_;
  base::FilePath partition_profile_dir_;
};

TEST_F(ChromePathsAndroidTest, DefaultProfileFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAndroidKeepProfilePartitionDirsInCacheDir);

  base::FilePath cache_dir;
  GetUserCacheDirectory(default_profile_dir_, &cache_dir);
  EXPECT_EQ(cache_dir_base_.value(), cache_dir.value());
}

TEST_F(ChromePathsAndroidTest, PartitionFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAndroidKeepProfilePartitionDirsInCacheDir);

  base::FilePath cache_dir;
  GetUserCacheDirectory(partition_profile_dir_, &cache_dir);
  base::FilePath expected_partition_cache_dir =
      cache_dir_base_.AppendASCII("Storage").AppendASCII("ext").AppendASCII(
          "glic");
  EXPECT_EQ(expected_partition_cache_dir.value(), cache_dir.value());
}

TEST_F(ChromePathsAndroidTest, TestProfileFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAndroidKeepProfilePartitionDirsInCacheDir);

  // Simulate a TestingProfile directory.
  const base::FilePath test_profile_dir =
      user_data_dir_.AppendASCII("test_profile_dir");

  base::FilePath cache_dir;
  GetUserCacheDirectory(test_profile_dir, &cache_dir);
  EXPECT_EQ(cache_dir_base_.AppendASCII("test_profile_dir").value(),
            cache_dir.value());

  // Test partition under the test profile directory.
  const base::FilePath test_partition_profile_dir =
      test_profile_dir.AppendASCII("Storage").AppendASCII("ext").AppendASCII(
          "glic");
  GetUserCacheDirectory(test_partition_profile_dir, &cache_dir);
  const base::FilePath expected_partition_cache_dir =
      cache_dir_base_.AppendASCII("test_profile_dir")
          .AppendASCII("Storage")
          .AppendASCII("ext")
          .AppendASCII("glic");
  EXPECT_EQ(expected_partition_cache_dir.value(), cache_dir.value());
}

TEST_F(ChromePathsAndroidTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAndroidKeepProfilePartitionDirsInCacheDir);

  base::FilePath cache_dir;
  // Default profile dir should NOT append anything.
  GetUserCacheDirectory(default_profile_dir_, &cache_dir);
  EXPECT_EQ(cache_dir_base_.value(), cache_dir.value());

  // Partition profile dir should NOT append anything (old behavior).
  GetUserCacheDirectory(partition_profile_dir_, &cache_dir);
  EXPECT_EQ(cache_dir_base_.value(), cache_dir.value());
}

#endif  // BUILDFLAG(IS_ANDROID)

// Chrome OS doesn't use any of the desktop linux configuration.
#if BUILDFLAG(IS_LINUX)
TEST(ChromePaths, DefaultUserDataDir) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  std::optional<std::string> orig_chrome_config_home =
      env->GetVar("CHROME_CONFIG_HOME");
  if (orig_chrome_config_home.has_value()) {
    env->UnSetVar("CHROME_CONFIG_HOME");
  }

  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  std::string expected_branding;
#if BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  std::string data_dir_basename = "google-chrome-for-testing";
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(skobes): Test channel suffixes with $CHROME_VERSION_EXTRA.
  expected_branding = "google-chrome";
#else
  expected_branding = "chromium";
#endif

  base::FilePath user_data_dir;
  GetDefaultUserDataDirectory(&user_data_dir);
  EXPECT_EQ(home_dir.Append(".config/" + expected_branding).value(),
            user_data_dir.value());

  env->SetVar("CHROME_CONFIG_HOME", "/foo/bar");
  GetDefaultUserDataDirectory(&user_data_dir);
  EXPECT_EQ("/foo/bar/" + expected_branding, user_data_dir.value());

  // TODO(skobes): It would be nice to test $CHROME_USER_DATA_DIR here too, but
  // it's handled by ChromeMainDelegate instead of GetDefaultUserDataDirectory.

  if (orig_chrome_config_home.has_value()) {
    env->SetVar("CHROME_CONFIG_HOME", orig_chrome_config_home.value());
  } else {
    env->UnSetVar("CHROME_CONFIG_HOME");
  }
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST(ChromePaths, UserMediaDirectories) {
  base::FilePath path;
  // Chrome OS does not support custom media directories.
  EXPECT_FALSE(GetUserMusicDirectory(&path));
  EXPECT_FALSE(GetUserPicturesDirectory(&path));
  EXPECT_FALSE(GetUserVideosDirectory(&path));
}
#endif

TEST(ChromePaths, DefaultUserDataDirectory) {
  EXPECT_TRUE(IsUsingDefaultDataDirectory().value());

  SetUsingDefaultUserDataDirectoryForTesting(false);
  EXPECT_FALSE(IsUsingDefaultDataDirectory().value());

  SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
  EXPECT_TRUE(IsUsingDefaultDataDirectory().value());
}

class UserDataDirectoryIdentityTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_.CreateUniqueTempDir()); }
  base::ScopedTempDir temp_;
};

TEST_F(UserDataDirectoryIdentityTest, FirstRunWithCustomDirectory) {
  const base::FilePath custom_dir = temp_.GetPath().AppendASCII("Custom");
  const base::FilePath default_dir = temp_.GetPath().AppendASCII("Default");
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(custom_dir, default_dir));

  // Use the same create-and-select operation as InitializeUserDataDir().
  base::ScopedPathOverride override(chrome::DIR_USER_DATA, custom_dir,
                                    /*is_absolute=*/false, /*create=*/true);
  const base::FilePath actual_dir =
      base::PathService::CheckedGet(DIR_USER_DATA);
  EXPECT_TRUE(IsConfirmedNonDefaultUserDataDirectory(actual_dir, default_dir));
  EXPECT_FALSE(base::PathExists(default_dir));
  ASSERT_TRUE(base::CreateDirectory(default_dir));
  EXPECT_TRUE(IsConfirmedNonDefaultUserDataDirectory(actual_dir, default_dir));
}

TEST_F(UserDataDirectoryIdentityTest, RelativePath) {
  base::FilePath cwd;
  ASSERT_TRUE(base::GetCurrentDirectory(&cwd));
  const base::FilePath relative(FILE_PATH_LITERAL("."));
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(relative, cwd));
  EXPECT_TRUE(
      IsConfirmedNonDefaultUserDataDirectory(relative, temp_.GetPath()));
}

#if BUILDFLAG(IS_WIN)
TEST_F(UserDataDirectoryIdentityTest, CaseVariant) {
  const base::FilePath dir = temp_.GetPath().Append(L"MixedCase");
  const base::FilePath case_variant = temp_.GetPath().Append(L"MIXEDCASE");
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(base::DirectoryExists(case_variant));
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(case_variant, dir));
}

TEST_F(UserDataDirectoryIdentityTest, JunctionToLongDirectory) {
  const base::FilePath root = temp_.GetPath().Append(L"Tree");
  base::ScopedTempDir tree;
  ASSERT_TRUE(tree.Set(base::FilePath(L"\\\\?\\" + root.value())));
  base::FilePath target = root;
  while (target.value().size() <= MAX_PATH) {
    target = target.Append(L"LongDirectoryName");
  }
  const base::FilePath long_path(L"\\\\?\\" + target.value());
  ASSERT_TRUE(base::CreateDirectory(long_path));
  const base::FilePath alias = temp_.GetPath().Append(L"Alias");
  ASSERT_TRUE(base::CreateDirectory(alias));
  auto junction = base::test::FilePathReparsePoint::Create(alias, target);
  ASSERT_TRUE(junction.has_value());
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(alias, long_path));
  EXPECT_TRUE(IsConfirmedNonDefaultUserDataDirectory(long_path, root));
}

TEST_F(UserDataDirectoryIdentityTest, ShortName) {
  const base::FilePath dir = temp_.GetPath().Append(L"LongDirectoryName");
  ASSERT_TRUE(base::CreateDirectory(dir));
  const DWORD size = ::GetShortPathNameW(dir.value().c_str(), nullptr, 0);
  ASSERT_GT(size, 0u);
  std::wstring name(size, L'\0');
  const DWORD length =
      ::GetShortPathNameW(dir.value().c_str(), name.data(), size);
  ASSERT_GT(length, 0u);
  ASSERT_LT(length, size);
  name.resize(length);
  const base::FilePath short_path(name);
  if (short_path.BaseName() == dir.BaseName()) {
    GTEST_SKIP() << "The volume did not assign an 8.3 name.";
  }
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(short_path, dir));
}

TEST_F(UserDataDirectoryIdentityTest, InaccessibleDefault) {
  const base::FilePath dir = temp_.GetPath().Append(L"Default");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePermissionRestorer restore_permissions(dir);
  ASSERT_TRUE(base::MakeFileUnreadable(dir));
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(temp_.GetPath(), dir));
}
#elif BUILDFLAG(IS_POSIX)  // BUILDFLAG(IS_WIN)
TEST_F(UserDataDirectoryIdentityTest, SymbolicLink) {
  const base::FilePath dir = temp_.GetPath().AppendASCII("Default");
  const base::FilePath alias = temp_.GetPath().AppendASCII("Alias");
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(base::CreateSymbolicLink(dir, alias));
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(alias, dir));
  EXPECT_TRUE(IsConfirmedNonDefaultUserDataDirectory(alias, temp_.GetPath()));
}

TEST_F(UserDataDirectoryIdentityTest, UnresolvableDefault) {
  const base::FilePath loop = temp_.GetPath().AppendASCII("Loop");
  ASSERT_TRUE(base::CreateSymbolicLink(loop, loop));
  EXPECT_FALSE(IsConfirmedNonDefaultUserDataDirectory(temp_.GetPath(), loop));
}
#endif                     // BUILDFLAG(IS_WIN)

TEST(ChromePaths, DefaultOrEnforcedDataDirectory) {
  EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), true);

  SetUsingDefaultUserDataDirectoryForTesting(false);
  EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), false);

  SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
  EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), true);

  base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);
  EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), false);

  {
    SetUserDataDirSetByPolicy();
    absl::Cleanup clear_policy = [] {
      ClearUserDataDirSetByPolicyForTesting();
    };
    EXPECT_EQ(IsUsingDefaultDataDirectory(), false);
    EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), true);

    SetUsingDefaultUserDataDirectoryForTesting(false);
    EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), false);

    SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
    EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), true);
  }

  EXPECT_EQ(IsUsingDefaultOrEnforcedDataDirectory(), false);
}

}  // namespace chrome
