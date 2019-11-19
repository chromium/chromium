// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_util.h"

#include <Aclapi.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using ::testing::_;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

namespace {

struct ScopedSecurityData {
  ~ScopedSecurityData() {
    if (everyone_sid)
      FreeSid(everyone_sid);
    if (acl)
      LocalFree(acl);
    if (sec_descr)
      LocalFree(sec_descr);
  }
  PSID everyone_sid = nullptr;
  PACL acl = nullptr;
  PSECURITY_DESCRIPTOR sec_descr = nullptr;
};

void CreateDeleteOnlySecurity(ScopedSecurityData* sec_data,
                              SECURITY_ATTRIBUTES* sa) {
  // Create a well-known SID for the Everyone group.
  SID_IDENTIFIER_AUTHORITY SIDAuthWorld{SECURITY_WORLD_SID_AUTHORITY};
  ASSERT_TRUE(AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0,
                                       0, 0, 0, 0, 0, 0,
                                       &sec_data->everyone_sid));

  // Initialize an EXPLICIT_ACCESS structure for an ACE.
  // The ACE will allow Everyone DELETE access to the key.
  EXPLICIT_ACCESS ea;
  ZeroMemory(&ea, sizeof(ea));
  ea.grfAccessPermissions = DELETE;
  ea.grfAccessMode = SET_ACCESS;
  ea.grfInheritance = NO_INHERITANCE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = static_cast<LPTSTR>(sec_data->everyone_sid);

  // Create a new ACL that contains the ACE.
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            SetEntriesInAcl(1, &ea, nullptr, &sec_data->acl));

  // Initialize a security descriptor.
  sec_data->sec_descr = static_cast<PSECURITY_DESCRIPTOR>(
      LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
  ASSERT_TRUE(sec_data->sec_descr);

  ASSERT_TRUE(InitializeSecurityDescriptor(sec_data->sec_descr,
                                           SECURITY_DESCRIPTOR_REVISION));

  // Add the ACL to the security descriptor.
  ASSERT_TRUE(SetSecurityDescriptorDacl(sec_data->sec_descr,
                                        TRUE,  // bDaclPresent flag
                                        sec_data->acl,
                                        FALSE));  // not a default DACL

  // Initialize a security attributes structure.
  sa->nLength = sizeof(*sa);
  sa->lpSecurityDescriptor = sec_data->sec_descr;
  sa->bInheritHandle = FALSE;
}

}  // namespace

class MockRegistryValuePredicate : public InstallUtil::RegistryValuePredicate {
 public:
  MOCK_CONST_METHOD1(Evaluate, bool(const std::wstring&));
};

class InstallUtilTest : public testing::Test {
 protected:
  InstallUtilTest() {}

  void SetUp() override { ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides()); }

  void ResetRegistryOverrides() {
    registry_override_manager_.reset(
        new registry_util::RegistryOverrideManager);
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_->OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_->OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

 private:
  std::unique_ptr<registry_util::RegistryOverrideManager>
      registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(InstallUtilTest);
};

TEST_F(InstallUtilTest, ComposeCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  std::pair<std::wstring, std::wstring> params[] = {
    std::make_pair(std::wstring(L""), std::wstring(L"")),
    std::make_pair(std::wstring(L""), std::wstring(L"--do-something --silly")),
    std::make_pair(std::wstring(L"spam.exe"), std::wstring(L"")),
    std::make_pair(std::wstring(L"spam.exe"),
                   std::wstring(L"--do-something --silly")),
  };
  for (std::pair<std::wstring, std::wstring>& param : params) {
    InstallUtil::ComposeCommandLine(param.first, param.second, &command_line);
    EXPECT_EQ(param.first, command_line.GetProgram().value());
    if (param.second.empty()) {
      EXPECT_TRUE(command_line.GetSwitches().empty());
    } else {
      EXPECT_EQ(2U, command_line.GetSwitches().size());
      EXPECT_TRUE(command_line.HasSwitch("do-something"));
      EXPECT_TRUE(command_line.HasSwitch("silly"));
    }
  }
}

TEST_F(InstallUtilTest, GetCurrentDate) {
  std::wstring date(InstallUtil::GetCurrentDate());
  EXPECT_EQ(8u, date.length());
  if (date.length() == 8) {
    // For an invalid date value, SystemTimeToFileTime will fail.
    // We use this to validate that we have a correct date string.
    SYSTEMTIME systime = {0};
    FILETIME ft = {0};
    // Just to make sure our assumption holds.
    EXPECT_FALSE(SystemTimeToFileTime(&systime, &ft));
    // Now fill in the values from our string.
    systime.wYear = _wtoi(date.substr(0, 4).c_str());
    systime.wMonth = _wtoi(date.substr(4, 2).c_str());
    systime.wDay = _wtoi(date.substr(6, 2).c_str());
    // Check if they make sense.
    EXPECT_TRUE(SystemTimeToFileTime(&systime, &ft));
  }
}

