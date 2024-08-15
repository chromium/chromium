// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"

#include "build/build_config.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace chrome_test_utils {

content::WebContents* GetActiveWebContents(PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel())
      return model->GetActiveWebContents();
  }
  NOTREACHED_IN_MIGRATION() << "No active TabModel??";
  return nullptr;
#else
  return browser_test->browser()->tab_strip_model()->GetActiveWebContents();
#endif
}

tabs::TabInterface* GetActiveTabInterface(PlatformBrowserTest* browser_test) {
  // TODO(yzshen): Once BrowserWindowInterface is supported on Android, consider
  // using it to get the active tab.
  content::WebContents* active_web_contents =
      GetActiveWebContents(browser_test);
  if (!active_web_contents) {
    return nullptr;
  }
  return tabs::TabInterface::GetFromContents(active_web_contents);
}

Profile* GetProfile(PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel())
      return model->GetProfile();
  }
  NOTREACHED_IN_MIGRATION() << "No active TabModel??";
  return nullptr;
#else
  return browser_test->browser()->profile();
#endif
}

}  // namespace chrome_test_utils
