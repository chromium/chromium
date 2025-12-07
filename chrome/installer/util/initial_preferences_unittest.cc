// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for initial preferences related methods.

#include "chrome/installer/util/initial_preferences.h"

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "rlz/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Optional;

// A helper class to set the "GoogleUpdateIsMachine" environment variable.
class ScopedGoogleUpdateIsMachine {
 public:
  explicit ScopedGoogleUpdateIsMachine(const char* value)
      : env_(base::Environment::Create()) {
    env_->SetVar("GoogleUpdateIsMachine", value);
  }

  ~ScopedGoogleUpdateIsMachine() { env_->UnSetVar("GoogleUpdateIsMachine"); }

 private:
  std::unique_ptr<base::Environment> env_;
};

class InitialPreferencesTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&prefs_file_));
  }

  void TearDown() override { EXPECT_TRUE(base::DeleteFile(prefs_file_)); }

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

TEST_F(InitialPreferencesTest, NoFileToParse) {
  EXPECT_TRUE(base::DeleteFile(prefs_file()));
  installer::InitialPreferences prefs(prefs_file());
  EXPECT_FALSE(prefs.read_from_file());
}

TEST_F(InitialPreferencesTest, ParseDistroParams) {
  const char text[] = R"({
    "distribution": {
      "show_welcome_page": true,
      "import_bookmarks_from_file": "c:\\foo",
      "do_not_create_any_shortcuts": true,
      "do_not_create_desktop_shortcut": true,
      "do_not_create_quick_launch_shortcut": true,
      "do_not_create_taskbar_shortcut": true,
      "do_not_launch_chrome": true,
      "make_chrome_default_for_user": true,
      "program_files_dir": "c:\\bar",
      "system_level": true,
      "verbose_logging": true,
      "require_eula": true
    },
    "blah": {
      "show_welcome_page": false
    }
  })";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text));
  installer::InitialPreferences prefs(prefs_file());
  EXPECT_TRUE(prefs.read_from_file());

  const char* const expected_true[] = {
      installer::initial_preferences::kDoNotCreateAnyShortcuts,
      installer::initial_preferences::kDoNotCreateDesktopShortcut,
      installer::initial_preferences::kDoNotCreateQuickLaunchShortcut,
      installer::initial_preferences::kDoNotCreateTaskbarShortcut,
      installer::initial_preferences::kDoNotLaunchChrome,
      installer::initial_preferences::kMakeChromeDefaultForUser,
      installer::initial_preferences::kSystemLevel,
      installer::initial_preferences::kVerboseLogging,
      installer::initial_preferences::kRequireEula,
  };

  for (const char* pref_name : expected_true) {
    bool value = false;
    EXPECT_TRUE(prefs.GetBool(pref_name, &value));
    EXPECT_TRUE(value) << pref_name;
  }

  std::string str_value;
  EXPECT_TRUE(prefs.GetString(
      installer::initial_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
  EXPECT_STREQ("c:\\foo", str_value.c_str());

  base::FilePath path;
  EXPECT_TRUE(
      prefs.GetPath(installer::initial_preferences::kProgramFilesDir, &path));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("c:\\bar")), path);
}

TEST_F(InitialPreferencesTest, ParseMissingDistroParams) {
  const char text[] = R"({
    "distribution": {
      "import_bookmarks_from_file": "",
      "do_not_create_desktop_shortcut": true,
      "do_not_create_quick_launch_shortcut": true,
      "do_not_launch_chrome": false
    }
  })";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text));
  installer::InitialPreferences prefs(prefs_file());
  EXPECT_TRUE(prefs.read_from_file());

  ExpectedBooleans expected_bool[] = {
      {installer::initial_preferences::kDoNotCreateDesktopShortcut, true},
      {installer::initial_preferences::kDoNotCreateQuickLaunchShortcut, true},
      {installer::initial_preferences::kDoNotLaunchChrome, false},
  };

  bool value = false;
  for (const auto& expected : expected_bool) {
    EXPECT_TRUE(prefs.GetBool(expected.name, &value));
    EXPECT_EQ(value, expected.expected_value) << expected.name;
  }

  const char* const missing_bools[] = {
      installer::initial_preferences::kDoNotRegisterForUpdateLaunch,
      installer::initial_preferences::kMakeChromeDefaultForUser,
  };

  for (const char* missing_bool : missing_bools) {
    EXPECT_FALSE(prefs.GetBool(missing_bool, &value)) << missing_bool;
  }

  std::string str_value;
  EXPECT_FALSE(prefs.GetString(
      installer::initial_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
}

TEST_F(InitialPreferencesTest, FirstRunTabs) {
  const char text[] = R"({
    "distribution": {
      "something here": true
    },
    "first_run_tabs": [
      "http://google.com/f1",
      "https://google.com/f2",
      "new_tab_page"
    ]
  })";

  EXPECT_TRUE(base::WriteFile(prefs_file(), text));
  installer::InitialPreferences prefs(prefs_file());
  typedef std::vector<std::string> TabsVector;
  TabsVector tabs = prefs.GetFirstRunTabs();
  ASSERT_EQ(3u, tabs.size());
  EXPECT_EQ("http://google.com/f1", tabs[0]);
  EXPECT_EQ("https://google.com/f2", tabs[1]);
  EXPECT_EQ("new_tab_page", tabs[2]);
}

