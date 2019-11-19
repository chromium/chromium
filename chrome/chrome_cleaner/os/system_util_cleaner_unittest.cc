// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/system_util_cleaner.h"

#include <windows.h>

#include <aclapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdint.h>
#include <wincrypt.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "sandbox/win/src/sid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

class ServiceUtilCleanerTest : public testing::Test {
 public:
  void SetUp() override {
    // Cleanup previous run. This may happen when previous execution of unittest
    // crashed, leaving background processes/services.
    ASSERT_TRUE(EnsureNoTestServicesRunning());
  }
};

}  // namespace

TEST(SystemUtilCleanerTests, AcquireDebugRightsPrivileges) {
  ASSERT_FALSE(HasDebugRightsPrivileges());
  EXPECT_TRUE(AcquireDebugRightsPrivileges());
  EXPECT_TRUE(HasDebugRightsPrivileges());
  EXPECT_TRUE(ReleaseDebugRightsPrivileges());
  EXPECT_FALSE(HasDebugRightsPrivileges());
}

TEST(SystemUtilCleanerTests, OpenRegistryKeyWithInvalidParameter) {
  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, L"non-existing key path");
  base::win::RegKey key;
  EXPECT_FALSE(key_path.Open(KEY_READ, &key));
}

TEST_F(ServiceUtilCleanerTest, DeleteService) {
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  service_handle.Close();

  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(DeleteService(service_handle.service_name()));
  EXPECT_TRUE(WaitForServiceDeleted(service_handle.service_name()));
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
}

// Disabled: https://crbug.com/956016
TEST_F(ServiceUtilCleanerTest, DISABLED_StopAndDeleteRunningService) {
  // Install and launch the service.
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());
  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(IsProcessRunning(kTestServiceExecutableName));
  service_handle.Close();

  // Stop the service.
  EXPECT_TRUE(StopService(service_handle.service_name()));
  EXPECT_TRUE(WaitForProcessesStopped(kTestServiceExecutableName));
  EXPECT_TRUE(WaitForServiceStopped(service_handle.service_name()));

  // Delete the service
  EXPECT_TRUE(DeleteService(service_handle.service_name()));
  EXPECT_TRUE(WaitForServiceDeleted(service_handle.service_name()));

  // The service must be fully stopped and deleted.
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
  EXPECT_FALSE(IsProcessRunning(kTestServiceExecutableName));
}

// Disabled: https://crbug.com/956016
TEST_F(ServiceUtilCleanerTest, DISABLED_DeleteRunningService) {
  // Install and launch the service.
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());
  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(IsProcessRunning(kTestServiceExecutableName));
  service_handle.Close();

  // Delete the service
  EXPECT_TRUE(DeleteService(service_handle.service_name()));

  // The service must be fully stopped and deleted.
  EXPECT_TRUE(WaitForProcessesStopped(kTestServiceExecutableName));
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
  EXPECT_FALSE(IsProcessRunning(kTestServiceExecutableName));
}

TEST_F(ServiceUtilCleanerTest, QuarantineFolderPermission) {
  base::ScopedPathOverride local_app_data_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));

  base::FilePath quarantine_path;
  EXPECT_TRUE(InitializeQuarantineFolder(&quarantine_path));

  PSID owner_sid;
  PACL dacl;
  PSECURITY_DESCRIPTOR security_descriptor;
  // Get the ownership and ACL of the quarantine folder and check the values.
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            ::GetNamedSecurityInfo(
                quarantine_path.AsUTF16Unsafe().c_str(), SE_FILE_OBJECT,
                OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                &owner_sid, /*psidGroup=*/nullptr, &dacl,
                /*pSacl=*/nullptr, &security_descriptor));

  sandbox::Sid admin_sid(WinBuiltinAdministratorsSid);
  ASSERT_TRUE(admin_sid.IsValid());

  // Check that the administrator group is the owner.
  EXPECT_TRUE(::EqualSid(owner_sid, admin_sid.GetPSID()));

  EXPLICIT_ACCESS* explicit_access;
  ULONG entry_count;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            ::GetExplicitEntriesFromAcl(dacl, &entry_count, &explicit_access));
  // ACL should only contains one rule.
  ASSERT_EQ(1UL, entry_count);

  // Administrator group should have full access.
  EXPECT_EQ(static_cast<DWORD>(FILE_ALL_ACCESS),
            FILE_ALL_ACCESS & explicit_access[0].grfAccessPermissions);
  EXPECT_EQ(static_cast<DWORD>(NO_INHERITANCE),
            explicit_access[0].grfInheritance);

  EXPECT_EQ(TRUSTEE_IS_SID, explicit_access[0].Trustee.TrusteeForm);
  // The trustee of the rule should be administrator group.
  EXPECT_TRUE(
      ::EqualSid(explicit_access[0].Trustee.ptstrName, admin_sid.GetPSID()));

  ::LocalFree(explicit_access);
  ::LocalFree(security_descriptor);
}

TEST_F(ServiceUtilCleanerTest, DefaultQuarantineFolderPath) {
  base::ScopedPathOverride local_app_data_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));

  base::FilePath quarantine_path;
  EXPECT_TRUE(InitializeQuarantineFolder(&quarantine_path));

  base::FilePath product_path;
  ASSERT_TRUE(GetAppDataProductDirectory(&product_path));
  const base::FilePath default_path = product_path.Append(kQuarantineFolder);
  EXPECT_EQ(quarantine_path, default_path);
}

TEST_F(ServiceUtilCleanerTest, SpecifiedQuarantineFolderPath) {
  // Override the default path of local appdata, so if we fail to redirect the
  // quarantine folder, the test won't drop any file in the real directory.
  base::ScopedPathOverride local_app_data_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kQuarantineDirSwitch, temp_dir.GetPath());

  base::FilePath quarantine_path;
  EXPECT_TRUE(InitializeQuarantineFolder(&quarantine_path));
  EXPECT_EQ(quarantine_path, temp_dir.GetPath());
}

}  // namespace chrome_cleaner
