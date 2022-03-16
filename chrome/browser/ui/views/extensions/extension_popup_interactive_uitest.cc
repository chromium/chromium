// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/test_extension_dir.h"

using ExtensionPopupInteractiveUiTest = extensions::ExtensionBrowserTest;

// Tests unloading an extension while its popup is actively under inspection.
// Regression test for https://crbug.com/1304499.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       UnloadExtensionWhileInspectingPopup) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "action": { "default_popup": "popup.html" },
           "version": "0.1"
         })";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "<html>Hello, world!</html>");
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Inspect the popup and wait for it to open.
  {
    extensions::ExtensionHostTestHelper popup_waiter(profile(),
                                                     extension->id());
    popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
    ExtensionActionTestHelper::Create(browser())->InspectPopup(extension->id());
    popup_waiter.WaitForHostCompletedFirstLoad();
  }

  // Unload the extension. This causes the popup's ExtensionHost to be
  // destroyed. This should be safe.
  {
    extensions::ExtensionHostTestHelper popup_waiter(profile(),
                                                     extension->id());
    popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
    extension_service()->DisableExtension(
        extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);
    popup_waiter.WaitForHostDestroyed();
  }
}