// Test the parsing of the initial_extensions block from initial preferences.
TEST_F(InitialPreferencesTest, ParseInitialExtensionsWithProviderName) {
  constexpr char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  constexpr char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  constexpr char kProvider[] = "GTest";
  constexpr std::string_view kInitialExtensions = R"({
    "initial_extensions": {
      "provider_name": "GTest",
      "list": [
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      ]
    }
  })";

  ASSERT_TRUE(base::WriteFile(prefs_file(), kInitialExtensions));
  installer::InitialPreferences prefs(prefs_file());
  ASSERT_TRUE(prefs.read_from_file());

  const base::Value::List* extensions = prefs.GetInitialExtensionsList();
  ASSERT_NE(extensions, nullptr);
  ASSERT_EQ(extensions->size(), 2u);

  const std::string* id1 = (*extensions)[0].GetIfString();
  ASSERT_NE(id1, nullptr);
  EXPECT_EQ(*id1, kTestExtensionId1);

  const std::string* id2 = (*extensions)[1].GetIfString();
  ASSERT_NE(id2, nullptr);
  EXPECT_EQ(*id2, kTestExtensionId2);

  EXPECT_EQ(prefs.GetInitialExtensionsProviderName(), std::string(kProvider));
}

// Test parsing when initial_extensions.list exists but provider_name is
// omitted.
TEST_F(InitialPreferencesTest, ParseInitialExtensionsWithoutProviderName) {
  constexpr char kTestExtensionId1[] = "cccccccccccccccccccccccccccccccc";
  constexpr char kTestExtensionId2[] = "dddddddddddddddddddddddddddddddd";
  constexpr std::string_view kInitialExtensions = R"({
    "initial_extensions": {
      "list": [
        "cccccccccccccccccccccccccccccccc",
        "dddddddddddddddddddddddddddddddd"
      ]
    }
  })";

  ASSERT_TRUE(base::WriteFile(prefs_file(), kInitialExtensions));
  installer::InitialPreferences prefs(prefs_file());
  ASSERT_TRUE(prefs.read_from_file());

  const base::Value::List* extensions = prefs.GetInitialExtensionsList();
  ASSERT_NE(extensions, nullptr);
  ASSERT_EQ(extensions->size(), 2u);

  const std::string* id1 = (*extensions)[0].GetIfString();
  ASSERT_NE(id1, nullptr);
  EXPECT_EQ(*id1, kTestExtensionId1);

  const std::string* id2 = (*extensions)[1].GetIfString();
  ASSERT_NE(id2, nullptr);
  EXPECT_EQ(*id2, kTestExtensionId2);

  // Provider name should be empty when not present.
  EXPECT_TRUE(prefs.GetInitialExtensionsProviderName().empty());
}

// Test that GetInitialExtensionsList returns null when the block is absent.
TEST_F(InitialPreferencesTest, MissingInitialExtensionsBlock) {
  constexpr std::string_view kDistribution = R"({
  "distribution": {
      "verbose_logging": true
    }
  })";

  ASSERT_TRUE(base::WriteFile(prefs_file(), kDistribution));
  installer::InitialPreferences prefs(prefs_file());
  ASSERT_TRUE(prefs.read_from_file());

  const base::Value::List* extensions = prefs.GetInitialExtensionsList();
  EXPECT_EQ(extensions, nullptr);
}

