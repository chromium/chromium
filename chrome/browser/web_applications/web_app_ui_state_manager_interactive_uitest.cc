// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace web_app {

using WebAppUiStateManagerInteractiveBrowserTest =
    InteractiveBrowserTestT<WebAppBrowserTestBase>;

IN_PROC_BROWSER_TEST_F(WebAppUiStateManagerInteractiveBrowserTest,
                       LaunchWebApp) {}

}  // namespace web_app
