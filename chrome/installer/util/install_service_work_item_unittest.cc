// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item.h"

#include <shlobj.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/strcat_win.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/install_service_work_item_impl.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr wchar_t kServiceName[] = L"InstallServiceWorkItemService";
constexpr wchar_t kServiceDisplayName[] = L"InstallServiceWorkItemService";
constexpr wchar_t kServiceDescription[] =
    L"InstallServiceWorkItemService is a test service";
constexpr uint32_t kServiceStartType = SERVICE_DEMAND_START;
constexpr base::FilePath::CharType kServiceProgramPath[] =
    FILE_PATH_LITERAL("c:\\windows\\SysWow64\\cmd.exe");
constexpr base::CommandLine::CharType kComServiceCmdLineArgs[] =
    FILE_PATH_LITERAL("com-service");

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

constexpr wchar_t kIidRegPath[] =
    L"Software\\Classes\\Interface\\{0F9A0C1C-A94A-4C0A-93C7-81330526AC7B}";
constexpr wchar_t kTypeLibRegPath[] =
    L"Software\\Classes\\TypeLib\\{0F9A0C1C-A94A-4C0A-93C7-81330526AC7B}";

// {4B565860-2646-4C5C-9A8E-27B91230BA2E}
constexpr GUID kIid2 = {0x4b565860,
                        0x2646,
                        0x4c5c,
                        {0x9a, 0x8e, 0x27, 0xb9, 0x12, 0x30, 0xba, 0x2e}};
const std::vector<GUID> kIids2 = {kIid2};

constexpr wchar_t kIid2RegPath[] =
    L"Software\\Classes\\Interface\\{4B565860-2646-4C5C-9A8E-27B91230BA2E}";
constexpr wchar_t kTypeLib2RegPath[] =
    L"Software\\Classes\\TypeLib\\{4B565860-2646-4C5C-9A8E-27B91230BA2E}";

constexpr base::span<const GUID> kNoIids;

}  // namespace

class InstallServiceWorkItemTest : public ::testing::Test {
 protected:
  static InstallServiceWorkItemImpl* GetImpl(InstallServiceWorkItem* item) {
    return item->impl_.get();
  }
  static bool IsServiceCorrectlyConfigured(InstallServiceWorkItem* item) {
    InstallServiceWorkItemImpl::ServiceConfig original_config;
    if (!GetImpl(item)->GetServiceConfig(&original_config))
      return false;

    InstallServiceWorkItemImpl::ServiceConfig new_config =
        GetImpl(item)->MakeUpgradeServiceConfig(original_config);
    return !GetImpl(item)->IsUpgradeNeeded(new_config);
  }

  static bool IsServiceGone(InstallServiceWorkItem* item) {
    if (!GetImpl(item)->OpenService()) {
      return true;
    }

    InstallServiceWorkItemImpl::ServiceConfig config;
    config.is_valid = true;

    // In order to determine whether the Service is in a "deleted" state, we
    // attempt to change just the display name in the service configuration.
    config.display_name = GetImpl(item)->GetCurrentServiceDisplayName();

    // If the service is deleted, `ChangeServiceConfig()` will return false.
    return !GetImpl(item)->ChangeServiceConfig(config);
  }

