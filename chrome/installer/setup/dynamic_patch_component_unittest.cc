// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/dynamic_patch_component.h"

#include <windows.h>

#include <shlobj.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr char kManifestName[] = "Chrome Dynamic Patch (Inner)";
constexpr char kCrxId[] = "binbghhnflgglfabhjocbobkiignbgfh";
constexpr char kBaseVersion[] = "130.0.6723.70";
constexpr wchar_t kUserSid[] =
    L"S-1-5-21-2127521184-1604012920-1887927527-1001";

// A User Data directory. The component never accesses it, so it need not
// exist.
constexpr base::FilePath::CharType kUserDataDir[] = FILE_PATH_LITERAL(
    "C:\\Users\\User\\AppData\\Local\\Google\\Chrome\\User Data");

base::win::Sid GetUserSid() {
  return *base::win::Sid::FromSddlString(kUserSid);
}

// The capabilities through which processes in Chrome's AppContainer sandboxes
// are granted access to Chrome's installation.
constexpr wchar_t kChromeInstallFilesCapabilitySid[] =
    L"S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-"
    L"316187997-3786583170-1043257646";
constexpr wchar_t kLpacChromeInstallFilesCapabilitySid[] =
    L"S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-"
    L"4240214049-4028510897-3317428798";

// Returns a manifest for a patch to Chrome `base_version`.
base::DictValue MakeManifest(std::string_view base_version = kBaseVersion) {
  return base::DictValue()
      .Set("name", kManifestName)
      .Set("base_version", base_version);
}

// Returns the directory into which patches to kBaseVersion are installed for
// `sid` and `user_data_dir` when Chrome is installed in `target_path`.
base::FilePath GetExpectedDestinationRoot(const base::FilePath& target_path,
                                          const base::win::Sid& sid,
                                          const base::FilePath& user_data_dir) {
  const base::FilePath user_path_component =
      child_module::ComputeUserPathComponent(sid, user_data_dir);
  CHECK(!user_path_component.empty());
  return target_path.AppendASCII(kBaseVersion)
      .Append(child_module::kModulesDirName)
      .Append(user_path_component);
}

}  // namespace

// For a system-level install, the patch is installed for the user and the User
// Data directory given on the command line.
TEST(DynamicPatchComponentTest, CreateSystemLevel) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir.GetPath().AppendASCII(kBaseVersion)));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kUserSid, kUserSid);
  command_line.AppendSwitchPath(switches::kUdd, base::FilePath(kUserDataDir));
  std::unique_ptr<DynamicPatchComponent> component =
      DynamicPatchComponent::Create(command_line, installer_state);
  ASSERT_TRUE(component);

  ASSERT_TRUE(component->ReadManifest(MakeManifest()));
  EXPECT_THAT(
      component->DetermineDestinationRoot(installer_state, kCrxId),
      base::test::ValueIs(GetExpectedDestinationRoot(
          temp_dir.GetPath(), GetUserSid(), base::FilePath(kUserDataDir))));
}

// For a user-level install, the patch is installed for the current user.
TEST(DynamicPatchComponentTest, CreateUserLevel) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::USER_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir.GetPath().AppendASCII(kBaseVersion)));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchPath(switches::kUdd, base::FilePath(kUserDataDir));
  std::unique_ptr<DynamicPatchComponent> component =
      DynamicPatchComponent::Create(command_line, installer_state);
  ASSERT_TRUE(component);

  ASSERT_OK_AND_ASSIGN(const auto token,
                       base::win::AccessToken::FromCurrentProcess());
  ASSERT_TRUE(component->ReadManifest(MakeManifest()));
  EXPECT_THAT(
      component->DetermineDestinationRoot(installer_state, kCrxId),
      base::test::ValueIs(GetExpectedDestinationRoot(
          temp_dir.GetPath(), token.User(), base::FilePath(kUserDataDir))));
}

// A system-level install requires --user-sid identifying a user account.
TEST(DynamicPatchComponentTest, CreateRejectsMissingOrInvalidUserSid) {
  const InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  const struct {
    const char* description;
    const wchar_t* user_sid;  // nullptr to omit the switch.
  } kCases[] = {
      {"missing", nullptr},
      {"malformed", L"invalid-sid"},
      {"LocalSystem", L"S-1-5-18"},
      {"Everyone", L"S-1-1-0"},
      {"Domain Users", L"S-1-5-21-2127521184-1604012920-1887927527-513"},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.description);
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchPath(switches::kUdd, base::FilePath(kUserDataDir));
    if (test_case.user_sid) {
      command_line.AppendSwitchNative(switches::kUserSid, test_case.user_sid);
    }
    EXPECT_FALSE(DynamicPatchComponent::Create(command_line, installer_state));
  }
}

