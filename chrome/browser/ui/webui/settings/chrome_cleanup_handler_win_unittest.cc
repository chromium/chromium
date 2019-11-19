// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler_win.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

using safe_browsing::MockChromeCleanerProcess;

TEST(ChromeCleanupHandlerTest, GetExtensionsNamesFromIds) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_(TestingBrowserProcess::GetGlobal());

  // Set up the testing profile to get the extensions registry from it.
  ASSERT_TRUE(profile_manager_.SetUp());
  TestingProfile* testing_profile_ =
      profile_manager_.CreateTestingProfile("DummyProfile");
  MockChromeCleanerProcess::AddMockExtensionsToProfile(testing_profile_);

  std::set<base::string16> test_ids = {
      MockChromeCleanerProcess::kInstalledExtensionId1,
      MockChromeCleanerProcess::kInstalledExtensionId2,
      MockChromeCleanerProcess::kUnknownExtensionId,
  };

  std::set<base::string16> expected_names = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Extension names are only available in Google-branded builds.
    MockChromeCleanerProcess::kInstalledExtensionName1,
    MockChromeCleanerProcess::kInstalledExtensionName2,
    l10n_util::GetStringFUTF16(
        IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXTENSION_UNKNOWN,
        MockChromeCleanerProcess::kUnknownExtensionId),
#endif
  };

  std::set<base::string16> actual_names;
  ChromeCleanupHandler::GetExtensionNamesFromIds(testing_profile_, test_ids,
                                                 &actual_names);

  EXPECT_THAT(actual_names, testing::ContainerEq(expected_names));
}

}  // namespace settings
