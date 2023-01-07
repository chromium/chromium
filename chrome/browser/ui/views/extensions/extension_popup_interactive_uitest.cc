// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/permissions/request_type.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using ExtensionPopupInteractiveUiTest = extensions::ExtensionApiTest;

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

// Tests that the extension popup does not render over an anchored permissions
// bubble. Regression test for https://crbug.com/1300006.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       ExtensionPopupOverPermissions) {
  // Geolocation requires HTTPS. Since we programmatically show the geolocation
  // prompt from C++ (rather than triggering the web API), this might not be
  // strictly necessary, but is nice to have.
  UseHttpsTestServer();
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an extension with a popup.
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

  // Navigate to an https site.
  const GURL secure_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  EXPECT_EQ("https", secure_url.scheme());

  content::RenderFrameHost* frame =
      ui_test_utils::NavigateToURL(browser(), secure_url);
  ASSERT_TRUE(frame);

  // Show the geolocation permissions prompt.
  test::PermissionRequestManagerTestApi permissions_api(browser());
  ASSERT_TRUE(permissions_api.manager());
  permissions_api.AddSimpleRequest(frame,
                                   permissions::RequestType::kGeolocation);
  // Sadly, there's no notification for these. All tests seem to rely on
  // RunUntilIdle() being sufficient.
  // TODO(https://crbug.com/1317865): Change this to be more deterministic.
  base::RunLoop().RunUntilIdle();

  // The permission may be shown using a chip UI instead of a popped-up bubble.
  // If so, click on the chip to open the bubble.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBarView* lbv = browser_view->toolbar()->location_bar();
  if (lbv->chip_controller()->IsPermissionPromptChipVisible() &&
      !lbv->chip_controller()->IsBubbleShowing()) {
    views::test::ButtonTestApi(lbv->chip_controller()->chip())
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  // The permissions bubble should now be showing.
  ASSERT_TRUE(permissions_api.GetPromptWindow());

  {
    // Open the extension popup.
    extensions::ExtensionHostTestHelper popup_waiter(profile(),
                                                     extension->id());
    popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
    ExtensionActionTestHelper::Create(browser())->Press(extension->id());
    popup_waiter.WaitForHostCompletedFirstLoad();
  }

  ExtensionPopup* extension_popup = ExtensionPopup::last_popup_for_testing();
  ASSERT_TRUE(extension_popup);
  // Ensure the popup is visible (it shows asynchronously from resource load).
  // This is safe to call even if the widget is already visible.
  views::test::WidgetVisibleWaiter(extension_popup->GetWidget()).Wait();
  EXPECT_TRUE(extension_popup->GetWidget()->IsVisible());

  // Finally, verify that the extension popup is not on top of the permissions
  // bubble.
  const bool is_stacked_above = views::test::WidgetTest::IsWindowStackedAbove(
      extension_popup->GetWidget(), permissions_api.GetPromptWindow());

#if BUILDFLAG(IS_MAC)
  // Child window re-ordering is not reliable on macOS <= 10.13.
  if (base::mac::IsAtMostOS10_13())
    return;
#endif
  EXPECT_FALSE(is_stacked_above);
}
