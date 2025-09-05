// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_restore_infobar {

class SessionRestoreInfobarModelTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SessionRestoreInfobarModelTest, CreateAndAccessModel) {
  TestingProfile profile;
  auto model =
      std::make_unique<SessionRestoreInfobarModel>(profile, false, false);
  EXPECT_TRUE(model);
}

TEST_F(SessionRestoreInfobarModelTest, GetSessionRestoreMessageValue_Prefs) {
  TestingProfile profile;
  SessionRestoreInfobarModel model(profile, false, false);

  // Test case 1: ContinueWhereLeftOff.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  EXPECT_EQ(SessionRestoreInfobarModel::SessionRestoreMessageValue::
                ContinueWhereLeftOff,
            model.GetSessionRestoreMessageValue());

  // Test case 2: OpenSpecificPages.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenSpecificPages,
      model.GetSessionRestoreMessageValue());

  // Test case 3: OpenNewTabPage.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 5);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenNewTabPage,
      model.GetSessionRestoreMessageValue());
}

}  // namespace session_restore_infobar
