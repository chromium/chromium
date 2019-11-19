// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for master preferences related methods.

#include "chrome/installer/util/master_preferences.h"

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "rlz/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A helper class to set the "GoogleUpdateIsMachine" environment variable.
class ScopedGoogleUpdateIsMachine {
 public:
  explicit ScopedGoogleUpdateIsMachine(const char* value)
      : env_(base::Environment::Create()) {
    env_->SetVar("GoogleUpdateIsMachine", value);
  }

  ~ScopedGoogleUpdateIsMachine() {
    env_->UnSetVar("GoogleUpdateIsMachine");
  }

 private:
  std::unique_ptr<base::Environment> env_;
};

class MasterPreferencesTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&prefs_file_));
  }

  void TearDown() override {
    EXPECT_TRUE(base::DeleteFile(prefs_file_, false));
  }

  const base::FilePath& prefs_file() const { return prefs_file_; }

 private:
  base::FilePath prefs_file_;
};

// Used to specify an expected value for a set boolean preference variable.
struct ExpectedBooleans {
  const char* name;
  bool expected_value;
};

}  // namespace

TEST_F(MasterPreferencesTest, NoFileToParse) {
  EXPECT_TRUE(base::DeleteFile(prefs_file(), false));
  installer::MasterPreferences prefs(prefs_file());
  EXPECT_FALSE(prefs.read_from_file());
}

TEST_F(MasterPreferencesTest, ParseDistroParams) {
  const char text[] =
      "{ \n"
      "  \"distribution\": { \n"
      "     \"show_welcome_page\": true,\n"
      "     \"import_bookmarks_from_file\": \"c:\\\\foo\",\n"
      "     \"do_not_create_any_shortcuts\": true,\n"
      "     \"do_not_create_desktop_shortcut\": true,\n"
      "     \"do_not_create_quick_launch_shortcut\": true,\n"
      "     \"do_not_create_taskbar_shortcut\": true,\n"
      "     \"do_not_launch_chrome\": true,\n"
      "     \"make_chrome_default\": true,\n"
      "     \"make_chrome_default_for_user\": true,\n"
      "     \"system_level\": true,\n"
      "     \"verbose_logging\": true,\n"
      "     \"require_eula\": true\n"
      "  },\n"
      "  \"blah\": {\n"
      "     \"show_welcome_page\": false\n"
      "  }\n"
      "} \n";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text,
                              static_cast<int>(strlen(text))));
  installer::MasterPreferences prefs(prefs_file());
  EXPECT_TRUE(prefs.read_from_file());

  const char* const expected_true[] = {
      installer::master_preferences::kDoNotCreateAnyShortcuts,
      installer::master_preferences::kDoNotCreateDesktopShortcut,
      installer::master_preferences::kDoNotCreateQuickLaunchShortcut,
      installer::master_preferences::kDoNotCreateTaskbarShortcut,
      installer::master_preferences::kDoNotLaunchChrome,
      installer::master_preferences::kMakeChromeDefault,
      installer::master_preferences::kMakeChromeDefaultForUser,
      installer::master_preferences::kSystemLevel,
      installer::master_preferences::kVerboseLogging,
      installer::master_preferences::kRequireEula,
  };

  for (size_t i = 0; i < base::size(expected_true); ++i) {
    bool value = false;
    EXPECT_TRUE(prefs.GetBool(expected_true[i], &value));
    EXPECT_TRUE(value) << expected_true[i];
  }

  std::string str_value;
  EXPECT_TRUE(prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
  EXPECT_STREQ("c:\\foo", str_value.c_str());
}

TEST_F(MasterPreferencesTest, ParseMissingDistroParams) {
  const char text[] =
      "{ \n"
      "  \"distribution\": { \n"
      "     \"import_bookmarks_from_file\": \"\",\n"
      "     \"do_not_create_desktop_shortcut\": true,\n"
      "     \"do_not_create_quick_launch_shortcut\": true,\n"
      "     \"do_not_launch_chrome\": false\n"
      "  }\n"
      "} \n";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text,
                              static_cast<int>(strlen(text))));
  installer::MasterPreferences prefs(prefs_file());
  EXPECT_TRUE(prefs.read_from_file());

  ExpectedBooleans expected_bool[] = {
      {installer::master_preferences::kDoNotCreateDesktopShortcut, true},
      {installer::master_preferences::kDoNotCreateQuickLaunchShortcut, true},
      {installer::master_preferences::kDoNotLaunchChrome, false},
  };

  bool value = false;
  for (size_t i = 0; i < base::size(expected_bool); ++i) {
    EXPECT_TRUE(prefs.GetBool(expected_bool[i].name, &value));
    EXPECT_EQ(value, expected_bool[i].expected_value) << expected_bool[i].name;
  }

  const char* const missing_bools[] = {
    installer::master_preferences::kDoNotRegisterForUpdateLaunch,
    installer::master_preferences::kMakeChromeDefault,
    installer::master_preferences::kMakeChromeDefaultForUser,
  };

  for (size_t i = 0; i < base::size(missing_bools); ++i) {
    EXPECT_FALSE(prefs.GetBool(missing_bools[i], &value)) << missing_bools[i];
  }

  std::string str_value;
  EXPECT_FALSE(prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
}

