// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_params.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class BrowserNavigatorParamsTest : public ChromeRenderViewHostTestHarness {};

TEST_F(BrowserNavigatorParamsTest, FillNavigateParamsFromOpenURLParamsOTR) {
  content::OpenURLParams params(GURL("https://foo.com"), content::Referrer(),
                                WindowOpenDisposition::OFF_THE_RECORD,
                                ui::PAGE_TRANSITION_LINK, false);

  NavigateParams result(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  result.FillNavigateParamsFromOpenURLParams(params);
  // The navigation is crossing from normal to OFF_THE_RECORD browsing.
  ASSERT_EQ(result.privacy_sensitivity,
            NavigateParams::PrivacySensitivity::CROSS_OTR);
}

TEST_F(BrowserNavigatorParamsTest, FillNavigateParamsFromOpenURLParamsNonOTR) {
  content::OpenURLParams params(GURL("https://foo.com"), content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);

  NavigateParams result(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  result.FillNavigateParamsFromOpenURLParams(params);
  // There is no abnormal privacy property of this navigation.
  ASSERT_EQ(result.privacy_sensitivity,
            NavigateParams::PrivacySensitivity::DEFAULT);
}
