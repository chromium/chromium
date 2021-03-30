// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_service_work_item_impl.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr wchar_t kServiceName[] = L"InstallServiceWorkItemService";
constexpr wchar_t kServiceDisplayName[] = L"InstallServiceWorkItemService";
constexpr base::FilePath::CharType kServiceProgramPath[] =
    FILE_PATH_LITERAL("c:\\windows\\SysWow64\\cmd.exe");

constexpr wchar_t kProductRegPath[] =
    L"Software\\ChromiumTestInstallServiceWorkItem";

// {76EDE292-9C33-4A09-9B3A-3B880DF64440}
constexpr GUID kClsid = {0x76ede292,
                         0x9c33,
                         0x4a09,
                         {0x9b, 0x3a, 0x3b, 0x88, 0xd, 0xf6, 0x44, 0x40}};
const std::vector<GUID> kClsids = {kClsid};

constexpr wchar_t kClsidRegPath[] =
    L"Software\\Classes\\CLSID\\{76EDE292-9C33-4A09-9B3A-3B880DF64440}";
constexpr wchar_t kAppidRegPath[] =
    L"Software\\Classes\\AppId\\{76EDE292-9C33-4A09-9B3A-3B880DF64440}";

// {0F9A0C1C-A94A-4C0A-93C7-81330526AC7B}
constexpr GUID kIid = {0xf9a0c1c,
                       0xa94a,
                       0x4c0a,
                       {0x93, 0xc7, 0x81, 0x33, 0x5, 0x26, 0xac, 0x7b}};
const std::vector<GUID> kIids = {kIid};

#define IID_REGISTRY_PATH \
  L"Software\\Classes\\Interface\\{0F9A0C1C-A94A-4C0A-93C7-81330526AC7B}"
constexpr wchar_t kIidPSRegPath[] = IID_REGISTRY_PATH L"\\ProxyStubClsid32";
constexpr wchar_t kIidTLBRegPath[] = IID_REGISTRY_PATH L"\\TypeLib";
#define TYPELIB_REGISTRY_PATH \
  L"Software\\Classes\\TypeLib\\{0F9A0C1C-A94A-4C0A-93C7-81330526AC7B}"
constexpr wchar_t kTypeLibWin32RegPath[] =
    TYPELIB_REGISTRY_PATH L"\\1.0\\0\\win32";
constexpr wchar_t kTypeLibWin64RegPath[] =
    TYPELIB_REGISTRY_PATH L"\\1.0\\0\\win64";

}  // namespace

class InstallServiceWorkItemTest : public ::testing::Test {
 protected:
  static InstallServiceWorkItemImpl* GetImpl(InstallServiceWorkItem* item) {
    DCHECK(item);
    return item->impl_.get();
  }
  static bool IsServiceCorrectlyConfigured(InstallServiceWorkItem* item) {
    DCHECK(item);
    InstallServiceWorkItemImpl::ServiceConfig config;
    if (!GetImpl(item)->GetServiceConfig(&config))
      return false;

    return GetImpl(item)->IsServiceCorrectlyConfigured(config);
  }

  void TearDown() override {
    base::win::RegKey(HKEY_LOCAL_MACHINE, L"", KEY_READ | KEY_WOW64_32KEY)
        .DeleteKey(kProductRegPath);

    base::win::RegKey key(HKEY_LOCAL_MACHINE, L"", KEY_READ);
    key.DeleteKey(kClsidRegPath);
    key.DeleteKey(kAppidRegPath);
    key.DeleteKey(IID_REGISTRY_PATH);
    key.DeleteKey(TYPELIB_REGISTRY_PATH);
  }

  // Set up InstallDetails for a system-level install.
  const install_static::ScopedInstallDetails install_details_{true};
  bool preexisting_clientstate_key_ = false;
};

