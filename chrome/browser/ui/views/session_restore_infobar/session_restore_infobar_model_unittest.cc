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
  auto model = std::make_unique<SessionRestoreInfobarModel>(profile, false);
  EXPECT_TRUE(model);
}

TEST_F(SessionRestoreInfobarModelTest, GetSessionRestoreMessageValue_Prefs) {
  TestingProfile profile;
  SessionRestoreInfobarModel model(profile, false);

  // Test case 1: ContinueWhereLeftOff.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  EXPECT_EQ(SessionRestoreInfobarModel::SessionRestoreMessageValue::
                kContinueWhereLeftOff,
            model.GetSessionRestoreMessageValue());

  // Test case 2: OpenSpecificPages.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::kOpenSpecificPages,
      model.GetSessionRestoreMessageValue());

  // Test case 3: OpenNewTabPage.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 5);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::kOpenNewTabPage,
      model.GetSessionRestoreMessageValue());
}

TEST_F(SessionRestoreInfobarModelTest, GetUntouchedSessionRestoreDefaultPref) {
  TestingProfile profile;
  auto model = std::make_unique<SessionRestoreInfobarModel>(profile, false);
  // Make sure the session restore preference is untouched.
  EXPECT_TRUE(model->IsDefaultSessionRestorePref());
  // Change session restore to default value.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  // Because the preference has been touched, default restore should be false.
  EXPECT_FALSE(model->IsDefaultSessionRestorePref());
}

TEST_F(SessionRestoreInfobarModelTest,
       GetUntouchedSessionRestoreDefaultPref_SetToDefault) {
  TestingProfile profile;
  auto model = std::make_unique<SessionRestoreInfobarModel>(profile, false);
  // Make sure the session restore preference is untouched.
  EXPECT_TRUE(model->IsDefaultSessionRestorePref());

  // Get the default value of the pref.
  int default_value = profile.GetPrefs()->GetInteger(prefs::kRestoreOnStartup);
  // Explicitly set session restore to its default value.
  profile.GetPrefs()->SetInteger(prefs::kRestoreOnStartup, default_value);
  // Because the preference has been explicitly set, even to the default value,
  // default restore should be false.
  EXPECT_FALSE(model->IsDefaultSessionRestorePref());
}

}  // namespace session_restore_infobar
