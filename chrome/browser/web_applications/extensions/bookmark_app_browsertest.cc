// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

using BookmarkAppBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(BookmarkAppBrowserTest, NoAppBrowserController) {
  const Extension* extension = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app"),
      /*expected_change=*/1, extensions::mojom::ManifestLocation::kUnpacked,
      Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension->is_hosted_app());
  ASSERT_TRUE(extension->from_bookmark());

  const std::string app_name =
      web_app::GenerateApplicationNameFromAppId(extension->id());
  Browser::CreateParams browser_params = Browser::CreateParams::CreateForApp(
      app_name, /*trusted_source=*/true, gfx::Rect(), profile(),
      /*user_gesture=*/true);
  Browser* app_browser = Browser::Create(browser_params);
  EXPECT_FALSE(app_browser->app_controller());
}

}  // namespace extensions