// --udd is required and must be an absolute path without ".." components.
TEST(DynamicPatchComponentTest, CreateRejectsInvalidUserDataDir) {
  const InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  const struct {
    const char* description;
    const base::FilePath::CharType* user_data_dir;  // nullptr to omit.
  } kCases[] = {
      {"missing", nullptr},
      {"empty", FILE_PATH_LITERAL("")},
      {"relative", FILE_PATH_LITERAL("User Data")},
      {"root-relative", FILE_PATH_LITERAL("\\User Data")},
      {"drive-relative", FILE_PATH_LITERAL("C:User Data")},
      {"parent-reference",
       FILE_PATH_LITERAL("C:\\Users\\User\\..\\Other\\User Data")},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.description);
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchNative(switches::kUserSid, kUserSid);
    if (test_case.user_data_dir) {
      command_line.AppendSwitchPath(switches::kUdd,
                                    base::FilePath(test_case.user_data_dir));
    }
    EXPECT_FALSE(DynamicPatchComponent::Create(command_line, installer_state));
  }
}

TEST(DynamicPatchComponentTest, ReadManifest) {
  DynamicPatchComponent component(GetUserSid(), base::FilePath(kUserDataDir));
  EXPECT_TRUE(component.ReadManifest(MakeManifest()));

  // The manifest must contain the component's name.
  EXPECT_FALSE(component.ReadManifest(
      base::DictValue().Set("base_version", kBaseVersion)));
  EXPECT_FALSE(component.ReadManifest(
      MakeManifest().Set("name", "Chrome Platform Runtime (Inner)")));

  // The manifest must contain a valid base version in canonical form.
  EXPECT_FALSE(
      component.ReadManifest(base::DictValue().Set("name", kManifestName)));
  EXPECT_FALSE(component.ReadManifest(MakeManifest().Set("base_version", 130)));
  EXPECT_FALSE(component.ReadManifest(MakeManifest("not_a_valid_version")));
  EXPECT_FALSE(component.ReadManifest(MakeManifest("0130.0.6723.70")));
}

// A patch is installed within the directory of the version that it patches,
// which must exist.
TEST(DynamicPatchComponentTest, DetermineDestinationRoot) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  DynamicPatchComponent component(GetUserSid(), base::FilePath(kUserDataDir));
  ASSERT_TRUE(component.ReadManifest(MakeManifest()));
  EXPECT_THAT(component.DetermineDestinationRoot(installer_state, kCrxId),
              base::test::ErrorIs(INSTALL_COMPONENT_INVALID_INPUT));

  ASSERT_TRUE(
      base::CreateDirectory(temp_dir.GetPath().AppendASCII(kBaseVersion)));
  EXPECT_THAT(
      component.DetermineDestinationRoot(installer_state, kCrxId),
      base::test::ValueIs(GetExpectedDestinationRoot(
          temp_dir.GetPath(), GetUserSid(), base::FilePath(kUserDataDir))));

  // Failure to compute the user path component (e.g., for a relative User Data
  // directory, which Create() rejects) is an internal error.
  DynamicPatchComponent relative_udd_component(
      GetUserSid(), base::FilePath(FILE_PATH_LITERAL("User Data")));
  ASSERT_TRUE(relative_udd_component.ReadManifest(MakeManifest()));
  EXPECT_THAT(
      relative_udd_component.DetermineDestinationRoot(installer_state, kCrxId),
      base::test::ErrorIs(INSTALL_COMPONENT_FAILED_INTERNAL));
}

