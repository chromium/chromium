// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include <set>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

apps::FileHandlers GetFileHandlersWithFileExtensions(
    const std::vector<std::string>& file_extensions) {
  apps::FileHandlers file_handlers;
  for (const auto& file_extension : file_extensions) {
    apps::FileHandler file_handler;
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.file_extensions.insert(file_extension);
    file_handler.accept.push_back(accept_entry);
    file_handlers.push_back(file_handler);
  }
  return file_handlers;
}

}  // namespace

namespace web_app {

constexpr char kAppName[] = "app name";

class WebAppFileHandlerRegistrationWinTest : public testing::Test {
 protected:
  WebAppFileHandlerRegistrationWinTest() {}

  void SetUp() override {
    // Set up fake windows registry
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ =
        testing_profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
  }
  void TearDown() override {
    profile_ = nullptr;
    testing_profile_manager_->DeleteAllTestingProfiles();
  }

  Profile* profile() { return profile_; }
  ProfileManager* profile_manager() {
    return testing_profile_manager_->profile_manager();
  }
  TestingProfileManager* testing_profile_manager() {
    return testing_profile_manager_.get();
  }
  const AppId& app_id() const { return app_id_; }

  // Returns true if Chrome extension with AppId |app_id| has its corresponding
  // prog_id registered in Windows registry to handle files with extension
  // |file_ext|, false otherwise.
  bool ProgIdRegisteredForFileExtension(const std::string& file_ext,
                                        const AppId& app_id,
                                        Profile* profile) {
    base::string16 key_name(ShellUtil::kRegClasses);
    key_name.push_back(base::FilePath::kSeparators[0]);
    key_name.append(base::UTF8ToUTF16(file_ext));
    key_name.push_back(base::FilePath::kSeparators[0]);
    key_name.append(ShellUtil::kRegOpenWithProgids);
    base::win::RegKey key;
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ));
    base::string16 prog_id = GetProgIdForApp(profile->GetPath(), app_id);
    return key.ReadValue(prog_id.c_str(), &value) == ERROR_SUCCESS &&
           value == L"";
  }

  void AddAndVerifyFileAssociations(Profile* profile,
                                    const std::string& app_name,
                                    const char* app_name_extension) {
    std::string sanitized_app_name(app_name);
    sanitized_app_name.append(app_name_extension);
    base::FilePath expected_app_launcher_path =
        GetLauncherPathForApp(profile, app_id(), sanitized_app_name);
    apps::FileHandlers file_handlers =
        GetFileHandlersWithFileExtensions({".txt", ".doc"});

    RegisterFileHandlersWithOs(app_id(), app_name, profile, file_handlers);

    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
        GetProgIdForApp(profile->GetPath(), app_id()));
    EXPECT_TRUE(base::PathExists(registered_app_path));
    EXPECT_EQ(registered_app_path, expected_app_launcher_path);
    // .txt and .doc should have |app_name| in their Open With lists.
    EXPECT_TRUE(ProgIdRegisteredForFileExtension(".txt", app_id(), profile));
    EXPECT_TRUE(ProgIdRegisteredForFileExtension(".doc", app_id(), profile));
  }

  // Gets the launcher file path for |sanitized_app_name|. If not
  // on Win7, the name will have the ".exe" extension.
  base::FilePath GetAppSpecificLauncherFilePath(
      const std::string& sanitized_app_name) {
    base::FilePath app_specific_launcher_filepath(
        base::ASCIIToUTF16(sanitized_app_name));
    if (base::win::GetVersion() > base::win::Version::WIN7) {
      app_specific_launcher_filepath =
          app_specific_launcher_filepath.AddExtension(L"exe");
    }
    return app_specific_launcher_filepath;
  }

  // Returns the expected app launcher path inside the subdirectory for
  // |app_id|.
  base::FilePath GetLauncherPathForApp(Profile* profile,
                                       const AppId app_id,
                                       const std::string& sanitized_app_name) {
    base::FilePath web_app_dir(GetOsIntegrationResourcesDirectoryForApp(
        profile->GetPath(), app_id, GURL()));
    base::FilePath app_specific_launcher_filepath =
        GetAppSpecificLauncherFilePath(sanitized_app_name);

    return web_app_dir.Append(app_specific_launcher_filepath);
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir temp_version_dir_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  const AppId app_id_ = "app_id";
};

