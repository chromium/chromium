// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace chrome_test_utils {

content::WebContents* GetActiveWebContents(PlatformBrowserTest* browser_test) {
#if defined(OS_ANDROID)
  for (size_t i = 0; i < TabModelList::size(); ++i) {
    if (TabModelList::get(i)->IsCurrentModel())
      return TabModelList::get(i)->GetActiveWebContents();
  }
  NOTREACHED() << "No active TabModel??";
  return nullptr;
#else
  return browser_test->browser()->tab_strip_model()->GetActiveWebContents();
#endif
}

}  // namespace chrome_test_utils
