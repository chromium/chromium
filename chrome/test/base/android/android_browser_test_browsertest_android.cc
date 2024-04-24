// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, Smoke) {
  ASSERT_EQ(TabModelList::models().size(), 1u);

  // Grab a tab an navigate its contents
  const TabModel* tab_model = TabModelList::models()[0];
  EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                     GURL("about:blank")));
}
