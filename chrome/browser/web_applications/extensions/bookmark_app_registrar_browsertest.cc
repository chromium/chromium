// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "extensions/test/test_extension_dir.h"

using BookmarkAppRegistrarBrowserTest = extensions::ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(BookmarkAppRegistrarBrowserTest, HostedAppIsInstalled) {
  // Install hosted app with web extents.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
     "name": "Hosted App",
      "version": "1",
      "manifest_version": 2,
      "app": {
        "launch": { "web_url": "https://app.com/" },
        "urls": ["*://app.com/"]
      }
    }
  )");
  const extensions::Extension* hosted_app = InstallExtensionWithSourceAndFlags(
      test_dir.UnpackedPath(), 1, extensions::Manifest::INTERNAL,
      extensions::Extension::NO_FLAGS);
  ASSERT_TRUE(hosted_app);

  // BookmarkAppRegistrar should not consider app.com as having installed PWAs.
  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile())->registrar();
  EXPECT_FALSE(registrar.IsLocallyInstalled(GURL("https://app.com/")));
  EXPECT_FALSE(
      registrar.IsLocallyInstalled(GURL("https://app.com/inner_page.html")));
}
