// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_ui_test_utils.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

using content::BrowserContext;
using content::WebContents;

namespace android_ui_test_utils {

void OpenUrlInNewTab(BrowserContext* context,
                     WebContents* parent,
                     const GURL& url) {
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(parent);
  ASSERT_EQ(parent, tab_model->GetActiveWebContents());
  int tab_count = tab_model->GetTabCount();
  ASSERT_GT(tab_count, 0);

  // Create a new tab.
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(context));
  WebContents* second_web_contents = contents.release();
  tab_model->CreateTab(TabAndroid::FromWebContents(parent), second_web_contents,
                       /*select=*/true);

  content::NavigateToURLBlockUntilNavigationsComplete(second_web_contents, url,
                                                      1);
  ASSERT_EQ(tab_count + 1, tab_model->GetTabCount());
  ASSERT_NE(parent, second_web_contents);
  ASSERT_EQ(second_web_contents, tab_model->GetActiveWebContents());
}

}  // namespace android_ui_test_utils