TEST_F(InstallServiceWorkItemTest, Do_MultiSzToVector) {
  constexpr wchar_t kZeroMultiSz[] = L"";
  std::vector<wchar_t> vec =
      InstallServiceWorkItemImpl::MultiSzToVector(kZeroMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kZeroMultiSz, sizeof(kZeroMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kZeroMultiSz));

  vec = InstallServiceWorkItemImpl::MultiSzToVector(nullptr);
  EXPECT_TRUE(vec.empty());

  constexpr wchar_t kRpcMultiSz[] = L"RPCSS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kRpcMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kRpcMultiSz, sizeof(kRpcMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kRpcMultiSz));

  constexpr wchar_t kMultiSz[] = L"RPCSS\0LSASS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kMultiSz, sizeof(kMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kMultiSz));
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstall) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);

  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  base::win::RegKey key;
  std::wstring value;

  // Check CLSID registration.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kClsidRegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"AppID", &value));
  EXPECT_EQ(base::win::WStringFromGUID(kClsid), value);

  // Check AppId registration.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kAppidRegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"LocalService", &value));
  EXPECT_EQ(kServiceName, value);

  // Check IID registration.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kIidPSRegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"{00020424-0000-0000-C000-000000000046}", value);

  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kIidTLBRegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(base::win::WStringFromGUID(kIid), value);
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"Version", &value));
  EXPECT_EQ(L"1.0", value);

  // Check TypeLib registration.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kTypeLibWin32RegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(kServiceProgramPath, value);

  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kTypeLibWin64RegPath, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(kServiceProgramPath, value);

  item->Rollback();
  EXPECT_FALSE(GetImpl(item.get())->OpenService());
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_LOCAL_MACHINE, kClsidRegPath, KEY_READ));
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_LOCAL_MACHINE, kAppidRegPath, KEY_READ));
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_LOCAL_MACHINE, IID_REGISTRY_PATH, KEY_READ));
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_LOCAL_MACHINE, TYPELIB_REGISTRY_PATH, KEY_READ));
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstallThenDeleteService) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);

  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  EXPECT_TRUE(InstallServiceWorkItem::DeleteService(
      kServiceName, kProductRegPath, kClsids, kIids));
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeNoChanges) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  // Same command line:
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);
  EXPECT_TRUE(item_upgrade->Do());

  item_upgrade->Rollback();
  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeChangedCmdLine) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  // New command line.
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine::FromString(L"NewCmd.exe arg1 arg2"), kProductRegPath,
      kClsids, kIids);
  EXPECT_TRUE(item_upgrade->Do());

  item_upgrade->Rollback();
  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  EXPECT_FALSE(IsServiceCorrectlyConfigured(item_upgrade.get()));

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());
}

TEST_F(InstallServiceWorkItemTest, Do_ServiceName) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids);

  EXPECT_STREQ(kServiceName,
               GetImpl(item.get())->GetCurrentServiceName().c_str());
  EXPECT_STREQ(
      base::StringPrintf(L"%ls (%ls)", kServiceDisplayName,
                         GetImpl(item.get())->GetCurrentServiceName().c_str())
          .c_str(),
      GetImpl(item.get())->GetCurrentServiceDisplayName().c_str());

  EXPECT_TRUE(GetImpl(item.get())->CreateAndSetServiceName());
  EXPECT_STRNE(kServiceName,
               GetImpl(item.get())->GetCurrentServiceName().c_str());
  EXPECT_EQ(0UL,
            GetImpl(item.get())->GetCurrentServiceName().find(kServiceName));
  EXPECT_STREQ(
      base::StringPrintf(L"%ls (%ls)", kServiceDisplayName,
                         GetImpl(item.get())->GetCurrentServiceName().c_str())
          .c_str(),
      GetImpl(item.get())->GetCurrentServiceDisplayName().c_str());

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, kProductRegPath,
                                    KEY_WRITE | KEY_WOW64_32KEY));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kServiceName));
}

}  // namespace installer
