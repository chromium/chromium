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
 protected:
  SessionRestoreInfobarModelTest() = default;

  ~SessionRestoreInfobarModelTest() override = default;

  void SetUp() override {
    ChromeBrowserMainExtraPartsProfiles::
        EnsureBrowserContextKeyedServiceFactoriesBuilt();
    RegisterProfilePrefs(/*is_signin_profile=*/false,
                         g_browser_process->GetApplicationLocale(),
                         prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(SessionRestoreInfobarModelTest, GetSessionRestoreMessageValue_Prefs) {
  // Pass the test's preference service to the model.
  SessionRestoreInfobarModel model(prefs_);

  // Test case 1: ContinueWhereLeftOff.
  prefs_.SetInteger(prefs::kRestoreOnStartup, 1);
  EXPECT_EQ(SessionRestoreInfobarModel::SessionRestoreMessageValue::
                ContinueWhereLeftOff,
            model.GetSessionRestoreMessageValue());

  // Test case 2: OpenSpecificPages.
  prefs_.SetInteger(prefs::kRestoreOnStartup, 4);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenSpecificPages,
      model.GetSessionRestoreMessageValue());

  // Test case 3: OpenNewTabPage.
  prefs_.SetInteger(prefs::kRestoreOnStartup, 5);
  EXPECT_EQ(
      SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenNewTabPage,
      model.GetSessionRestoreMessageValue());
}

}  // namespace session_restore_infobar
