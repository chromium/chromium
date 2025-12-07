// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_utils.h"

#include <windows.h>

#include <winnt.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/installer.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

#define UPDATER_POLICIES_KEY \
  L"Software\\Policies\\" COMPANY_SHORTNAME_STRING L"\\Update\\"

namespace enterprise_companion {

namespace {

constexpr wchar_t kTestExe[] = L"enterprise_companion_test.exe";
constexpr wchar_t kRegKeyCompanyCloudManagement[] =
    L"Software\\Policies\\" COMPANY_SHORTNAME_STRING "\\CloudManagement\\";

class TestMethodsWin : public TestMethods {
 public:
  base::FilePath GetTestExePath() override {
    return base::PathService::CheckedGet(base::DIR_EXE).Append(kTestExe);
  }

  void ExpectInstalled() override {
    TestMethods::ExpectInstalled();
    ExpectUpdaterRegistration();
  }

  void Clean() override {
    TestMethods::Clean();

    EXPECT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kAppRegKey,
                                KEY_ALL_ACCESS | KEY_WOW64_32KEY)
                  .DeleteKey(L""),
              ERROR_SUCCESS);
    EXPECT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE,
                                kRegKeyCompanyCloudManagement, KEY_ALL_ACCESS)
                  .DeleteKey(L""),
              ERROR_SUCCESS);
    EXPECT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                                KEY_ALL_ACCESS)
                  .DeleteKey(L""),
              ERROR_SUCCESS);
  }

  void ExpectClean() override {
    TestMethods::ExpectClean();
    base::win::RegKey app_key;
    EXPECT_NE(app_key.Open(HKEY_LOCAL_MACHINE, kAppRegKey,
                           KEY_QUERY_VALUE | KEY_WOW64_32KEY),
              ERROR_SUCCESS);
  }

#if BUILDFLAG(CHROMIUM_BRANDING)
  base::FilePath GetOlderVersionExePath() override {
    return base::PathService::CheckedGet(base::DIR_EXE)
        .Append(L"old_enterprise_companion")
#if defined(ARCH_CPU_X86_64)
        .Append(L"chromium_win_x86_64")
#elif defined(ARCH_CPU_X86)
        .Append(L"chromium_win_x86")
#elif defined(ARCH_CPU_ARM64)
        .Append(L"chromium_win_x86_64")
#else
#error Unsupported architecture
#endif
        .Append(L"cipd")
        .Append(L"enterprise_companion_test.exe");
  }
#endif
};

}  // namespace

void ExpectUpdaterRegistration() {
  base::win::RegKey app_key(HKEY_LOCAL_MACHINE, kAppRegKey,
                            KEY_QUERY_VALUE | KEY_WOW64_32KEY);

  std::wstring pv;
  ASSERT_EQ(app_key.ReadValue(kRegValuePV, &pv), ERROR_SUCCESS);
  EXPECT_TRUE(base::Version(base::SysWideToUTF8(pv)).IsValid());

  std::wstring name;
  ASSERT_EQ(app_key.ReadValue(kRegValueName, &name), ERROR_SUCCESS);
  EXPECT_EQ(name, L"" PRODUCT_FULLNAME_STRING);
}

void SetLocalProxyPolicies(
    std::optional<std::string> proxy_mode,
    std::optional<std::string> pac_url,
    std::optional<std::string> proxy_server,
    std::optional<bool> cloud_policy_overrides_platform_policy) {
  base::win::RegKey updater_policies_key(HKEY_LOCAL_MACHINE,
                                         UPDATER_POLICIES_KEY,
                                         KEY_ALL_ACCESS | KEY_WOW64_32KEY);
  if (proxy_mode) {
    updater_policies_key.WriteValue(L"ProxyMode",
                                    base::SysUTF8ToWide(*proxy_mode).c_str());
  }
  if (pac_url) {
    updater_policies_key.WriteValue(L"ProxyPacUrl",
                                    base::SysUTF8ToWide(*pac_url).c_str());
  }
  if (proxy_server) {
    updater_policies_key.WriteValue(L"ProxyServer",
                                    base::SysUTF8ToWide(*proxy_server).c_str());
  }
  if (cloud_policy_overrides_platform_policy) {
    updater_policies_key.WriteValue(L"CloudPolicyOverridesPlatformPolicy",
                                    *cloud_policy_overrides_platform_policy);
  }
}

TestMethods& GetTestMethods() {
  static base::NoDestructor<TestMethodsWin> test_methods;
  return *test_methods;
}

}  // namespace enterprise_companion