  static void ExpectIidCOMRegistrationCorrect(
      const GUID& iid,
      const std::wstring& iid_reg_path,
      const std::wstring& typelib_reg_path,
      const base::FilePath::CharType typelib_path[]) {
    base::win::RegKey key;
    std::wstring value;
    const std::wstring iid_string = base::win::WStringFromGUID(iid);

    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      EXPECT_EQ(ERROR_SUCCESS,
                key.Open(HKEY_LOCAL_MACHINE, iid_reg_path.c_str(),
                         KEY_READ | key_flag));
      EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
      EXPECT_EQ(base::StrCat({L"Interface ", iid_string}), value);

      EXPECT_EQ(
          ERROR_SUCCESS,
          key.Open(HKEY_LOCAL_MACHINE,
                   base::StrCat({iid_reg_path, L"\\ProxyStubClsid32"}).c_str(),
                   KEY_READ | key_flag));
      EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
      EXPECT_EQ(L"{00020424-0000-0000-C000-000000000046}", value);

      EXPECT_EQ(ERROR_SUCCESS,
                key.Open(HKEY_LOCAL_MACHINE,
                         base::StrCat({iid_reg_path, L"\\TypeLib"}).c_str(),
                         KEY_READ | key_flag));
      EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
      EXPECT_EQ(iid_string, value);
      EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"Version", &value));
      EXPECT_EQ(L"1.0", value);
    }

    // Check TypeLib registration.
    EXPECT_EQ(
        ERROR_SUCCESS,
        key.Open(HKEY_LOCAL_MACHINE,
                 base::StrCat({typelib_reg_path, L"\\1.0"}).c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
    EXPECT_EQ(base::StrCat({L"TypeLib for Interface ", iid_string}), value);

    EXPECT_EQ(
        ERROR_SUCCESS,
        key.Open(HKEY_LOCAL_MACHINE,
                 base::StrCat({typelib_reg_path, L"\\1.0\\0\\win32"}).c_str(),
                 KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
    EXPECT_EQ(typelib_path, value);

    EXPECT_EQ(
        ERROR_SUCCESS,
        key.Open(HKEY_LOCAL_MACHINE,
                 base::StrCat({typelib_reg_path, L"\\1.0\\0\\win64"}).c_str(),
                 KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
    EXPECT_EQ(typelib_path, value);
  }

  static void ExpectIidCOMRegistrationAbsent(
      const std::wstring& iid_reg_path,
      const std::wstring& typelib_reg_path) {
    base::win::RegKey key;
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      EXPECT_EQ(ERROR_FILE_NOT_FOUND,
                key.Open(HKEY_LOCAL_MACHINE, iid_reg_path.c_str(),
                         KEY_READ | key_flag));
    }
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
              key.Open(HKEY_LOCAL_MACHINE, typelib_reg_path.c_str(), KEY_READ));
  }

  static void ExpectServiceCOMRegistrationCorrect(
      const base::CommandLine& com_service_cmd_line_args,
      const base::FilePath::CharType typelib_path[]) {
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

    if (!com_service_cmd_line_args.GetArgumentsString().empty()) {
      EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"ServiceParameters", &value));
      EXPECT_EQ(com_service_cmd_line_args.GetArgumentsString(), value);
    } else {
      EXPECT_FALSE(key.HasValue(L"ServiceParameters"));
    }

    ExpectIidCOMRegistrationCorrect(kIid, kIidRegPath, kTypeLibRegPath,
                                    typelib_path);
  }

  static void ExpectServiceCOMRegistrationAbsent() {
    base::win::RegKey key;

    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
              key.Open(HKEY_LOCAL_MACHINE, kClsidRegPath, KEY_READ));
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
              key.Open(HKEY_LOCAL_MACHINE, kAppidRegPath, KEY_READ));
    ExpectIidCOMRegistrationAbsent(kIidRegPath, kTypeLibRegPath);
  }

  void TearDown() override {
    base::win::RegKey(HKEY_LOCAL_MACHINE, L"", KEY_READ | KEY_WOW64_32KEY)
        .DeleteKey(kProductRegPath);

    base::win::RegKey key(HKEY_LOCAL_MACHINE, L"", KEY_READ);
    key.DeleteKey(kClsidRegPath);
    key.DeleteKey(kAppidRegPath);
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, kIidRegPath, key_flag);
      installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, kIid2RegPath, key_flag);
    }
    key.DeleteKey(kTypeLibRegPath);
    key.DeleteKey(kTypeLib2RegPath);
  }

  // Set up InstallDetails for a system-level install.
  const install_static::ScopedInstallDetails install_details_{true};
  bool preexisting_clientstate_key_ = false;
};

