// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/installed_applications.h"

#include <map>

#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/msi_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr wchar_t kRegistryKeyPathPrefix[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";

struct CommonInfo {
  std::wstring product_id;
  bool is_system_component;
  bool is_microsoft_published;
  std::wstring display_name;
  std::wstring uninstall_string;
};

struct InstallLocationApplicationInfo {
  CommonInfo common_info;
  std::wstring install_location;
};

struct MsiApplicationInfo {
  CommonInfo common_info;
  std::vector<std::wstring> components;
};

class MockMsiUtil : public MsiUtil {
 public:
  MockMsiUtil(const std::map<std::wstring, std::vector<std::wstring>>&
                  component_paths_map)
      : component_paths_map_(component_paths_map) {}

  bool GetMsiComponentPaths(
      const std::wstring& product_guid,
      const std::wstring& user_sid,
      std::vector<std::wstring>* component_paths) const override {
    auto iter = component_paths_map_->find(product_guid);
    if (iter == component_paths_map_->end())
      return false;

    *component_paths = iter->second;
    return true;
  }

 private:
  const raw_ref<const std::map<std::wstring, std::vector<std::wstring>>>
      component_paths_map_;
};

class TestInstalledApplications : public InstalledApplications {
 public:
  explicit TestInstalledApplications(std::unique_ptr<MsiUtil> msi_util)
      : InstalledApplications(std::move(msi_util)) {}
};

class InstalledApplicationsTest : public testing::Test {
 public:
  InstalledApplicationsTest() = default;

  InstalledApplicationsTest(const InstalledApplicationsTest&) = delete;
  InstalledApplicationsTest& operator=(const InstalledApplicationsTest&) =
      delete;

  ~InstalledApplicationsTest() override = default;

  // Overrides HKLM and HKCU to prevent real keys from messing with the tests.
  void OverrideRegistry() {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void AddCommonInfo(const CommonInfo& common_info,
                     base::win::RegKey* registry_key) {
    registry_key->WriteValue(L"SystemComponent",
                             common_info.is_system_component ? 1 : 0);
    registry_key->WriteValue(L"UninstallString",
                             common_info.uninstall_string.c_str());
    if (common_info.is_microsoft_published)
      registry_key->WriteValue(L"Publisher", L"Microsoft Corporation");
    registry_key->WriteValue(L"DisplayName", common_info.display_name.c_str());
  }

  void AddFakeApplication(const MsiApplicationInfo& application_info) {
    const std::wstring registry_key_path =
        kRegistryKeyPathPrefix + application_info.common_info.product_id;
    base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                   KEY_WRITE);

    AddCommonInfo(application_info.common_info, &registry_key);

    component_paths_map_.insert(
        {application_info.common_info.product_id, application_info.components});
  }

  void AddFakeApplication(
      const InstallLocationApplicationInfo& application_info) {
    const std::wstring registry_key_path =
        kRegistryKeyPathPrefix + application_info.common_info.product_id;
    base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                   KEY_WRITE);

    AddCommonInfo(application_info.common_info, &registry_key);

    registry_key.WriteValue(L"InstallLocation",
                            application_info.install_location.c_str());
  }

  TestInstalledApplications& installed_applications() {
    return *installed_applications_;
  }

  void InitializeInstalledApplications() {
    installed_applications_ = std::make_unique<TestInstalledApplications>(
        std::make_unique<MockMsiUtil>(component_paths_map_));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;

  std::unique_ptr<TestInstalledApplications> installed_applications_;

  std::map<std::wstring, std::vector<std::wstring>> component_paths_map_;
};

}  // namespace

// Checks that registry entries with invalid information are skipped.
TEST_F(InstalledApplicationsTest, InvalidEntries) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocation[] = L"c:\\application files\\application\\";

  InstallLocationApplicationInfo kTestCases[] = {
      {
          {
              L"Is SystemComponent",
              true,
              false,
              kValidDisplayName,
              kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Is Microsoft published",
              false,
              true,
              kValidDisplayName,
              kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Missing DisplayName",
              false,
              false,
              L"",
              kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Missing UninstallString",
              false,
              false,
              kValidDisplayName,
              L"",
          },
          kInstallLocation,
      },
  };

  ASSERT_NO_FATAL_FAILURE(OverrideRegistry());

  for (const auto& test_case : kTestCases)
    AddFakeApplication(test_case);

  InitializeInstalledApplications();

  // None of the invalid entries were picked up.
  const base::FilePath valid_child_file =
      base::FilePath(kInstallLocation).Append(L"file.dll");
  std::vector<InstalledApplications::ApplicationInfo> applications;
  EXPECT_FALSE(installed_applications().GetInstalledApplications(
      valid_child_file, &applications));
}

// Tests InstalledApplications on a valid entry with an InstallLocation.
TEST_F(InstalledApplicationsTest, InstallLocation) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocation[] = L"c:\\application files\\application\\";

  InstallLocationApplicationInfo kTestCase = {
      {
          L"Completely valid",
          false,
          false,
          kValidDisplayName,
          kValidUninstallString,
      },
      kInstallLocation,
  };

  ASSERT_NO_FATAL_FAILURE(OverrideRegistry());

  AddFakeApplication(kTestCase);

  InitializeInstalledApplications();

  // Child file path.
  const base::FilePath valid_child_file =
      base::FilePath(kInstallLocation).Append(L"file.dll");
  std::vector<InstalledApplications::ApplicationInfo> applications;
  EXPECT_TRUE(installed_applications().GetInstalledApplications(
      valid_child_file, &applications));
  ASSERT_EQ(1u, applications.size());
  EXPECT_EQ(kTestCase.common_info.display_name, applications[0].name);
  EXPECT_EQ(HKEY_CURRENT_USER, applications[0].registry_root);
  EXPECT_FALSE(applications[0].registry_key_path.empty());
  EXPECT_EQ(0u, applications[0].registry_wow64_access);

  // Non-child file path.
  const base::FilePath invalid_child_file(
      L"c:\\application files\\another application\\test.dll");
  EXPECT_FALSE(installed_applications().GetInstalledApplications(
      invalid_child_file, &applications));
}

