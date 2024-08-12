// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/per_install_values.h"

#include <shlobj.h>

#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

TEST(PerInstallValuesTest, SetGetDelete) {
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  const HKEY root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                      : HKEY_CURRENT_USER;
#else
  const HKEY root = HKEY_CURRENT_USER;
#endif

  const std::wstring key_path =
      install_static::IsSystemInstall()
          ? install_static::GetClientStateMediumKeyPath()
          : install_static::GetClientStateKeyPath();
  const bool key_already_exists =
      base::win::RegKey(root, key_path.c_str(),
                        KEY_WOW64_32KEY | KEY_QUERY_VALUE)
          .Valid();

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  if (install_static::IsSystemInstall() && !key_already_exists &&
      !::IsUserAnAdmin()) {
    GTEST_SKIP();
  }
#endif

  PerInstallValue test_value(L"PerInstallValueTest");
  ASSERT_FALSE(test_value.Get());

  test_value.Set(base::Value(227));
  ASSERT_EQ(*test_value.Get(), base::Value(227));
  test_value.Delete();
  ASSERT_FALSE(test_value.Get());

  if (!key_already_exists) {
    ASSERT_EQ(
        base::win::RegKey(root, key_path.c_str(), KEY_WOW64_32KEY | DELETE)
            .DeleteKey(L""),
        ERROR_SUCCESS);
  }
}

}  // namespace installer