TEST_F(MasterPreferencesTest, FirstRunTabs) {
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"something here\": true\n"
    "  },\n"
    "  \"first_run_tabs\": [\n"
    "     \"http://google.com/f1\",\n"
    "     \"https://google.com/f2\",\n"
    "     \"new_tab_page\"\n"
    "  ]\n"
    "} \n";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text,
                              static_cast<int>(strlen(text))));
  installer::MasterPreferences prefs(prefs_file());
  typedef std::vector<std::string> TabsVector;
  TabsVector tabs = prefs.GetFirstRunTabs();
  ASSERT_EQ(3u, tabs.size());
  EXPECT_EQ("http://google.com/f1", tabs[0]);
  EXPECT_EQ("https://google.com/f2", tabs[1]);
  EXPECT_EQ("new_tab_page", tabs[2]);
}

// In this test instead of using our synthetic json file, we use an
// actual test case from the extensions unittest. The hope here is that if
// they change something in the manifest this test will break, but in
// general it is expected the extension format to be backwards compatible.
TEST(MasterPrefsExtension, ValidateExtensionJSON) {
  base::FilePath prefs_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &prefs_path));
  prefs_path = prefs_path.AppendASCII("extensions")
      .AppendASCII("good").AppendASCII("Preferences");

  installer::MasterPreferences prefs(prefs_path);
  base::DictionaryValue* extensions = NULL;
  EXPECT_TRUE(prefs.GetExtensionsBlock(&extensions));
  int location = 0;
  EXPECT_TRUE(extensions->GetInteger(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.location", &location));
  int state = 0;
  EXPECT_TRUE(extensions->GetInteger(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.state", &state));
  std::string path;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.path", &path));
  std::string key;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.key", &key));
  std::string name;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.name", &name));
  std::string version;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.version", &version));
}

// Test that we are parsing master preferences correctly.
TEST_F(MasterPreferencesTest, GetInstallPreferencesTest) {
  // Create a temporary prefs file.
  base::FilePath prefs_file;
  ASSERT_TRUE(base::CreateTemporaryFile(&prefs_file));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"do_not_create_desktop_shortcut\": false,\n"
    "     \"do_not_create_quick_launch_shortcut\": false,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"system_level\": true,\n"
    "     \"verbose_logging\": false\n"
    "  }\n"
    "} \n";
  EXPECT_TRUE(base::WriteFile(prefs_file, text,
                              static_cast<int>(strlen(text))));

  // Make sure command line values override the values in master preferences.
  std::wstring cmd_str(
      L"setup.exe --installerdata=\"" + prefs_file.value() + L"\"");
  cmd_str.append(L" --do-not-launch-chrome");
  base::CommandLine cmd_line = base::CommandLine::FromString(cmd_str);
  installer::MasterPreferences prefs(cmd_line);

  // Check prefs that do not have any equivalent command line option.
  ExpectedBooleans expected_bool[] = {
    { installer::master_preferences::kDoNotLaunchChrome, true },
    { installer::master_preferences::kSystemLevel, true },
    { installer::master_preferences::kVerboseLogging, false },
  };

  // Now check that prefs got merged correctly.
  bool value = false;
  for (size_t i = 0; i < base::size(expected_bool); ++i) {
    EXPECT_TRUE(prefs.GetBool(expected_bool[i].name, &value));
    EXPECT_EQ(value, expected_bool[i].expected_value) << expected_bool[i].name;
  }

  // Delete temporary prefs file.
  EXPECT_TRUE(base::DeleteFile(prefs_file, false));

  // Check that if master prefs doesn't exist, we can still parse the common
  // prefs.
  cmd_str = L"setup.exe --do-not-launch-chrome";
  cmd_line.ParseFromString(cmd_str);
  installer::MasterPreferences prefs2(cmd_line);
  ExpectedBooleans expected_bool2[] = {
    { installer::master_preferences::kDoNotLaunchChrome, true },
  };

  for (size_t i = 0; i < base::size(expected_bool2); ++i) {
    EXPECT_TRUE(prefs2.GetBool(expected_bool2[i].name, &value));
    EXPECT_EQ(value, expected_bool2[i].expected_value)
        << expected_bool2[i].name;
  }

  EXPECT_FALSE(prefs2.GetBool(
      installer::master_preferences::kSystemLevel, &value));
  EXPECT_FALSE(prefs2.GetBool(
      installer::master_preferences::kVerboseLogging, &value));
}

TEST_F(MasterPreferencesTest, TestDefaultInstallConfig) {
  std::wstringstream chrome_cmd;
  chrome_cmd << "setup.exe";

  base::CommandLine chrome_install(
      base::CommandLine::FromString(chrome_cmd.str()));

  installer::MasterPreferences pref_chrome(chrome_install);
}

TEST_F(MasterPreferencesTest, EnforceLegacyPreferences) {
  static const char kLegacyPrefs[] =
      "{"
      "  \"distribution\": {"
      "     \"create_all_shortcuts\": false,\n"
      "     \"import_bookmarks\": true,\n"
      "     \"import_history\": true,\n"
      "     \"import_home_page\": true,\n"
      "     \"import_search_engine\": true,\n"
      "     \"ping_delay\": 40\n"
      "  }"
      "}";

  installer::MasterPreferences prefs(kLegacyPrefs);

  bool do_not_create_desktop_shortcut = false;
  bool do_not_create_quick_launch_shortcut = false;
  bool do_not_create_taskbar_shortcut = false;
  prefs.GetBool(installer::master_preferences::kDoNotCreateDesktopShortcut,
                &do_not_create_desktop_shortcut);
  prefs.GetBool(installer::master_preferences::kDoNotCreateQuickLaunchShortcut,
                &do_not_create_quick_launch_shortcut);
  prefs.GetBool(installer::master_preferences::kDoNotCreateTaskbarShortcut,
                &do_not_create_taskbar_shortcut);
  // create_all_shortcuts is a legacy preference that should only enforce
  // do_not_create_desktop_shortcut and do_not_create_quick_launch_shortcut
  // when set to false.
  EXPECT_TRUE(do_not_create_desktop_shortcut);
  EXPECT_TRUE(do_not_create_quick_launch_shortcut);
  EXPECT_FALSE(do_not_create_taskbar_shortcut);

  bool actual_value = false;
  EXPECT_TRUE(prefs.master_dictionary().GetBoolean(prefs::kImportBookmarks,
                                                   &actual_value));
  EXPECT_TRUE(actual_value);
  EXPECT_TRUE(prefs.master_dictionary().GetBoolean(prefs::kImportHistory,
                                                   &actual_value));
  EXPECT_TRUE(actual_value);
  EXPECT_TRUE(prefs.master_dictionary().GetBoolean(prefs::kImportHomepage,
                                                   &actual_value));
  EXPECT_TRUE(actual_value);
  EXPECT_TRUE(prefs.master_dictionary().GetBoolean(prefs::kImportSearchEngine,
                                                   &actual_value));
  EXPECT_TRUE(actual_value);

#if BUILDFLAG(ENABLE_RLZ)
  int rlz_ping_delay = 0;
  EXPECT_TRUE(prefs.master_dictionary().GetInteger(prefs::kRlzPingDelaySeconds,
                                                   &rlz_ping_delay));
  EXPECT_EQ(40, rlz_ping_delay);
#endif  // BUILDFLAG(ENABLE_RLZ)
}

TEST_F(MasterPreferencesTest, DontEnforceLegacyCreateAllShortcutsTrue) {
  static const char kCreateAllShortcutsFalsePrefs[] =
      "{"
      "  \"distribution\": {"
      "     \"create_all_shortcuts\": true"
      "  }"
      "}";

    installer::MasterPreferences prefs(kCreateAllShortcutsFalsePrefs);

    bool do_not_create_desktop_shortcut = false;
    bool do_not_create_quick_launch_shortcut = false;
    bool do_not_create_taskbar_shortcut = false;
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateDesktopShortcut,
        &do_not_create_desktop_shortcut);
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateQuickLaunchShortcut,
        &do_not_create_quick_launch_shortcut);
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateTaskbarShortcut,
        &do_not_create_taskbar_shortcut);
    EXPECT_FALSE(do_not_create_desktop_shortcut);
    EXPECT_FALSE(do_not_create_quick_launch_shortcut);
    EXPECT_FALSE(do_not_create_taskbar_shortcut);
}