TEST_F(InstallServiceWorkItemTest, Do_MultiSzToVector) {
  constexpr wchar_t kZeroMultiSz[] = L"";
  std::vector<wchar_t> vec =
      InstallServiceWorkItemImpl::MultiSzToVector(kZeroMultiSz);

  EXPECT_TRUE(base::span(vec) == kZeroMultiSz);
  EXPECT_EQ(vec.size(), std::size(kZeroMultiSz));

  vec = InstallServiceWorkItemImpl::MultiSzToVector(nullptr);
  EXPECT_TRUE(vec.empty());

  constexpr wchar_t kRpcMultiSz[] = L"RPCSS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kRpcMultiSz);
  EXPECT_TRUE(base::span(vec) == kRpcMultiSz);
  EXPECT_EQ(vec.size(), std::size(kRpcMultiSz));

  constexpr wchar_t kMultiSz[] = L"RPCSS\0LSASS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kMultiSz);
  EXPECT_TRUE(base::span(vec) == kMultiSz);
  EXPECT_EQ(vec.size(), std::size(kMultiSz));
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstall) {
  if (!::IsUserAnAdmin()) {
    // Calling ::OpenSCManager requires an admin user.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  base::HistogramTester histogram_tester;
  base::CommandLine com_service_cmd_line_args(base::CommandLine::NO_PROGRAM);
  com_service_cmd_line_args.AppendArgNative(kComServiceCmdLineArgs);

  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      com_service_cmd_line_args, kProductRegPath, kClsids, kIids, kNoIids);

  ASSERT_FALSE(InstallServiceWorkItem::IsComServiceInstalled(kClsid));
  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(InstallServiceWorkItem::IsComServiceInstalled(kClsid));
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  ExpectServiceCOMRegistrationCorrect(com_service_cmd_line_args,
                                      kServiceProgramPath);

  item->Rollback();

  EXPECT_TRUE(IsServiceGone(item.get()));
  ExpectServiceCOMRegistrationAbsent();
  EXPECT_FALSE(InstallServiceWorkItem::IsComServiceInstalled(kClsid));

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Setup.Install.SCM."),
              testing::Not(testing::IsEmpty()));
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstallThenDeleteService) {
  if (!::IsUserAnAdmin()) {
    // Calling ::OpenSCManager requires an admin user.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      base::CommandLine(base::CommandLine::NO_PROGRAM), kProductRegPath,
      kClsids, kIids, kNoIids);

  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  EXPECT_TRUE(InstallServiceWorkItem::DeleteService(
      kServiceName, kProductRegPath, kClsids, kIids));

  // Check to make sure that the item shows that the service is deleted.
  EXPECT_TRUE(IsServiceGone(item.get()));
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeNoChanges) {
  if (!::IsUserAnAdmin()) {
    // Calling ::OpenSCManager requires an admin user.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      base::CommandLine(base::CommandLine::NO_PROGRAM), kProductRegPath,
      kClsids, kIids, kNoIids);
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  // Same command line:
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      base::CommandLine(base::CommandLine::NO_PROGRAM), kProductRegPath,
      kClsids, kIids, kNoIids);
  EXPECT_TRUE(item_upgrade->Do());

  // Check to make sure that no upgrade happened, and both the old and new items
  // show that the service is correctly configured.
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item_upgrade.get()));

  item_upgrade->Rollback();

  // Check to make sure that no rollback happened, and both the old and new
  // items show that the service is correctly configured.
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item_upgrade.get()));

  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());

  // Check to make sure that both items show that the service is deleted.
  EXPECT_TRUE(IsServiceGone(item.get()));
  EXPECT_TRUE(IsServiceGone(item_upgrade.get()));
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeChangedCmdLineStartTypeCOMArgs) {
  if (!::IsUserAnAdmin()) {
    // Calling ::OpenSCManager requires admin access.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  base::CommandLine com_service_cmd_line_args(base::CommandLine::NO_PROGRAM);
  com_service_cmd_line_args.AppendArgNative(kComServiceCmdLineArgs);

  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      com_service_cmd_line_args, kProductRegPath, kClsids, kIids, kNoIids);
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  ExpectServiceCOMRegistrationCorrect(com_service_cmd_line_args,
                                      kServiceProgramPath);
  EXPECT_EQ(GetImpl(item.get())->GetCurrentServiceDescription(),
            kServiceDescription);

  // New command line and start type.
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription,
      SERVICE_AUTO_START,
      base::CommandLine::FromString(L"NewCmd.exe arg1 arg2"),
      base::CommandLine(base::CommandLine::NO_PROGRAM), kProductRegPath,
      kClsids, kIids, kNoIids);
  EXPECT_TRUE(item_upgrade->Do());

  // Check to make sure the upgrade happened, and the new item shows that the
  // service is correctly configured, while the old item shows that the service
  // is not correctly configured.
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item_upgrade.get()));
  EXPECT_EQ(GetImpl(item_upgrade.get())->GetCurrentServiceDescription(),
            kServiceDescription);
  ExpectServiceCOMRegistrationCorrect(
      base::CommandLine(base::CommandLine::NO_PROGRAM), L"NewCmd.exe");
  EXPECT_FALSE(IsServiceCorrectlyConfigured(item.get()));

  item_upgrade->Rollback();
  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  // Check to make sure the rollback happened, and the old item shows that it is
  // correctly configured, while the new item shows that the service is not
  // correctly configured.
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  ExpectServiceCOMRegistrationCorrect(com_service_cmd_line_args,
                                      kServiceProgramPath);
  EXPECT_FALSE(IsServiceCorrectlyConfigured(item_upgrade.get()));

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());

  // Check to make sure that both items show that the service is deleted.
  EXPECT_TRUE(IsServiceGone(item.get()));
  EXPECT_TRUE(IsServiceGone(item_upgrade.get()));
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeChangedIIDs) {
  if (!::IsUserAnAdmin()) {
    // Calling ::OpenSCManager requires admin access.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  base::CommandLine com_service_cmd_line_args(base::CommandLine::NO_PROGRAM);
  com_service_cmd_line_args.AppendArgNative(kComServiceCmdLineArgs);

  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      com_service_cmd_line_args, kProductRegPath, kClsids, kIids, kNoIids);
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  ExpectServiceCOMRegistrationCorrect(com_service_cmd_line_args,
                                      kServiceProgramPath);
  ExpectIidCOMRegistrationAbsent(kIid2RegPath, kTypeLib2RegPath);

  // Upgrade to kIids2, deleting previous kIids.
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      com_service_cmd_line_args, kProductRegPath, kClsids, kIids2, kIids);
  EXPECT_TRUE(item_upgrade->Do());

  // Check that old IID is gone and new IID is registered.
  ExpectIidCOMRegistrationAbsent(kIidRegPath, kTypeLibRegPath);
  ExpectIidCOMRegistrationCorrect(kIid2, kIid2RegPath, kTypeLib2RegPath,
                                  kServiceProgramPath);

  // Rollback upgrade.
  item_upgrade->Rollback();

  // Check that old IID is restored and new IID is gone.
  ExpectIidCOMRegistrationCorrect(kIid, kIidRegPath, kTypeLibRegPath,
                                  kServiceProgramPath);
  ExpectIidCOMRegistrationAbsent(kIid2RegPath, kTypeLib2RegPath);

  // Re-apply upgrade so both old and new IIDs are involved when deleting.
  item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::FilePath(kServiceProgramPath)),
      com_service_cmd_line_args, kProductRegPath, kClsids, kIids2, kIids);
  EXPECT_TRUE(item_upgrade->Do());
  ExpectIidCOMRegistrationAbsent(kIidRegPath, kTypeLibRegPath);
  ExpectIidCOMRegistrationCorrect(kIid2, kIid2RegPath, kTypeLib2RegPath,
                                  kServiceProgramPath);

  // Delete service.
  EXPECT_TRUE(InstallServiceWorkItem::DeleteService(
      kServiceName, kProductRegPath, kClsids, kIids2));

  EXPECT_TRUE(IsServiceGone(item.get()));
  ExpectServiceCOMRegistrationAbsent();
  ExpectIidCOMRegistrationAbsent(kIid2RegPath, kTypeLib2RegPath);
}