// Test the parsing of bookmarks block from initial preferences.
TEST_F(InitialPreferencesTest, ValidateBookmarksJSON) {
  constexpr char bookmarks_json_string[] = R"({
    "bookmarks": {
      "first_run_bookmarks": {
        "children": [
          {
            "name": "ABC",
            "type": "url",
            "url": "https://google.com"
          },
          {
            "name": "Folder1",
            "type": "folder",
            "children": [
              {
                "name": "ABC",
                "type": "url",
                "url": "https://google.com"
              },
              {
                "name": "XYZ",
                "type": "url",
                "url": "https://facebook.com"
              }
            ]
          }
        ]
      }
    }
  })";
  ASSERT_TRUE(base::WriteFile(prefs_file(), bookmarks_json_string));

  installer::InitialPreferences prefs(prefs_file());

  const base::Value::Dict* bookmarks = prefs.GetBookmarksBlock();
  ASSERT_TRUE(bookmarks);

  ASSERT_TRUE(bookmarks->FindDict("first_run_bookmarks"));

  const base::Value::List* children =
      bookmarks->FindListByDottedPath("first_run_bookmarks.children");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 2u);

  const base::Value::Dict* first_child = (*children)[0].GetIfDict();
  ASSERT_TRUE(first_child);
  EXPECT_EQ(*first_child->FindString("name"), "ABC");
  EXPECT_EQ(*first_child->FindString("type"), "url");
  EXPECT_EQ(*first_child->FindString("url"), "https://google.com");

  const base::Value::Dict* second_child = (*children)[1].GetIfDict();
  ASSERT_TRUE(second_child);
  EXPECT_EQ(*second_child->FindString("name"), "Folder1");
  EXPECT_EQ(*second_child->FindString("type"), "folder");
  EXPECT_EQ(second_child->FindList("children")->size(), 2u);
}

// Test that we are parsing initial preferences correctly.
TEST_F(InitialPreferencesTest, GetInstallPreferencesTest) {
  // Create a temporary prefs file.
  base::FilePath prefs_file;
  ASSERT_TRUE(base::CreateTemporaryFile(&prefs_file));
  const char text[] = R"({
    "distribution": {
      "do_not_create_desktop_shortcut": false,
      "do_not_create_quick_launch_shortcut": false,
      "do_not_launch_chrome": true,
      "system_level": true,
      "verbose_logging": false
    }
  })";
  EXPECT_TRUE(base::WriteFile(prefs_file, text));

  // Make sure command line values override the values in initial preferences.
  std::wstring cmd_str(L"setup.exe --installerdata=\"" + prefs_file.value() +
                       L"\"");
  cmd_str.append(L" --do-not-launch-chrome");
  base::CommandLine cmd_line = base::CommandLine::FromString(cmd_str);
  installer::InitialPreferences prefs(cmd_line);

  // Check prefs that do not have any equivalent command line option.
  ExpectedBooleans expected_bool[] = {
      {installer::initial_preferences::kDoNotLaunchChrome, true},
      {installer::initial_preferences::kSystemLevel, true},
      {installer::initial_preferences::kVerboseLogging, false},
  };

  // Now check that prefs got merged correctly.
  bool value = false;
  for (const auto& expected : expected_bool) {
    EXPECT_TRUE(prefs.GetBool(expected.name, &value));
    EXPECT_EQ(value, expected.expected_value) << expected.name;
  }

  // Delete temporary prefs file.
  EXPECT_TRUE(base::DeleteFile(prefs_file));

  // Check that if initial prefs doesn't exist, we can still parse the common
  // prefs.
  cmd_str = L"setup.exe --do-not-launch-chrome";
  cmd_line.ParseFromString(cmd_str);
  installer::InitialPreferences prefs2(cmd_line);
  ExpectedBooleans expected_bool2[] = {
      {installer::initial_preferences::kDoNotLaunchChrome, true},
  };

  for (const auto& expected : expected_bool2) {
    EXPECT_TRUE(prefs2.GetBool(expected.name, &value));
    EXPECT_EQ(value, expected.expected_value) << expected.name;
  }

  EXPECT_FALSE(
      prefs2.GetBool(installer::initial_preferences::kSystemLevel, &value));
  EXPECT_FALSE(
      prefs2.GetBool(installer::initial_preferences::kVerboseLogging, &value));
}

TEST_F(InitialPreferencesTest, TestDefaultInstallConfig) {
  std::wstringstream chrome_cmd;
  chrome_cmd << "setup.exe";

  base::CommandLine chrome_install(
      base::CommandLine::FromString(chrome_cmd.str()));

  installer::InitialPreferences pref_chrome(chrome_install);
}

TEST_F(InitialPreferencesTest, EnforceLegacyPreferences) {
  static const char kLegacyPrefs[] = R"({
    "distribution": {
      "create_all_shortcuts": false,
      "import_bookmarks": true,
      "import_history": true,
      "import_home_page": true,
      "import_search_engine": true,
      "ping_delay": 40
    }
  })";

  installer::InitialPreferences prefs(kLegacyPrefs);

  bool do_not_create_desktop_shortcut = false;
  bool do_not_create_quick_launch_shortcut = false;
  bool do_not_create_taskbar_shortcut = false;
  prefs.GetBool(installer::initial_preferences::kDoNotCreateDesktopShortcut,
                &do_not_create_desktop_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateQuickLaunchShortcut,
                &do_not_create_quick_launch_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateTaskbarShortcut,
                &do_not_create_taskbar_shortcut);
  // create_all_shortcuts is a legacy preference that should only enforce
  // do_not_create_desktop_shortcut and do_not_create_quick_launch_shortcut
  // when set to false.
  EXPECT_TRUE(do_not_create_desktop_shortcut);
  EXPECT_TRUE(do_not_create_quick_launch_shortcut);
  EXPECT_FALSE(do_not_create_taskbar_shortcut);

  EXPECT_THAT(prefs.initial_dictionary().FindBool(prefs::kImportBookmarks),
              Optional(true));
  EXPECT_THAT(prefs.initial_dictionary().FindBool(prefs::kImportHistory),
              Optional(true));
  EXPECT_THAT(prefs.initial_dictionary().FindBool(prefs::kImportHomepage),
              Optional(true));
  EXPECT_THAT(prefs.initial_dictionary().FindBool(prefs::kImportSearchEngine),
              Optional(true));

