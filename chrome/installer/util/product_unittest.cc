// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using registry_util::RegistryOverrideManager;

TEST(ProductTest, ProductInstallBasic) {
  // TODO(tommi): We should mock this and use our mocked distribution.
  const bool system_level = true;
  installer::InstallationState machine_state;
  machine_state.Initialize();

  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  EXPECT_FALSE(user_data_dir.empty());

  base::FilePath program_files;
  ASSERT_TRUE(base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files));
  // The User Data path should never be under program files, even though
  // system_level is true.
  EXPECT_EQ(std::wstring::npos,
            user_data_dir.value().find(program_files.value()));

  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  {
    RegistryOverrideManager override_manager;
    ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(root));

    // There should be no installed version in the registry.
    machine_state.Initialize();
    EXPECT_EQ(nullptr, machine_state.GetProductState(system_level));

    // Let's pretend chrome is installed.
    RegKey version_key(root, install_static::GetClientsKeyPath().c_str(),
                       KEY_ALL_ACCESS);
    ASSERT_TRUE(version_key.Valid());

    const char kCurrentVersion[] = "1.2.3.4";
    base::Version current_version(kCurrentVersion);
    version_key.WriteValue(
        google_update::kRegVersionField,
        base::UTF8ToWide(current_version.GetString()).c_str());

    machine_state.Initialize();
    const installer::ProductState* chrome_state =
        machine_state.GetProductState(system_level);
    ASSERT_NE(nullptr, chrome_state);
    EXPECT_EQ(chrome_state->version(), current_version);
  }
}

TEST(ProductTest, LaunchChrome) {
  // TODO(tommi): Test Product::LaunchChrome and
  // Product::LaunchChromeAndWait.
  NOTIMPLEMENTED();
}
