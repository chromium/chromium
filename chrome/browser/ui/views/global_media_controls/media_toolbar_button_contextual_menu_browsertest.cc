// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class MediaToolbarButtonContextualMenuBrowserTest
    : public InProcessBrowserTest {
 public:
  MediaToolbarButtonContextualMenuBrowserTest() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeedbackDisabledDialog);
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaToolbarButtonContextualMenuBrowserTest,
                       ExecuteReportIssueCommand) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto menu = std::make_unique<MediaToolbarButtonContextualMenu>(
      browser()->GetProfile());
  auto model = menu->CreateMenuModel();
  std::optional<size_t> index =
      model->GetIndexOfCommandId(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  ASSERT_TRUE(index.has_value());
  model->ActivatedAt(index.value(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            GURL("chrome://cast-feedback"));
#else
  GTEST_SKIP()
      << "Report issue command is only available in Google Chrome branding";
#endif
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
IN_PROC_BROWSER_TEST_F(MediaToolbarButtonContextualMenuBrowserTest,
                       ExecuteReportIssueCommand_FeedbackDisabled) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  false);

  base::HistogramTester histogram_tester;
  auto menu = std::make_unique<MediaToolbarButtonContextualMenu>(
      browser()->GetProfile());
  auto model = menu->CreateMenuModel();
  std::optional<size_t> index =
      model->GetIndexOfCommandId(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  ASSERT_TRUE(index.has_value());
  model->ActivatedAt(index.value(), 0);
  EXPECT_NE(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            GURL("chrome://cast-feedback"));
  histogram_tester.ExpectUniqueSample(
      "Feedback.DisabledDialog.ParentStatus",
      chrome::FeedbackDisabledDialogParentStatus::kFoundByFallback, 1);
}
#endif
