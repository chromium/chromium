// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kApp1Id[] = "app1_id";
const char kApp1Name[] = "app1 name";
const char kApp1Url[] = "https://app1.com/%s";
const char kApp2Id[] = "app2_id";
const char kApp2Name[] = "app2 name";
const char kApp2Url[] = "https://app2.com/%s";
}  // namespace

using custom_handlers::ProtocolHandler;

namespace web_app {

class WebAppProtocolHandlerRegistrationWinTest : public testing::Test {
 protected:
  WebAppProtocolHandlerRegistrationWinTest() {}

  void SetUp() override {
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
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

  Profile* GetProfile() { return profile_; }
  ProfileManager* profile_manager() {
    return testing_profile_manager_->profile_manager();
  }
  TestingProfileManager* testing_profile_manager() {
    return testing_profile_manager_.get();
  }

  // Ensures that URLAssociations entries are created for a given protocol.
  // "HKEY_CURRENT_USER\Software\<prog_id>\Capabilities\URLAssociations\<protocol>".
  bool ProgIdRegisteredForProtocol(const std::string& protocol,
                                   const webapps::AppId& app_id,
                                   Profile* profile) {
    std::wstring prog_id = GetProgIdForApp(profile->GetPath(), app_id);

    std::wstring url_associations_key_name(install_static::GetRegistryPath());
    url_associations_key_name.append(ShellUtil::kRegAppProtocolHandlers);
    url_associations_key_name.push_back(base::FilePath::kSeparators[0]);
    url_associations_key_name.append(prog_id);
    url_associations_key_name.append(L"\\Capabilities");
    url_associations_key_name.append(L"\\URLAssociations");

    base::win::RegKey key;
    std::wstring value;
    bool association_exists =
        key.Open(HKEY_CURRENT_USER, url_associations_key_name.c_str(),
                 KEY_READ) == ERROR_SUCCESS;

    bool entry_matches = (key.ReadValue(base::UTF8ToWide(protocol).c_str(),
                                        &value) == ERROR_SUCCESS) &&
                         value == prog_id;

    return association_exists && entry_matches;
  }

  void AddAndVerifyProtocolAssociations(const webapps::AppId& app_id,
                                        const std::string& app_name,
                                        const std::string& app_url,
                                        Profile* profile,
                                        const char* app_name_extension) {
    std::string sanitized_app_name(app_name);
    sanitized_app_name.append(app_name_extension);
    base::FilePath expected_app_launcher_path =
        GetLauncherPathForApp(profile, app_id, sanitized_app_name);

    apps::ProtocolHandlerInfo handler1_info;
    handler1_info.protocol = "mailto";
    handler1_info.url = GURL(app_url);

    apps::ProtocolHandlerInfo handler2_info;
    handler2_info.protocol = "web+test";
    handler2_info.url = GURL(app_url);

    base::RunLoop run_loop;
    RegisterProtocolHandlersWithOs(
        app_id, app_name, profile->GetPath(), {handler1_info, handler2_info},
        base::BindLambdaForTesting([&](Result result) {
          EXPECT_EQ(Result::kOk, result);
          run_loop.Quit();
        }));
    run_loop.Run();

    base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
        GetProgIdForApp(profile->GetPath(), app_id));

    EXPECT_TRUE(base::PathExists(registered_app_path));
    EXPECT_EQ(registered_app_path, expected_app_launcher_path);

    // Both protocols should have been registered with the OS registry.
    EXPECT_TRUE(
        ProgIdRegisteredForProtocol(handler1_info.protocol, app_id, profile));
    EXPECT_TRUE(
        ProgIdRegisteredForProtocol(handler2_info.protocol, app_id, profile));
  }

  // Gets the launcher file path for |sanitized_app_name|.
  base::FilePath GetAppSpecificLauncherFilePath(
      const std::string& sanitized_app_name) {
    base::FilePath app_specific_launcher_filepath(
        base::ASCIIToWide(sanitized_app_name));
    app_specific_launcher_filepath =
        app_specific_launcher_filepath.AddExtension(L"exe");
    return app_specific_launcher_filepath;
  }