// For a system-level install, the destination root does not inherit access
// rights from its parent. SYSTEM and Administrators are granted full control,
// and the user and Chrome's AppContainer capabilities are granted read and
// execute access, but not write access, to the root and its contents.
TEST(DynamicPatchComponentTest, InitializeDestinationRootSystemLevel) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath destination_root =
      temp_dir.GetPath().AppendASCII("root");
  ASSERT_TRUE(base::CreateDirectory(destination_root));

  DynamicPatchComponent component(GetUserSid(), base::FilePath(kUserDataDir));
  ASSERT_TRUE(component.ReadManifest(MakeManifest()));
  ASSERT_TRUE(
      component.InitializeDestinationRoot(installer_state, destination_root));

  ASSERT_OK_AND_ASSIGN(const auto sd,
                       base::win::SecurityDescriptor::FromFile(
                           destination_root, DACL_SECURITY_INFORMATION));
  // The DACL is protected ("P"), and each of its grants is inherited by files
  // ("OI") and directories ("CI") within the root. The user and capabilities
  // are granted FILE_GENERIC_READ | FILE_GENERIC_EXECUTE (0x1200a9). Windows
  // stores each grant of GENERIC_ALL as an ACE granting the equivalent
  // file-specific rights ("FA") to the root itself and an inherit-only ("IO")
  // ACE granting GENERIC_ALL ("GA") to its contents.
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION),
            base::StrCat(
                {L"D:PAI(A;;FA;;;SY)(A;OICIIO;GA;;;SY)",
                 L"(A;;FA;;;BA)(A;OICIIO;GA;;;BA)", L"(A;OICI;0x1200a9;;;",
                 kUserSid, L")", L"(A;OICI;0x1200a9;;;",
                 kChromeInstallFilesCapabilitySid, L")", L"(A;OICI;0x1200a9;;;",
                 kLpacChromeInstallFilesCapabilitySid, L")"}));

  // Failure to set the DACL is reported.
  EXPECT_FALSE(component.InitializeDestinationRoot(
      installer_state, temp_dir.GetPath().AppendASCII("nonexistent")));
}

// For a system-level install, a process in one of Chrome's AppContainer
// sandboxes (e.g., a renderer) running as the user can read and execute
// modules within the destination root only by virtue of its capabilities.
TEST(DynamicPatchComponentTest,
     InitializeDestinationRootGrantsAccessToAppContainerCapabilities) {
  // Only SYSTEM and Administrators can delete the destination root's contents
  // once its DACL has been set.
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP() << "Requires administrator rights.";
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath destination_root =
      temp_dir.GetPath().AppendASCII("root");
  ASSERT_TRUE(base::CreateDirectory(destination_root));
  // Setting the root's DACL replaces the inherited ACEs of its contents.
  const base::FilePath module =
      destination_root.AppendASCII("chrome_renderer.dll");
  ASSERT_TRUE(base::WriteFile(module, "dummy_dll_payload"));

  // Install for the current user so that an AppContainer token derived from
  // the current process's token is subject to the ACE granting access to the
  // user.
  ASSERT_OK_AND_ASSIGN(const auto token,
                       base::win::AccessToken::FromCurrentProcess(
                           /*impersonation=*/false, TOKEN_ALL_ACCESS));
  DynamicPatchComponent component(token.User(), base::FilePath(kUserDataDir));
  ASSERT_TRUE(component.ReadManifest(MakeManifest()));
  ASSERT_TRUE(
      component.InitializeDestinationRoot(installer_state, destination_root));

  // AccessCheck requires the owner and group.
  ASSERT_OK_AND_ASSIGN(
      auto module_sd,
      base::win::SecurityDescriptor::FromFile(
          module, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                      DACL_SECURITY_INFORMATION));

  ASSERT_OK_AND_ASSIGN(const auto package_sid, base::win::Sid::FromSddlString(
                                                   L"S-1-15-2-1-2-3-4-5-6-7"));
  ASSERT_OK_AND_ASSIGN(
      const auto chrome_install_files,
      base::win::Sid::FromSddlString(kChromeInstallFilesCapabilitySid));
  ASSERT_OK_AND_ASSIGN(
      const auto lpac_chrome_install_files,
      base::win::Sid::FromSddlString(kLpacChromeInstallFilesCapabilitySid));

  // Returns true if an AppContainer process with `capabilities` is granted
  // read and execute access to the module, or std::nullopt on error.
  auto can_read_and_execute = [&](base::span<const base::win::Sid> capabilities)
      -> std::optional<bool> {
    std::optional<base::win::AccessToken> app_container_token =
        token.CreateAppContainer(package_sid, capabilities, TOKEN_DUPLICATE);
    if (!app_container_token) {
      return std::nullopt;
    }
    std::optional<base::win::AccessToken> impersonation_token =
        app_container_token->DuplicateImpersonation(
            base::win::SecurityImpersonationLevel::kIdentification);
    if (!impersonation_token) {
      return std::nullopt;
    }
    std::optional<base::win::AccessCheckResult> result = module_sd.AccessCheck(
        *impersonation_token, FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
        base::win::SecurityObjectType::kFile);
    if (!result) {
      return std::nullopt;
    }
    return result->access_status;
  };

  EXPECT_EQ(
      can_read_and_execute(base::span_from_ref(lpac_chrome_install_files)),
      true);
  EXPECT_EQ(can_read_and_execute(base::span_from_ref(chrome_install_files)),
            true);
  EXPECT_EQ(can_read_and_execute({}), false);
}

