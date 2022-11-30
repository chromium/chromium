// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class PreinstalledWebAppUtilsTest : public testing::Test {
 public:
  PreinstalledWebAppUtilsTest() = default;
  PreinstalledWebAppUtilsTest(const PreinstalledWebAppUtilsTest&) = delete;
  PreinstalledWebAppUtilsTest& operator=(const PreinstalledWebAppUtilsTest&) =
      delete;
  ~PreinstalledWebAppUtilsTest() override = default;
};

// https://crbug.com/1198780 tracks test failures due to memory smashing on
// Linux, ChromeOS, and the Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_GetTranslatedName DISABLED_GetTranslatedName
#else
#define MAYBE_GetTranslatedName GetTranslatedName
#endif
TEST_F(PreinstalledWebAppUtilsTest, MAYBE_GetTranslatedName) {
  std::string test_locale;
  constexpr Translation kTranslations[] = {
      {"en", "en"},
      {"en-GB", "en-GB"},
      {"fr", "fr"},
  };

  auto test = [&](const char* application_locale) -> std::string {
    test_locale = application_locale;
    g_browser_process->SetApplicationLocale(test_locale);
    return GetTranslatedName("default", kTranslations);
  };

  EXPECT_EQ(test("en"), "en");
  EXPECT_EQ(test("en-US"), "en");
  EXPECT_EQ(test("en-GB"), "en-GB");
  EXPECT_EQ(test("fr"), "fr");
  EXPECT_EQ(test("fr-CA"), "fr");
  EXPECT_EQ(test("ko"), "default");
}

}  // namespace web_app
