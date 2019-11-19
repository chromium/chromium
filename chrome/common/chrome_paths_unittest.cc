// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <stdlib.h>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

// Test the behavior of chrome::GetUserCacheDirectory.
// See that function's comments for discussion of the subtleties.
TEST(ChromePaths, UserCacheDir) {
  base::FilePath test_profile_dir, cache_dir;
#if defined(OS_MACOSX)
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &test_profile_dir));
  test_profile_dir = test_profile_dir.Append("foobar");
  base::FilePath expected_cache_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &expected_cache_dir));
  expected_cache_dir = expected_cache_dir.Append("foobar");
#elif(OS_ANDROID)
  // No matter what the test_profile_dir is, Android always use the
  // application's cache directory since multiple profiles are not
  // supported.
  base::FilePath expected_cache_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &expected_cache_dir));
#elif(OS_POSIX)
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  // Note: we assume XDG_CACHE_HOME/XDG_CONFIG_HOME are at their
  // default settings.
  test_profile_dir = homedir.Append(".config/foobar");
  base::FilePath expected_cache_dir = homedir.Append(".cache/foobar");
#endif

  // Verify that a profile in the special platform-specific source
  // location ends up in the special target location.
#if !defined(OS_WIN)  // No special behavior on Windows.
  GetUserCacheDirectory(test_profile_dir, &cache_dir);
  EXPECT_EQ(expected_cache_dir.value(), cache_dir.value());
#endif

  // Verify that a profile in some other random directory doesn't use
  // the special cache dir.
  test_profile_dir = base::FilePath(FILE_PATH_LITERAL("/some/other/path"));
  GetUserCacheDirectory(test_profile_dir, &cache_dir);
#if defined(OS_ANDROID)
  EXPECT_EQ(expected_cache_dir.value(), cache_dir.value());
#else
  EXPECT_EQ(test_profile_dir.value(), cache_dir.value());
#endif
}

#if defined(OS_LINUX)
TEST(ChromePaths, DefaultUserDataDir) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string orig_chrome_config_home;
  bool chrome_config_home_was_set =
      env->GetVar("CHROME_CONFIG_HOME", &orig_chrome_config_home);
  if (chrome_config_home_was_set)
    env->UnSetVar("CHROME_CONFIG_HOME");

  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  std::string expected_branding;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

  if (chrome_config_home_was_set)
    env->SetVar("CHROME_CONFIG_HOME", orig_chrome_config_home);
  else
    env->UnSetVar("CHROME_CONFIG_HOME");
}
#endif

}  // namespace chrome