  // Returns the expected app launcher path inside the subdirectory for
  // |app_id|.
  base::FilePath GetLauncherPathForApp(Profile* profile,
                                       const webapps::AppId app_id,
                                       const std::string& sanitized_app_name) {
    base::FilePath web_app_dir(GetOsIntegrationResourcesDirectoryForApp(
        profile->GetPath(), app_id, GURL()));
    base::FilePath app_specific_launcher_filepath =
        GetAppSpecificLauncherFilePath(sanitized_app_name);

    return web_app_dir.Append(app_specific_launcher_filepath);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(WebAppProtocolHandlerRegistrationWinTest,
       AddAndVerifyProtocolAssociations) {
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, GetProfile(),
                                   "");
}

TEST_F(WebAppProtocolHandlerRegistrationWinTest,
       RegisterMultipleHandlersWithSameScheme) {
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, GetProfile(),
                                   "");
  AddAndVerifyProtocolAssociations(kApp2Id, kApp2Name, kApp2Url, GetProfile(),
                                   "");
}

// When an app is registered in one profile, and then is registered in a second
// profile, the disambiguation dialog for both app registrations should include
// the profile name, e.g., "app name (Default)" and "app name (Profile 2)".
TEST_F(WebAppProtocolHandlerRegistrationWinTest,
       RegisterProtocolHandlersForWebAppIn2Profiles) {
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, GetProfile(),
                                   "");

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, profile2,
                                   " (Profile 2)");

  ShellUtil::ApplicationInfo app_info = ShellUtil::GetApplicationInfoForProgId(
      GetProgIdForApp(GetProfile()->GetPath(), kApp1Id));

  ASSERT_FALSE(app_info.application_name.empty());
  // Profile 1's app name should now include the profile in the name.
  EXPECT_EQ(base::WideToUTF8(app_info.application_name), "app1 name (Default)");

  // Profile 1's app_launcher should include the profile in its name.
  base::FilePath profile1_app_specific_launcher_path =
      GetAppSpecificLauncherFilePath("app1 name (Default)");
  base::FilePath profile1_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(GetProfile()->GetPath(), kApp1Id));
  EXPECT_EQ(profile1_launcher_path.BaseName(),
            profile1_app_specific_launcher_path);

  // Verify that the app is still registered for "mailto" and "web+test" in
  // profile 1.
  EXPECT_TRUE(ProgIdRegisteredForProtocol("mailto", kApp1Id, GetProfile()));
  EXPECT_TRUE(ProgIdRegisteredForProtocol("web+test", kApp1Id, GetProfile()));
}

// When an app is registered in two profiles, and then unregistered in one of
// them, the remaining registration should no longer be profile-specific. It
// should not have the profile name in app_launcher executable name, or the
// registered app name.
TEST_F(WebAppProtocolHandlerRegistrationWinTest,
       UnRegisterProtocolHandlersForWebAppIn2Profiles) {
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, GetProfile(),
                                   "");
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(GetProfile()->GetPath(), kApp1Id));

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, profile2,
                                   " (Profile 2)");

  base::RunLoop run_loop;
  UnregisterProtocolHandlersWithOs(
      kApp1Id, GetProfile()->GetPath(),
      base::BindLambdaForTesting([&](Result result) {
        EXPECT_EQ(Result::kOk, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));

  // Verify that "(Profile 2)" was removed from the web app launcher and
  // protocol association registry entries.
  ShellUtil::ApplicationInfo app_info = ShellUtil::GetApplicationInfoForProgId(
      GetProgIdForApp(profile2->GetPath(), kApp1Id));
  ASSERT_FALSE(app_info.application_name.empty());

  // Profile 2's app name should no longer include the profile in the name.
  EXPECT_EQ(base::WideToUTF8(app_info.application_name), kApp1Name);

  // Profile 2's app_launcher should no longer include the profile in its name.
  base::FilePath profile2_app_specific_launcher_path =
      GetAppSpecificLauncherFilePath(kApp1Name);
  base::FilePath profile2_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(profile2->GetPath(), kApp1Id));
  EXPECT_EQ(profile2_launcher_path.BaseName(),
            profile2_app_specific_launcher_path);

  // Verify that the app is still registered for "mailto" and "web+test" in
  // profile 2.
  EXPECT_TRUE(ProgIdRegisteredForProtocol("mailto", kApp1Id, profile2));
  EXPECT_TRUE(ProgIdRegisteredForProtocol("web+test", kApp1Id, profile2));
}

// Register protocol handlers, and verify that unregistering removes the
// registry settings and the app-specific launcher.
TEST_F(WebAppProtocolHandlerRegistrationWinTest,
       UnregisterProtocolHandlersForWebApp) {
  AddAndVerifyProtocolAssociations(kApp1Id, kApp1Name, kApp1Url, GetProfile(),
                                   "");
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(
          GetProgIdForApp(GetProfile()->GetPath(), kApp1Id));

  base::RunLoop run_loop;
  UnregisterProtocolHandlersWithOs(
      kApp1Id, GetProfile()->GetPath(),
      base::BindLambdaForTesting([&](Result result) {
        EXPECT_EQ(Result::kOk, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));
  EXPECT_FALSE(ProgIdRegisteredForProtocol("mailto", kApp1Id, GetProfile()));
  EXPECT_FALSE(ProgIdRegisteredForProtocol("web+test", kApp1Id, GetProfile()));

  ShellUtil::ApplicationInfo app_info = ShellUtil::GetApplicationInfoForProgId(
      GetProgIdForApp(GetProfile()->GetPath(), kApp1Id));
  EXPECT_TRUE(app_info.application_name.empty());
}

}  // namespace web_app