// Test various attributes of ProgIds returned by GetAppIdForApp.
TEST_F(WebAppFileHandlerRegistrationWinTest, GetProgIdForApp) {
  // Create a long app_id and verify that the prog id is less
  // than 39 characters, and only contains alphanumeric characters and
  // non leading '.'s See
  // https://docs.microsoft.com/en-us/windows/win32/com/-progid--key.
  AppId app_id1("app_id12345678901234567890123456789012345678901234");
  constexpr unsigned int kMaxProgIdLen = 39;
  base::string16 prog_id1 = GetProgIdForApp(profile()->GetPath(), app_id1);
  EXPECT_LE(prog_id1.length(), kMaxProgIdLen);
  for (auto itr = prog_id1.begin(); itr != prog_id1.end(); itr++)
    EXPECT_TRUE(std::isalnum(*itr) || (*itr == '.' && itr != prog_id1.begin()));
  AppId app_id2("different_appid");
  // Check that different app ids in the same profile have different
  // prog ids.
  EXPECT_NE(prog_id1, GetProgIdForApp(profile()->GetPath(), app_id2));

  // Create a different profile, and verify that the prog id for the same
  // app_id in a different profile is different.
  TestingProfile profile2;
  EXPECT_NE(prog_id1, GetProgIdForApp(profile2.GetPath(), app_id1));
}

TEST_F(WebAppFileHandlerRegistrationWinTest, RegisterFileHandlersForWebApp) {
  AddAndVerifyFileAssociations(profile(), kAppName, "");
}

// When an app is registered in one profile, and then is registered in a second
// profile, the open with context menu items for both app registrations should
// include the profile name, e.g., "app name (Default)" and "app name (Profile
// 2)".
TEST_F(WebAppFileHandlerRegistrationWinTest,
       RegisterFileHandlersForWebAppIn2Profiles) {
  AddAndVerifyFileAssociations(profile(), kAppName, "");

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  AddAndVerifyFileAssociations(profile2, kAppName, " (Profile 2)");

  ShellUtil::FileAssociationsAndAppName file_associations_and_app_name(
      ShellUtil::GetFileAssociationsAndAppName(
          GetProgIdForApp(profile()->GetPath(), app_id())));
  ASSERT_FALSE(file_associations_and_app_name.app_name.empty());
  // Profile 1's app name should now include the profile in the name.
  std::string app_name_str =
      base::UTF16ToUTF8(file_associations_and_app_name.app_name);
  EXPECT_EQ(app_name_str, "app name (Default)");
  // Profile 1's app_launcher should include the profile in its name.
  base::FilePath profile1_app_specific_launcher_path =
      GetAppSpecificLauncherFilePath("app name (Default)");
  base::FilePath profile1_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile()->GetPath(), app_id()));
  EXPECT_EQ(profile1_launcher_path.BaseName(),
            profile1_app_specific_launcher_path);
  // Verify that the app is still registered for ".txt" and ".doc" in profile 1.
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".txt", app_id(), profile()));
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".doc", app_id(), profile()));
}

// Test that we don't use the gaia name in the file association app name, but
// rather, just the local profile name.
TEST_F(WebAppFileHandlerRegistrationWinTest,
       RegisterFileHandlersForWebAppIn2ProfilesWithGaiaName) {
  AddAndVerifyFileAssociations(profile(), kAppName, "");

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(
      storage.GetProfileAttributesWithPath(profile2->GetPath(), &entry));
  entry->SetGAIAName(base::ASCIIToUTF16("gaia user"));
  AddAndVerifyFileAssociations(profile2, kAppName, " (Profile 2)");
}

// When an app is registered in two profiles, and then unregistered in one of
// them, the remaining registration should no longer be profile-specific. It
// should not have the profile name in app_launcher executable name, or the
// registered app name.
TEST_F(WebAppFileHandlerRegistrationWinTest,
       UnRegisterFileHandlersForWebAppIn2Profiles) {
  AddAndVerifyFileAssociations(profile(), kAppName, "");
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile()->GetPath(), app_id()));

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  AddAndVerifyFileAssociations(profile2, kAppName, " (Profile 2)");

  UnregisterFileHandlersWithOs(app_id(), profile());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));
  // Verify that "(Profile 2)" was removed from the web app launcher and
  // file association registry entries.
  ShellUtil::FileAssociationsAndAppName file_associations_and_app_name =
      ShellUtil::GetFileAssociationsAndAppName(
          GetProgIdForApp(profile2->GetPath(), app_id()));
  ASSERT_FALSE(file_associations_and_app_name.app_name.empty());
  // Profile 2's app name should no longer include the profile in the name.
  std::string app_name_str =
      base::UTF16ToUTF8(file_associations_and_app_name.app_name);
  EXPECT_EQ(app_name_str, kAppName);
  // Profile 2's app_launcher should no longer include the profile in its name.
  base::FilePath profile2_app_specific_launcher_path =
      GetAppSpecificLauncherFilePath(kAppName);
  base::FilePath profile2_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile2->GetPath(), app_id()));
  EXPECT_EQ(profile2_launcher_path.BaseName(),
            profile2_app_specific_launcher_path);
  // Verify that the app is still registered for ".txt" and ".doc" in profile 2.
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".txt", app_id(), profile2));
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".doc", app_id(), profile2));
}

