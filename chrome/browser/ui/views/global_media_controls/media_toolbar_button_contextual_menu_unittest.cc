// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include <memory>

#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaToolbarButtonContextualMenuTest : public MenuModelTest,
                                             public testing::Test {
 public:
  MediaToolbarButtonContextualMenuTest() = default;
  ~MediaToolbarButtonContextualMenuTest() override = default;

  void SetUp() override {
    menu_ = std::make_unique<MediaToolbarButtonContextualMenu>(profile());
  }

  void TearDown() override { menu_.reset(); }

  TestingProfile* profile() { return &profile_; }

  void ExecuteToggleOtherSessionCommand() {
    menu_->ExecuteCommand(IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS, 0);
  }

  bool IsOtherSessionItemChecked() {
    return menu_->IsCommandIdChecked(
        IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);
  }

  void TestOtherSessionItemIsDisabledWhenPolicyIsSet(bool policy_value) {
    profile()->GetTestingPrefService()->SetManagedPref(
        media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
        std::make_unique<base::Value>(policy_value));

    auto menu = std::make_unique<MediaToolbarButtonContextualMenu>(profile());
    auto model = menu->CreateMenuModel();
    ASSERT_EQ(model->GetCommandIdAt(0),
              IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);
    EXPECT_FALSE(model->IsEnabledAt(0));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MediaToolbarButtonContextualMenu> menu_;
};

TEST_F(MediaToolbarButtonContextualMenuTest, ShowMenu) {
  auto menu = std::make_unique<MediaToolbarButtonContextualMenu>(profile());
  auto model = menu->CreateMenuModel();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(model->GetItemCount(), 2u);
  EXPECT_EQ(model->GetCommandIdAt(1),
            IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  EXPECT_TRUE(model->IsEnabledAt(1));
#else
  EXPECT_EQ(model->GetItemCount(), 1u);
#endif
  EXPECT_EQ(model->GetCommandIdAt(0),
            IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);
  EXPECT_TRUE(model->IsEnabledAt(0));
}

// The kMediaRouterShowCastSessionsStartedByOtherDevices pref is not registered
// on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(MediaToolbarButtonContextualMenuTest, ToggleOtherSessionsItem) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      false);
  EXPECT_FALSE(IsOtherSessionItemChecked());

  ExecuteToggleOtherSessionCommand();
  EXPECT_TRUE(IsOtherSessionItemChecked());
  EXPECT_TRUE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices));

  ExecuteToggleOtherSessionCommand();
  EXPECT_FALSE(IsOtherSessionItemChecked());
  EXPECT_FALSE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices));
}
#endif

TEST_F(MediaToolbarButtonContextualMenuTest,
       DisableOtherSessionsItemWhenPolicyIsTrue) {
  TestOtherSessionItemIsDisabledWhenPolicyIsSet(/*policy_value=*/true);
}

TEST_F(MediaToolbarButtonContextualMenuTest,
       DisableOtherSessionsItemWhenPolicyIsFalse) {
  TestOtherSessionItemIsDisabledWhenPolicyIsSet(/*policy_value=*/false);
}
