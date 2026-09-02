// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"

#include "build/build_config.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace chrome_test_utils {

content::WebContents* GetActiveWebContents(
    const PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel())
      return model->GetActiveWebContents();
  }
  NOTREACHED() << "No active TabModel??";
#else
  return browser_test->browser()->GetTabStripModel()->GetActiveWebContents();
#endif
}

tabs::TabInterface* GetActiveTab(const PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel()) {
      return model->GetActiveTab();
    }
  }
  NOTREACHED() << "No active TabModel??";
#else
  return browser_test->browser()->GetTabStripModel()->GetActiveTab();
#endif
}

content::WebContents* GetWebContentsAt(const PlatformBrowserTest* browser_test,
                                       int index) {
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel()) {
      return model->GetWebContentsAt(index);
    }
  }
  NOTREACHED() << "No active TabModel??";
#else
  return browser_test->browser()->GetTabStripModel()->GetWebContentsAt(index);
#endif
}

Profile* GetProfile(const PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel())
      return model->GetProfile();
  }
  NOTREACHED() << "No active TabModel??";
#else
  return browser_test->browser()->GetProfile();
#endif
}

bool NavigateToURL(content::WebContents* web_contents, const GURL& url) {
  content::TestNavigationObserver observer(web_contents);
  // The return value is ignored because some tests load URLs that cause
  // redirects, or are blocked URLs, which make NavigateToURL return false.
  std::ignore = content::NavigateToURL(web_contents, url);
  // Wait for load to stop.
  observer.Wait();
  return observer.last_navigation_succeeded();
}

}  // namespace chrome_test_utils