// Tests InstalledApplications on a valid MSI entry.
TEST_F(InstalledApplicationsTest, Msi) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";

  MsiApplicationInfo kTestCase = {
      {
          L"Completely valid",
          false,
          false,
          kValidDisplayName,
          kValidUninstallString,
      },
      {
          L"c:\\application files\\application\\file1.dll",
          L"c:\\application files\\application\\file2.dll",
          L"c:\\application files\\application\\sub\\file3.dll",
          L"c:\\windows\\system32\\file4.dll",
      },
  };

  ASSERT_NO_FATAL_FAILURE(OverrideRegistry());

  AddFakeApplication(kTestCase);

  InitializeInstalledApplications();

  // Checks that all the files match the application.
  for (const auto& component : kTestCase.components) {
    std::vector<InstalledApplications::ApplicationInfo> applications;
    EXPECT_TRUE(installed_applications().GetInstalledApplications(
        base::FilePath(component), &applications));
    ASSERT_EQ(1u, applications.size());
    EXPECT_EQ(kTestCase.common_info.display_name, applications[0].name);
    EXPECT_EQ(HKEY_CURRENT_USER, applications[0].registry_root);
    EXPECT_FALSE(applications[0].registry_key_path.empty());
    EXPECT_EQ(0u, applications[0].registry_wow64_access);
  }

  // Any other file shouldn't work.
  const base::FilePath invalid_child_file(
      L"c:\\application files\\another application\\test.dll");
  std::vector<InstalledApplications::ApplicationInfo> applications;
  EXPECT_FALSE(installed_applications().GetInstalledApplications(
      invalid_child_file, &applications));
}

// Checks that if a file matches an InstallLocation and an MSI component, only
// the MSI application will be considered.
TEST_F(InstalledApplicationsTest, PrioritizeMsi) {
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocationDisplayName[] = L"InstallLocation DisplayName";
  const wchar_t kMsiDisplayName[] = L"Msi DisplayName";
  const wchar_t kInstallLocation[] = L"c:\\application files\\application\\";
  const wchar_t kMsiComponent[] =
      L"c:\\application files\\application\\file.dll";

  InstallLocationApplicationInfo kInstallLocationFakeApplication = {
      {
          L"GUID1",
          false,
          false,
          kInstallLocationDisplayName,
          kValidUninstallString,
      },
      kInstallLocation,
  };

  MsiApplicationInfo kMsiFakeApplication = {
      {
          L"GUID2",
          false,
          false,
          kMsiDisplayName,
          kValidUninstallString,
      },
      {
          kMsiComponent,
      },
  };

  ASSERT_NO_FATAL_FAILURE(OverrideRegistry());

  AddFakeApplication(kInstallLocationFakeApplication);
  AddFakeApplication(kMsiFakeApplication);

  InitializeInstalledApplications();

  std::vector<InstalledApplications::ApplicationInfo> applications;
  EXPECT_TRUE(installed_applications().GetInstalledApplications(
      base::FilePath(kMsiComponent), &applications));
  ASSERT_EQ(1u, applications.size());
  EXPECT_NE(kInstallLocationDisplayName, applications[0].name);
  EXPECT_EQ(kMsiDisplayName, applications[0].name);
}

// Tests that if 2 entries with conflicting InstallLocation exist, both are
// ignored.
TEST_F(InstalledApplicationsTest, ConflictingInstallLocations) {
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kDisplayName1[] = L"DisplayName1";
  const wchar_t kDisplayName2[] = L"DisplayName2";
  const wchar_t kInstallLocationParent[] = L"c:\\application files\\company\\";
  const wchar_t kInstallLocationChild[] =
      L"c:\\application files\\company\\application";
  const wchar_t kFile[] =
      L"c:\\application files\\company\\application\\file.dll";

  InstallLocationApplicationInfo kFakeApplication1 = {
      {
          L"GUID1",
          false,
          false,
          kDisplayName1,
          kValidUninstallString,
      },
      kInstallLocationParent,
  };
  InstallLocationApplicationInfo kFakeApplication2 = {
      {
          L"GUID2",
          false,
          false,
          kDisplayName2,
          kValidUninstallString,
      },
      kInstallLocationChild,
  };

  ASSERT_NO_FATAL_FAILURE(OverrideRegistry());

  AddFakeApplication(kFakeApplication1);
  AddFakeApplication(kFakeApplication2);

  InitializeInstalledApplications();

  std::vector<InstalledApplications::ApplicationInfo> applications;
  EXPECT_FALSE(installed_applications().GetInstalledApplications(
      base::FilePath(kFile), &applications));
}

// This test ensures that each uninstall registry key is only read once, and
// thus no applications are picked up twice.
// This is possible if the same registry key is requested for both the 32-bit
// and 64-bit view but either that key is shared between the views, or the host
// OS is 32-bit, and there is no 64-bit view.
TEST_F(InstalledApplicationsTest, NoDuplicates) {
  InitializeInstalledApplications();

  auto applications = installed_applications().applications_;
  std::sort(std::begin(applications), std::end(applications));
  EXPECT_EQ(std::end(applications),
            base::ranges::adjacent_find(
                applications, std::equal_to<>(), [](const auto& app) {
                  return std::tie(app.name, app.registry_root,
                                  app.registry_key_path,
                                  app.registry_wow64_access);
                }));
}
