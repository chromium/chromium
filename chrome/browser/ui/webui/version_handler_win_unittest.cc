// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_win.h"

#include "base/test/scoped_os_info_override_win.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/ui/webui/version_handler_win.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebUIWindowsVersion : public testing::Test {
 protected:
  base::win::RegKey ubr_key;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));
    ubr_key.Create(root, ubr_loc, KEY_ALL_ACCESS);
    EXPECT_TRUE(ubr_key.Valid());
  }
  void TearDown() override {
    ubr_key.DeleteKey(ubr_loc);
    ubr_key.Close();
  }

 private:
  const HKEY root = HKEY_LOCAL_MACHINE;
  // Win10 UBR, see base::win::OSInfo
  const wchar_t* ubr_loc = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(WebUIWindowsVersion, Win10Pro) {
  // set Windows Registry Key UBR
  ubr_key.WriteValue(L"UBR", 555);
  ubr_key.WriteValue(L"ReleaseId", L"1000");
  // override base::win::OSInfo
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin10Pro);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "10 OS Version 1000 (Build 15063.555)");
}

TEST_F(WebUIWindowsVersion, WinServer2016) {
  ubr_key.WriteValue(L"UBR", 1555);
  ubr_key.WriteValue(L"ReleaseId", L"1001");
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWinServer2016);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "Server OS Version 1001 (Build 17134.1555)");
}

TEST_F(WebUIWindowsVersion, Win81Pro) {
  ubr_key.WriteValue(L"UBR", 0UL);
  ubr_key.WriteValue(L"ReleaseId", L"1001");
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin81Pro);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "8.1 Version 1001 (Build 9600)");
}

TEST_F(WebUIWindowsVersion, WinServer2012R2) {
  ubr_key.WriteValue(L"UBR", 0UL);
  ubr_key.WriteValue(L"ReleaseId", L"1001");
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWinServer2012R2);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "Server 2012 R2 Version 1001 (Build 9600)");
}

TEST_F(WebUIWindowsVersion, Win7ProSP1) {
  ubr_key.WriteValue(L"UBR", 0UL);
  ubr_key.WriteValue(L"ReleaseId", L"1001");
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin7ProSP1);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "7 Service Pack 1 Version 1001 (Build 7601)");
}

TEST_F(WebUIWindowsVersion, Win7ProSP1NoReleaseId) {
  ubr_key.WriteValue(L"UBR", 0UL);
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin7ProSP1);
  EXPECT_EQ(VersionHandlerWindows::GetFullWindowsVersionForTesting(),
            "7 Service Pack 1 (Build 7601)");
}