TEST_F(InstallUtilTest, DeleteRegistryKeyIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring parent_key_path(L"SomeKey\\ToDelete");
  std::wstring child_key_path(parent_key_path);
  child_key_path.append(L"\\ChildKey\\WithAValue");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  // Nothing to delete if the keys aren't even there.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
    EXPECT_EQ(InstallUtil::NOT_FOUND,
              InstallUtil::DeleteRegistryKeyIf(
                  root, parent_key_path, child_key_path,
                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Parent exists, but not child: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_SET_VALUE).Valid());
    EXPECT_EQ(InstallUtil::NOT_FOUND,
              InstallUtil::DeleteRegistryKeyIf(
                  root, parent_key_path, child_key_path,
                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Child exists, but no value: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_TRUE(RegKey(root, child_key_path.c_str(), KEY_SET_VALUE).Valid());
    EXPECT_EQ(InstallUtil::NOT_FOUND,
              InstallUtil::DeleteRegistryKeyIf(
                  root, parent_key_path, child_key_path,
                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Value exists, but doesn't match: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(value_name, L"foosball!"));
    EXPECT_EQ(InstallUtil::NOT_FOUND,
              InstallUtil::DeleteRegistryKeyIf(
                  root, parent_key_path, child_key_path,
                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Value exists, and matches: delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(value_name, value));
    EXPECT_EQ(InstallUtil::DELETED,
              InstallUtil::DeleteRegistryKeyIf(
                  root, parent_key_path, child_key_path,
                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Default value exists and matches: delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(NULL, value));
    EXPECT_EQ(InstallUtil::DELETED, InstallUtil::DeleteRegistryKeyIf(
                                        root, parent_key_path, child_key_path,
                                        WorkItem::kWow64Default, NULL, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }
}

TEST_F(InstallUtilTest, DeleteRegistryValueIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring key_path(L"SomeKey\\ToDelete");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Nothing to delete if the key isn't even there.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_EQ(InstallUtil::NOT_FOUND,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default,
                                                   value_name, pred));
      EXPECT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Key exists, but no value: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_TRUE(RegKey(root, key_path.c_str(), KEY_SET_VALUE).Valid());
      EXPECT_EQ(InstallUtil::NOT_FOUND,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default,
                                                   value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Value exists, but doesn't match: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, L"foosball!"));
      EXPECT_EQ(InstallUtil::NOT_FOUND,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default,
                                                   value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_TRUE(RegKey(root, key_path.c_str(),
                         KEY_QUERY_VALUE).HasValue(value_name));
    }

    // Value exists, and matches: delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, value));
      EXPECT_EQ(InstallUtil::DELETED,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default,
                                                   value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(RegKey(root, key_path.c_str(),
                          KEY_QUERY_VALUE).HasValue(value_name));
    }
  }

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Default value matches: delete using empty string.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(L"", value));
      EXPECT_EQ(InstallUtil::DELETED,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default, L"",
                                                   pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(RegKey(root, key_path.c_str(),
                          KEY_QUERY_VALUE).HasValue(L""));
    }
  }

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Default value matches: delete using NULL.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(L"", value));
      EXPECT_EQ(InstallUtil::DELETED,
                InstallUtil::DeleteRegistryValueIf(root, key_path.c_str(),
                                                   WorkItem::kWow64Default,
                                                   NULL, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(RegKey(root, key_path.c_str(),
                          KEY_QUERY_VALUE).HasValue(L""));
    }
  }
}

TEST_F(InstallUtilTest, ValueEquals) {
  InstallUtil::ValueEquals pred(L"howdy");

  EXPECT_FALSE(pred.Evaluate(L""));
  EXPECT_FALSE(pred.Evaluate(L"Howdy"));
  EXPECT_FALSE(pred.Evaluate(L"howdy!"));
  EXPECT_FALSE(pred.Evaluate(L"!howdy"));
  EXPECT_TRUE(pred.Evaluate(L"howdy"));
}

// A matcher that returns true if its argument (a string) matches a given
// base::FilePath.
MATCHER_P(EqPathIgnoreCase, value, "") {
  return base::FilePath::CompareEqualIgnoreCase(arg, value.value());
}

TEST_F(InstallUtilTest, ProgramCompare) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  const base::FilePath some_long_dir(
      test_dir.GetPath().Append(L"Some Long Directory Name"));
  const base::FilePath expect(some_long_dir.Append(L"file.txt"));
  const base::FilePath expect_upcase(some_long_dir.Append(L"FILE.txt"));
  const base::FilePath other(some_long_dir.Append(L"otherfile.txt"));

  // Tests where the expected file doesn't exist.

  // Paths don't match.
  EXPECT_FALSE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + expect_upcase.value() + L"\""));

  // Tests where the expected file exists.
  static const char data[] = "data";
  ASSERT_TRUE(base::CreateDirectory(some_long_dir));
  ASSERT_NE(-1, base::WriteFile(expect, data, base::size(data) - 1));
  // Paths don't match.
  EXPECT_FALSE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + expect_upcase.value() + L"\""));

  // Test where strings don't match, but the same file is indicated.
  std::wstring short_expect;
  DWORD short_len = GetShortPathName(expect.value().c_str(),
                                     base::WriteInto(&short_expect, MAX_PATH),
                                     MAX_PATH);
  ASSERT_NE(static_cast<DWORD>(0), short_len);
  ASSERT_GT(static_cast<DWORD>(MAX_PATH), short_len);
  short_expect.resize(short_len);
  ASSERT_THAT(short_expect, Not(EqPathIgnoreCase(expect)));
  EXPECT_TRUE(InstallUtil::ProgramCompare(expect).Evaluate(
      L"\"" + short_expect + L"\""));
}

