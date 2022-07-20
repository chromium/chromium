// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/pref_names.h"

class MediaToolbarButtonContextualMenuTest : public MenuModelTest,
                                             public BrowserWithTestWindowTest {
 public:
  MediaToolbarButtonContextualMenuTest() = default;
  ~MediaToolbarButtonContextualMenuTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));

    menu_ = MediaToolbarButtonContextualMenu::Create(browser());
  }

  void ExecuteToggleOtherSessionCommand() {
    menu_->ExecuteCommand(IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS, 0);
  }

  bool IsOtherSessionItemChecked() {
    return menu_->IsCommandIdChecked(
        IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void ExecuteReportIssueCommand() {
    menu_->ExecuteCommand(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE, 0);
  }
#endif

 private:
  std::unique_ptr<MediaToolbarButtonContextualMenu> menu_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaToolbarButtonContextualMenuTest, ShowMenu) {
  auto menu = MediaToolbarButtonContextualMenu::Create(browser());
  auto model = menu->CreateMenuModel();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(model->GetItemCount(), 2);
  EXPECT_EQ(model->GetCommandIdAt(1),
            IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
#else
  EXPECT_EQ(model->GetItemCount(), 1);
#endif
  EXPECT_EQ(model->GetCommandIdAt(0),
            IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);
}

// The kMediaRouterShowCastSessionsStartedByOtherDevices pref is not registered
// on ChromeOS nor Android.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(MediaToolbarButtonContextualMenuTest, ToggleOtherSessionsItem) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(MediaToolbarButtonContextualMenuTest, ExecuteReportIssueCommand) {
  ExecuteReportIssueCommand();
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            GURL("chrome://cast-feedback"));
}
#endif