#if BUILDFLAG(ENABLE_RLZ)
  std::optional<int> rlz_ping_delay =
      prefs.initial_dictionary().FindInt(prefs::kRlzPingDelaySeconds);
  EXPECT_TRUE(rlz_ping_delay);
  EXPECT_GT(rlz_ping_delay, 0);
  EXPECT_EQ(40, rlz_ping_delay);
#endif  // BUILDFLAG(ENABLE_RLZ)
}

TEST_F(InitialPreferencesTest, DontEnforceLegacyCreateAllShortcutsTrue) {
  static const char kCreateAllShortcutsFalsePrefs[] = R"({
    "distribution": {
      "create_all_shortcuts": true
    }
  })";

  installer::InitialPreferences prefs(kCreateAllShortcutsFalsePrefs);

  bool do_not_create_desktop_shortcut = false;
  bool do_not_create_quick_launch_shortcut = false;
  bool do_not_create_taskbar_shortcut = false;
  prefs.GetBool(installer::initial_preferences::kDoNotCreateDesktopShortcut,
                &do_not_create_desktop_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateQuickLaunchShortcut,
                &do_not_create_quick_launch_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateTaskbarShortcut,
                &do_not_create_taskbar_shortcut);
  EXPECT_FALSE(do_not_create_desktop_shortcut);
  EXPECT_FALSE(do_not_create_quick_launch_shortcut);
  EXPECT_FALSE(do_not_create_taskbar_shortcut);
}

TEST_F(InitialPreferencesTest,
       DontEnforceLegacyCreateAllShortcutsNotSpecified) {
  static const char kCreateAllShortcutsFalsePrefs[] = R"({
    "distribution": {
      "some_other_pref": true
    }
  })";

  installer::InitialPreferences prefs(kCreateAllShortcutsFalsePrefs);

  bool do_not_create_desktop_shortcut = false;
  bool do_not_create_quick_launch_shortcut = false;
  bool do_not_create_taskbar_shortcut = false;
  prefs.GetBool(installer::initial_preferences::kDoNotCreateDesktopShortcut,
                &do_not_create_desktop_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateQuickLaunchShortcut,
                &do_not_create_quick_launch_shortcut);
  prefs.GetBool(installer::initial_preferences::kDoNotCreateTaskbarShortcut,
                &do_not_create_taskbar_shortcut);
  EXPECT_FALSE(do_not_create_desktop_shortcut);
  EXPECT_FALSE(do_not_create_quick_launch_shortcut);
  EXPECT_FALSE(do_not_create_taskbar_shortcut);
}

TEST_F(InitialPreferencesTest, GoogleUpdateIsMachine) {
  {
    ScopedGoogleUpdateIsMachine env_setter("0");
    installer::InitialPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::initial_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("1");
    installer::InitialPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::initial_preferences::kSystemLevel, &value);
    EXPECT_TRUE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("1bridgetoofar");
    installer::InitialPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::initial_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
  {
    ScopedGoogleUpdateIsMachine env_setter("2");
    installer::InitialPreferences prefs(
        base::CommandLine(base::FilePath(FILE_PATH_LITERAL("setup.exe"))));
    bool value = false;
    prefs.GetBool(installer::initial_preferences::kSystemLevel, &value);
    EXPECT_FALSE(value);
  }
}

#if !BUILDFLAG(IS_MAC)

TEST_F(InitialPreferencesTest, Path) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto initial_pref_path =
      temp_dir.GetPath().AppendASCII("initial_preferences");

  EXPECT_EQ(temp_dir.GetPath().AppendASCII("master_preferences"),
            installer::InitialPreferences::Path(temp_dir.GetPath()));
  EXPECT_EQ(initial_pref_path, installer::InitialPreferences::Path(
                                   temp_dir.GetPath(), /*for_read=*/false));

  base::File file(initial_pref_path, base::File::Flags::FLAG_CREATE);
  file.Close();

  EXPECT_EQ(initial_pref_path,
            installer::InitialPreferences::Path(temp_dir.GetPath()));
}

#endif  // !BUILDFLAG(IS_MAC)