TEST_F(InstallUtilTest, AddDowngradeVersion) {
  install_static::ScopedInstallDetails system_install(true);
  const HKEY kRoot = HKEY_LOCAL_MACHINE;
  RegKey(kRoot, install_static::GetClientStateKeyPath().c_str(),
         KEY_SET_VALUE | KEY_WOW64_32KEY);
  std::unique_ptr<WorkItemList> list;

  base::Version current_version("1.1.1.1");
  base::Version higer_new_version("1.1.1.2");
  base::Version lower_new_version_1("1.1.1.0");
  base::Version lower_new_version_2("1.1.0.0");

  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Upgrade should not create the value.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &current_version,
                                             higer_new_version, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Downgrade should create the value.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &current_version,
                                             lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Multiple downgrades should not change the value.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &lower_new_version_1,
                                             lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());
}

TEST_F(InstallUtilTest, DeleteDowngradeVersion) {
  install_static::ScopedInstallDetails system_install(true);
  const HKEY kRoot = HKEY_LOCAL_MACHINE;
  RegKey(kRoot, install_static::GetClientStateKeyPath().c_str(),
         KEY_SET_VALUE | KEY_WOW64_32KEY);
  std::unique_ptr<WorkItemList> list;

  base::Version current_version("1.1.1.1");
  base::Version higer_new_version("1.1.1.2");
  base::Version lower_new_version_1("1.1.1.0");
  base::Version lower_new_version_2("1.1.0.0");

  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &current_version,
                                             lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Upgrade should not delete the value if it still lower than the version that
  // downgrade from.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &lower_new_version_2,
                                             lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Repair should not delete the value.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &lower_new_version_1,
                                             lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Fully upgrade should delete the value.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &lower_new_version_1,
                                             higer_new_version, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Fresh install should delete the value if it exists.
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, &current_version,
                                             lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());
  list.reset(WorkItem::CreateWorkItemList());
  InstallUtil::AddUpdateDowngradeVersionItem(kRoot, nullptr,
                                             lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());
}

TEST(DeleteRegistryKeyTest, DeleteAccessRightIsEnoughToDelete) {
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_CURRENT_USER));

  ScopedSecurityData sec_data;
  SECURITY_ATTRIBUTES sa;
  ASSERT_NO_FATAL_FAILURE(CreateDeleteOnlySecurity(&sec_data, &sa));

  HKEY sub_key = nullptr;
  ASSERT_EQ(ERROR_SUCCESS, RegCreateKeyEx(HKEY_CURRENT_USER, L"TestKey", 0,
                                          nullptr, REG_OPTION_NON_VOLATILE,
                                          DELETE | WorkItem::kWow64Default, &sa,
                                          &sub_key, nullptr));
  RegCloseKey(sub_key);

  EXPECT_TRUE(InstallUtil::DeleteRegistryKey(HKEY_CURRENT_USER, L"TestKey",
                                             WorkItem::kWow64Default));
}

TEST_F(InstallUtilTest, GetToastActivatorRegistryPath) {
  base::string16 toast_activator_reg_path =
      InstallUtil::GetToastActivatorRegistryPath();
  EXPECT_FALSE(toast_activator_reg_path.empty());

  // Confirm that the string is a path followed by a GUID.
  size_t guid_begin = toast_activator_reg_path.find('{');
  EXPECT_NE(std::wstring::npos, guid_begin);
  ASSERT_GE(guid_begin, 1u);
  EXPECT_EQ(L'\\', toast_activator_reg_path[guid_begin - 1]);

  // A GUID has the form "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}".
  constexpr size_t kGuidLength = 38;
  EXPECT_EQ(kGuidLength, toast_activator_reg_path.length() - guid_begin);

  EXPECT_EQ('}', toast_activator_reg_path.back());
}

TEST_F(InstallUtilTest, GuidToSquid) {
  ASSERT_EQ(InstallUtil::GuidToSquid(L"EDA620E3-AA98-3846-B81E-3493CB2E0E02"),
            L"3E026ADE89AA64838BE14339BCE2E020");
}
