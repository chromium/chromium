// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class PrefsTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  virtual base::FilePath GetPreferencesFilePath() {
    base::FilePath test_data_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("web_prefs")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_directory;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    base::FilePath default_profile =
        user_data_directory.AppendASCII(TestingProfile::kTestUserProfileDir);
    if (!base::CreateDirectory(default_profile)) {
      LOG(ERROR) << "Can't create " << default_profile.MaybeAsASCII();
      return false;
    }
    base::FilePath pref_file = GetPreferencesFilePath();
    if (!base::PathExists(pref_file)) {
      LOG(ERROR) << "Doesn't exist " << pref_file.MaybeAsASCII();
      return false;
    }
    base::FilePath default_pref_file =
        default_profile.Append(chrome::kPreferencesFilename);
    if (!base::CopyFile(pref_file, default_pref_file)) {
      LOG(ERROR) << "Copy error from " << pref_file.MaybeAsASCII()
                 << " to " << default_pref_file.MaybeAsASCII();
      return false;
    }

#if defined(OS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    if (!::SetFileAttributesW(default_pref_file.value().c_str(),
                              FILE_ATTRIBUTE_NORMAL)) return false;
#endif
    return true;
  }
};

// Tests that a sampling of web prefs are registered and ones with values in the
// test user preferences file take on those values.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, WebPrefs) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitCursiveFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitSerifFontFamilyJapanese)->IsDefaultValue());

  EXPECT_EQ("windows-1251", prefs->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(16, prefs->GetInteger(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ("Noto Sans CJK KR",
            prefs->GetString(prefs::kWebKitStandardFontFamilyKorean));
  EXPECT_EQ("Tinos", prefs->GetString(prefs::kWebKitStandardFontFamily));
  EXPECT_EQ("DejaVu Sans", prefs->GetString(prefs::kWebKitSansSerifFontFamily));
}