// When an app is registered in three profiles, and then unregistered in one of
// them, the remaining registrations should not change.
TEST_F(WebAppFileHandlerRegistrationWinTest,
       UnRegisterFileHandlersForWebAppIn3Profiles) {
  AddAndVerifyFileAssociations(profile(), kAppName, "");
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile()->GetPath(), app_id()));

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  AddAndVerifyFileAssociations(profile2, kAppName, " (Profile 2)");

  Profile* profile3 =
      testing_profile_manager()->CreateTestingProfile("Profile 3");
  ASSERT_EQ(3u, storage.GetNumberOfProfiles());
  AddAndVerifyFileAssociations(profile3, kAppName, " (Profile 3)");

  UnregisterFileHandlersWithOs(app_id(), profile());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));
  // Verify that "(Profile 2)" was not removed from the web app launcher and
  // file association registry entries.
  ShellUtil::FileAssociationsAndAppName file_associations_and_app_name2(
      ShellUtil::GetFileAssociationsAndAppName(
          GetProgIdForApp(profile2->GetPath(), app_id())));
  ASSERT_FALSE(file_associations_and_app_name2.app_name.empty());
  // Profile 2's app name should still include the profile name in its name.
  std::string app_name_str =
      base::UTF16ToUTF8(file_associations_and_app_name2.app_name);
  EXPECT_EQ(app_name_str, "app name (Profile 2)");

  // Profile 3's app name should still include the profile name in its name.
  ShellUtil::FileAssociationsAndAppName file_associations_and_app_name3(
      ShellUtil::GetFileAssociationsAndAppName(
          GetProgIdForApp(profile3->GetPath(), app_id())));
  ASSERT_FALSE(file_associations_and_app_name3.app_name.empty());
  // Profile 2's app name should still include the profile in the name.
  app_name_str = base::UTF16ToUTF8(file_associations_and_app_name3.app_name);
  EXPECT_EQ(app_name_str, "app name (Profile 3)");
}

TEST_F(WebAppFileHandlerRegistrationWinTest, UnregisterFileHandlersForWebApp) {
  // Register file handlers, and then verify that unregistering removes
  // the registry settings and the app-specific launcher.
  AddAndVerifyFileAssociations(profile(), kAppName, "");
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile()->GetPath(), app_id()));

  UnregisterFileHandlersWithOs(app_id(), profile());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));
  EXPECT_FALSE(ProgIdRegisteredForFileExtension(".txt", app_id(), profile()));
  EXPECT_FALSE(ProgIdRegisteredForFileExtension(".doc", app_id(), profile()));

  ShellUtil::FileAssociationsAndAppName file_associations_and_app_name =
      ShellUtil::GetFileAssociationsAndAppName(
          GetProgIdForApp(profile()->GetPath(), app_id()));
  EXPECT_TRUE(file_associations_and_app_name.app_name.empty());
}

// Test that invalid file name characters in app_name are replaced with '_'.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameWithInvalidChars) {
  // '*' is an invalid char in Windows file names, so it should be replaced
  // with '_'.
  std::string app_name("app*name");
  base::FilePath app_specific_launcher_path =
      GetLauncherPathForApp(profile(), app_id(), "app_name");
  apps::FileHandlers file_handlers =
      GetFileHandlersWithFileExtensions({".txt"});

  RegisterFileHandlersWithOs(app_id(), app_name, profile(), file_handlers);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile()->GetPath(), app_id()));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

// Test that an app name that is a reserved filename on Windows has '_'
// prepended to it when used as a filename for its launcher.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameIsReservedFilename) {
  // "con" is a reserved filename on Windows, so it should have '_' prepended.
  std::string app_name("con");
  base::FilePath app_specific_launcher_path =
      GetLauncherPathForApp(profile(), app_id(), "_con");
  apps::FileHandlers file_handlers =
      GetFileHandlersWithFileExtensions({".txt"});

  RegisterFileHandlersWithOs(app_id(), app_name, profile(), file_handlers);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile()->GetPath(), app_id()));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

// Test that an app name containing '.' characters has them replaced with '_' on
// Windows 7 when used as a filename for its launcher.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameContainsDot) {
  // "some.app.name" should become "some_app_name" on Windows 7.
  std::string app_name("some.app.name");
  base::FilePath app_specific_launcher_path = GetLauncherPathForApp(
      profile(), app_id(),
      base::win::GetVersion() > base::win::Version::WIN7 ? "some.app.name"
                                                         : "some_app_name");
  apps::FileHandlers file_handlers =
      GetFileHandlersWithFileExtensions({".txt"});

  RegisterFileHandlersWithOs(app_id(), app_name, profile(), file_handlers);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile()->GetPath(), app_id()));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

}  // namespace web_app
