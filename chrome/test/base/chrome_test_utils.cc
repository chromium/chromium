// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"

#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
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
  return browser_test->browser()->tab_strip_model()->GetActiveWebContents();
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
  return browser_test->browser()->tab_strip_model()->GetActiveTab();
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
  return browser_test->browser()->tab_strip_model()->GetWebContentsAt(index);
#endif
}

Profile* GetProfile(const PlatformBrowserTest* browser_test) {
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel())
      return model->GetProfile();
  }
  NOTREACHED() << "No active TabModel??";
#else
  return browser_test->browser()->profile();
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

base::FilePath GetChromeTestDataDir() {
  return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
}

void OverrideChromeTestDataDir() {
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  CHECK(base::PathService::Override(chrome::DIR_TEST_DATA,
                                    src_dir.Append(GetChromeTestDataDir())));
}

base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

}  // namespace chrome_test_utils
