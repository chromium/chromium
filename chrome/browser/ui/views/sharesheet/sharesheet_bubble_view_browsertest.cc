// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet/sharesheet_bubble_view.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class SharesheetBubbleViewBrowserTest
    : public ::testing::WithParamInterface<bool>,
      public DialogBrowserTest {
 public:
  SharesheetBubbleViewBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kNearbySharing);
    }
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    sharesheet::SharesheetService* const sharesheet_service =
        sharesheet::SharesheetServiceFactory::GetForProfile(
            browser()->profile());
    GURL test_url = GURL("https://www.google.com/");
    auto intent = apps_util::CreateIntentFromUrl(test_url);
    intent->action = apps_util::kIntentActionSend;
    sharesheet_service->ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(intent),
        base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharesheetBubbleViewBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SharesheetBubbleViewBrowserTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}
