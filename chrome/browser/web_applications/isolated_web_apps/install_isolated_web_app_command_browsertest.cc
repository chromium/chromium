// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

using InstallIsolatedWebAppCommandBrowserTest = WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppCommandBrowserTest, DoesFoo) {
  const GURL kWebAppUrl =
      https_server()->GetURL(R"(/banners/manifest_test_page.html)");
}

}  // namespace web_app
