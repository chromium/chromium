// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"

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

TEST_F(PreinstalledWebAppUtilsTest, GetTranslatedName) {
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