TEST_F(InstallServiceWorkItemTest, Do_ServiceName) {
  if (!::IsUserAnAdmin()) {
    // Writing to HKLM requires an admin user.
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName, kServiceDescription, kServiceStartType,
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      base::CommandLine(base::FilePath(kServiceProgramPath)), kProductRegPath,
      kClsids, kIids, kNoIids);

  EXPECT_EQ(kServiceName, GetImpl(item.get())->GetCurrentServiceName());
  EXPECT_EQ(base::StrCat({kServiceDisplayName, L" (",
                          GetImpl(item.get())->GetCurrentServiceName(), L")"}),
            GetImpl(item.get())->GetCurrentServiceDisplayName());

  EXPECT_TRUE(GetImpl(item.get())->CreateAndSetServiceName());
  EXPECT_NE(kServiceName, GetImpl(item.get())->GetCurrentServiceName());
  EXPECT_EQ(0UL,
            GetImpl(item.get())->GetCurrentServiceName().find(kServiceName));
  EXPECT_EQ(base::StrCat({kServiceDisplayName, L" (",
                          GetImpl(item.get())->GetCurrentServiceName(), L")"}),
            GetImpl(item.get())->GetCurrentServiceDisplayName());

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, kProductRegPath,
                                    KEY_WRITE | KEY_WOW64_32KEY));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kServiceName));
}

}  // namespace installer