TEST_F(MasterPreferencesTest, DontEnforceLegacyCreateAllShortcutsNotSpecified) {
  static const char kCreateAllShortcutsFalsePrefs[] =
      "{"
      "  \"distribution\": {"
      "     \"some_other_pref\": true"
      "  }"
      "}";

    installer::MasterPreferences prefs(kCreateAllShortcutsFalsePrefs);

    bool do_not_create_desktop_shortcut = false;
    bool do_not_create_quick_launch_shortcut = false;
    bool do_not_create_taskbar_shortcut = false;
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateDesktopShortcut,
        &do_not_create_desktop_shortcut);
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateQuickLaunchShortcut,
        &do_not_create_quick_launch_shortcut);
    prefs.GetBool(
        installer::master_preferences::kDoNotCreateTaskbarShortcut,
        &do_not_create_taskbar_shortcut);
    EXPECT_FALSE(do_not_create_desktop_shortcut);
    EXPECT_FALSE(do_not_create_quick_launch_shortcut);
    EXPECT_FALSE(do_not_create_taskbar_shortcut);
}

TEST_F(MasterPreferencesTest, GoogleUpdateIsMachine) {
  {
    ScopedGoogleUpdateIsMachine env_setter("0");
    installer::MasterPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::master_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("1");
    installer::MasterPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::master_preferences::kSystemLevel, &value);
    EXPECT_TRUE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("1bridgetoofar");
    installer::MasterPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::master_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("2");
    installer::MasterPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::master_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
}
