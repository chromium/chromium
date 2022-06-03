// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/page_load_strategy.h"

#include <ostream>

#include "base/notreached.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h"

const char PageLoadStrategy::kNormal[] = "normal";
const char PageLoadStrategy::kNone[] = "none";
const char PageLoadStrategy::kEager[] = "eager";

PageLoadStrategy* PageLoadStrategy::Create(
    std::string strategy,
    DevToolsClient* client,
    WebView* web_view,
    const BrowserInfo* browser_info,
    const JavaScriptDialogManager* dialog_manager) {
  if (strategy == kNone) {
    return new NonBlockingNavigationTracker();
  } else if (strategy == kNormal) {
    return new NavigationTracker(client, web_view, browser_info, dialog_manager,
                                 false);
  } else if (strategy == kEager) {
    return new NavigationTracker(client, web_view, browser_info, dialog_manager,
                                 true);
  } else {
    NOTREACHED() << "invalid strategy '" << strategy << "'";
    return nullptr;
  }
}