// For a user-level install, the destination root is left to inherit its ACL
// from its parent.
TEST(DynamicPatchComponentTest, InitializeDestinationRootUserLevel) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::USER_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath destination_root =
      temp_dir.GetPath().AppendASCII("root");
  ASSERT_TRUE(base::CreateDirectory(destination_root));
  ASSERT_OK_AND_ASSIGN(const auto sd_before,
                       base::win::SecurityDescriptor::FromFile(
                           destination_root, DACL_SECURITY_INFORMATION));

  DynamicPatchComponent component(GetUserSid(), base::FilePath(kUserDataDir));
  ASSERT_TRUE(component.ReadManifest(MakeManifest()));
  EXPECT_TRUE(
      component.InitializeDestinationRoot(installer_state, destination_root));

  ASSERT_OK_AND_ASSIGN(const auto sd_after,
                       base::win::SecurityDescriptor::FromFile(
                           destination_root, DACL_SECURITY_INFORMATION));
  EXPECT_FALSE(sd_after.dacl_protected());
  EXPECT_EQ(sd_after.ToSddl(DACL_SECURITY_INFORMATION),
            sd_before.ToSddl(DACL_SECURITY_INFORMATION));
}

TEST(DynamicPatchComponentTest, IsUserSid) {
  // Valid standard domain / local user account.
  auto user_sid = base::win::Sid::FromSddlString(
      L"S-1-5-21-2127521184-1604012920-1887927527-1001");
  ASSERT_TRUE(user_sid.has_value());
  EXPECT_TRUE(IsUserSid(*user_sid));

  // Valid built-in Administrator account (RID 500).
  auto admin_user_sid = base::win::Sid::FromSddlString(
      L"S-1-5-21-2127521184-1604012920-1887927527-500");
  ASSERT_TRUE(admin_user_sid.has_value());
  EXPECT_TRUE(IsUserSid(*admin_user_sid));

  // Valid Azure AD / Entra ID user account.
  auto azure_ad_user_sid = base::win::Sid::FromSddlString(
      L"S-1-12-1-3236356646-1269869771-1072057250-1222535468");
  ASSERT_TRUE(azure_ad_user_sid.has_value());
  EXPECT_TRUE(IsUserSid(*azure_ad_user_sid));

  // Service accounts:
  // LocalSystem (S-1-5-18).
  EXPECT_FALSE(
      IsUserSid(base::win::Sid(base::win::WellKnownSid::kLocalSystem)));

  // LocalService (S-1-5-19).
  EXPECT_FALSE(
      IsUserSid(base::win::Sid(base::win::WellKnownSid::kLocalService)));

  // NetworkService (S-1-5-20).
  EXPECT_FALSE(
      IsUserSid(base::win::Sid(base::win::WellKnownSid::kNetworkService)));

  // NT Service (S-1-5-80-...).
  auto nt_service_sid = base::win::Sid::FromSddlString(L"S-1-5-80-12345-67890");
  ASSERT_TRUE(nt_service_sid.has_value());
  EXPECT_FALSE(IsUserSid(*nt_service_sid));

  // Groups and well-known SIDs:
  // Everyone / World (S-1-1-0).
  EXPECT_FALSE(IsUserSid(base::win::Sid(base::win::WellKnownSid::kWorld)));

  // Builtin Administrators group (S-1-5-32-544).
  EXPECT_FALSE(IsUserSid(
      base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators)));

  // Builtin Users group (S-1-5-32-545).
  EXPECT_FALSE(
      IsUserSid(base::win::Sid(base::win::WellKnownSid::kBuiltinUsers)));

  // Domain Users group (RID 513).
  auto domain_group_sid = base::win::Sid::FromSddlString(
      L"S-1-5-21-2127521184-1604012920-1887927527-513");
  ASSERT_TRUE(domain_group_sid.has_value());
  EXPECT_FALSE(IsUserSid(*domain_group_sid));

  // AppContainer (S-1-15-2-1).
  auto app_container_sid = base::win::Sid::FromSddlString(L"S-1-15-2-1");
  ASSERT_TRUE(app_container_sid.has_value());
  EXPECT_FALSE(IsUserSid(*app_container_sid));
}

}  // namespace installer
