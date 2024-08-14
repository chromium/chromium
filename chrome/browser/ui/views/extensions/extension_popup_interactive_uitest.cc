// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/window/dialog_delegate.h"

using ExtensionPopupInteractiveUiTest = extensions::ExtensionApiTest;

namespace {

// A helper class for waiting until the devtools is attached to the given
// WebContents.
class DevToolsAttachWaiter : public content::DevToolsAgentHostObserver {
 public:
  explicit DevToolsAttachWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    content::DevToolsAgentHost::AddObserver(this);
  }
  ~DevToolsAttachWaiter() override {
    content::DevToolsAgentHost::RemoveObserver(this);
  }

  void Wait() {
    if (content::DevToolsAgentHost::HasFor(web_contents_)) {
      return;
    }
    run_loop_.Run();
  }

  // content::DevToolsAgentHostObserver:
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override {
    if (agent_host->GetWebContents() == web_contents_) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<content::WebContents> web_contents_;
};

views::UniqueWidgetPtr CreateTestTopLevelWidget() {
  views::UniqueWidgetPtr widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->widget_delegate()->SetCanActivate(true);
  return widget;
}

// Create a dialog widget as a child of `parent` widget.
views::UniqueWidgetPtr CreateTestDialogWidget(views::Widget* parent) {
  auto dialog_delegate = std::make_unique<views::DialogDelegateView>();
  return std::unique_ptr<views::Widget>(
      views::DialogDelegate::CreateDialogWidget(
          dialog_delegate.release(), nullptr, parent->GetNativeView()));
}

void ExpectWidgetDestroy(base::WeakPtr<views::Widget> widget) {
  if (widget) {
    views::test::WidgetDestroyedWaiter(widget.get()).Wait();
  }
  EXPECT_TRUE(widget.WasInvalidated() || widget->IsClosed());
}

base::WeakPtr<views::Widget> WaitForLastExtensionPopupVisible() {
  EXPECT_NE(ExtensionPopup::last_popup_for_testing(), nullptr);
  base::WeakPtr<views::Widget> extension_popup_widget =
      ExtensionPopup::last_popup_for_testing()->GetWidget()->GetWeakPtr();
  EXPECT_TRUE(extension_popup_widget);
  // Ensure the popup is visible (it shows asynchronously from resource load).
  // This is safe to call even if the widget is already visible.
  views::test::WidgetVisibleWaiter(extension_popup_widget.get()).Wait();
  EXPECT_TRUE(extension_popup_widget->IsVisible());
  return extension_popup_widget;
}

base::WeakPtr<views::Widget> OpenExtensionPopup(
    Browser* browser,
    const extensions::Extension* extension) {
  extensions::ExtensionHostTestHelper popup_waiter(browser->profile(),
                                                   extension->id());
  popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
  ExtensionActionTestHelper::Create(browser)->Press(extension->id());
  popup_waiter.WaitForHostCompletedFirstLoad();
  return WaitForLastExtensionPopupVisible();
}

}  // namespace

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
  // TODO(crbug.com/40835018): Change this to be more deterministic.
  base::RunLoop().RunUntilIdle();

  // The permission may be shown using a chip UI instead of a popped-up bubble.
  // If so, click on the chip to open the bubble.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBarView* lbv = browser_view->toolbar()->location_bar();
  if (lbv->GetChipController()->IsPermissionPromptChipVisible() &&
      !lbv->GetChipController()->IsBubbleShowing()) {
    views::test::ButtonTestApi(lbv->GetChipController()->chip())
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  // The permissions bubble should now be showing.
  ASSERT_TRUE(permissions_api.GetPromptWindow());

  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Finally, verify that the extension popup is not on top of the permissions
  // bubble.
  const bool is_stacked_above = views::test::WidgetTest::IsWindowStackedAbove(
      extension_popup_widget.get(), permissions_api.GetPromptWindow());

  EXPECT_FALSE(is_stacked_above);
}

// Tests that an extension popup does not close on deactivation while it is
// under inspection.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       ExtensionPopupDoesNotCloseWhileInpsecting) {
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

  // Open the popup and inspect it.
  extensions::ExtensionHostTestHelper popup_waiter(profile(), extension->id());
  popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
  ExtensionActionTestHelper::Create(browser())->InspectPopup(extension->id());
  popup_waiter.WaitForHostCompletedFirstLoad();
  content::WebContents* extension_contents =
      ExtensionPopup::last_popup_for_testing()->host()->host_contents();
  ASSERT_NE(extension_contents, nullptr);
  DevToolsAttachWaiter(extension_contents).Wait();

  base::WeakPtr<views::Widget> extension_popup_widget =
      ExtensionPopup::last_popup_for_testing()->GetWidget()->GetWeakPtr();
  ASSERT_TRUE(extension_popup_widget);
  // Ensure the popup is visible (it shows asynchronously from resource load).
  // This is safe to call even if the widget is already visible.
  views::test::WidgetVisibleWaiter(extension_popup_widget.get()).Wait();
  EXPECT_TRUE(extension_popup_widget->IsVisible());

  // Deactivating the extension popup should not cause the extension popup to
  // close. Note that some platforms don't implement Widget::Deactivate() so we
  // activate another window (the browser window in this case) to trigger that.
  extension_popup_widget->Activate();
  // However, on Lacros activating the browser window does not cause the
  // extension popup to deactivate, thus we also explicitly call Deactivate().
  extension_popup_widget->Deactivate();
  browser()->window()->Activate();
  views::test::WaitForWidgetActive(extension_popup_widget.get(), false);
  ASSERT_TRUE(extension_popup_widget);
  EXPECT_TRUE(extension_popup_widget->IsVisible());

  // Reactivates the extension popup.
  ExtensionPopup::last_popup_for_testing()->GetWidget()->Activate();

  // Stop inspecting the extension popup.
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(extension_contents));
  scoped_refptr<content::DevToolsAgentHost> devtools_agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(extension_contents);
  devtools_agent_host->DetachAllClients();

  // Activating the browser window should cause the extension popup to be
  // deactivated and closed.
  browser()->window()->Activate();
  ExpectWidgetDestroy(extension_popup_widget);
}

// Tests that an extension popup does not close on deactivation when it shows
// a JS alert dialog.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       ExtensionPopupDoesNotCloseWhileShowingJSAlert) {
  // Install a test extension that opens an alert() dialog.
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "action": { "default_popup": "popup.html" },
           "version": "0.1"
         })";
  // Use setTimeout() to create the JS alert dialog, otherwise
  // WaitForHostCompletedFirstLoad() won't return due to the open dialog.
  static constexpr char kPageJS[] =
      R"(window.onload = function() {
           setTimeout(alert, 0);
         })";
  static constexpr char kHTML[] =
      R"(<html><script src="page.js"></script></html>)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJS);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kHTML);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Wait until the web modal dialog is shown.
  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_NE(dialog, nullptr);
  // The extension popup should be still showing.
  ASSERT_TRUE(extension_popup_widget);
  EXPECT_TRUE(extension_popup_widget->IsVisible());

  // Close the JS dialog and deactivate the extension popup.
  dialog->CloseModalDialog();
  // Activating the browser window should cause the extension popup to be
  // deactivated and closed.
  browser()->window()->Activate();

  // The extension popup should close.
  ExpectWidgetDestroy(extension_popup_widget);
}

// Tests that an extension popup is closed when a web dialog is shown as active.
// In this test the web dialog is not initiated by the extension popup (see
// ExtensionPopupDoesNotCloseWhileShowingJSAlert for showing an extension-owned
// dialog).
// This prevents the extension from occluding the web dialog, which can be
// dangerous when the web dialog is a security-related dialog (e.g. screen
// sharing dialog, webauthn attestation dialog).
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       ExtensionPopupClosesOnShowingWebDialog) {
  // Install a test extension.
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
  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Show a web dialog.
  auto web_dialog = std::make_unique<views::DialogDelegateView>();
  web_dialog->SetPreferredSize(gfx::Size(100, 100));
  web_dialog->SetModalType(ui::mojom::ModalType::kChild);
  web_dialog->SetCanActivate(true);
  views::Widget* web_dialog_widget =
      constrained_window::ShowWebModalDialogViews(
          web_dialog.release(),
          browser()->tab_strip_model()->GetActiveWebContents());
  views::test::WidgetVisibleWaiter(web_dialog_widget).Wait();
  EXPECT_TRUE(web_dialog_widget->IsVisible());

  // Check that the extension popup is closed.
  ExpectWidgetDestroy(extension_popup_widget);
}

// Tests that an extension popup closes when activating the browser window.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       ExtensionPopupClosesOnActivatingBrowserWindow) {
  // Install a test extension.
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
  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Activate the browser window should close the extension popup.
  browser()->window()->Activate();
  ExpectWidgetDestroy(extension_popup_widget);
}

#if BUILDFLAG(IS_MAC)
// Tests that an extension popup closes when activating the browser window
// in macOS fullscreen.
IN_PROC_BROWSER_TEST_F(
    ExtensionPopupInteractiveUiTest,
    ExtensionPopupClosesOnActivatingBrowserWindowMacFullscreen) {
  // Install a test extension.
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

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Activate the browser window should close the extension popup.
  browser()->window()->Activate();
  ExpectWidgetDestroy(extension_popup_widget);
}
#endif

// Tests that an extension popup does not close when activating an unrelated
// top-level widget. This behavior is useful for users who want to keep the
// popup open to look at the info there while working on other apps or browser
// windows.
IN_PROC_BROWSER_TEST_F(
    ExtensionPopupInteractiveUiTest,
    ExtensionPopupDoesNotClosesOnActivatingOtherTopLevelWindow) {
  // Install a test extension.
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
  base::WeakPtr<views::Widget> extension_popup_widget =
      OpenExtensionPopup(browser(), extension);

  // Activate a different top-level widget should not close the extension popup.
  views::UniqueWidgetPtr widget = CreateTestTopLevelWidget();
  widget->Show();
  views::test::WaitForWidgetActive(widget.get(), true);

  ASSERT_NE(extension_popup_widget, nullptr);
  EXPECT_FALSE(extension_popup_widget->IsClosed());
}

// Tests that an API-triggered extenion popup does not show if a security dialog
// is visible. This extension loads a slow image that completes loading only
// after a security dialog is shown.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       APITriggeredPopupIsBlockedBySecurityDialog) {
  // Start an embedded test server that serves a slow responding image.
  static constexpr char kSlowImgURL[] = "/slow-img";
  net::test_server::ControllableHttpResponse slow_img_response(
      embedded_test_server(), kSlowImgURL);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL slow_image_url = embedded_test_server()->GetURL(kSlowImgURL);

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "action": { "default_popup": "popup.html" },
           "version": "0.1"
         })";
  const std::string html =
      base::StrCat({"<html><img src='", slow_image_url.spec(), "'></html>"});

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), html);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Try to open an extension by API.
  extensions::ExtensionHostTestHelper popup_waiter(browser()->profile(),
                                                   extension->id());
  popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
  ExtensionActionTestHelper::Create(browser())->TriggerPopupForAPI(
      extension->id());

  // The extension should load the image.
  slow_img_response.WaitForRequest();

  // While the extension is loading, open a security UI.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::UniqueWidgetPtr security_widget =
      CreateTestDialogWidget(browser_view->GetWidget());
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      security_widget.get());
  security_widget->Show();
  views::test::WidgetVisibleWaiter(security_widget.get()).Wait();

  // Respond to the image loading.
  slow_img_response.Send(net::HTTP_OK, "image/png");
  slow_img_response.Send("image_body");
  slow_img_response.Done();

  // The extension should be destroyed without showing.
  popup_waiter.WaitForHostDestroyed();
}

// Tests that a user-triggered extenion popup is blocked by visible security
// dialogs.
IN_PROC_BROWSER_TEST_F(ExtensionPopupInteractiveUiTest,
                       UserTriggeredPopupIsBlockedBySecurityUI) {
  // Start an embedded test server that serves a slow responding image.
  static constexpr char kSlowImgURL[] = "/slow-img";
  net::test_server::ControllableHttpResponse slow_img_response(
      embedded_test_server(), kSlowImgURL);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL slow_image_url = embedded_test_server()->GetURL(kSlowImgURL);

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "action": { "default_popup": "popup.html" },
           "version": "0.1"
         })";
  const std::string html =
      base::StrCat({"<html><img src='", slow_image_url.spec(), "'></html>"});

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), html);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Try to open an extension.
  extensions::ExtensionHostTestHelper popup_waiter(browser()->profile(),
                                                   extension->id());
  popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
  ExtensionActionTestHelper::Create(browser())->Press(extension->id());

  // The extension should load the image.
  slow_img_response.WaitForRequest();

  // While the extension is loading, open a security UI.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::UniqueWidgetPtr security_widget =
      CreateTestDialogWidget(browser_view->GetWidget());
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      security_widget.get());
  security_widget->Show();
  views::test::WidgetVisibleWaiter(security_widget.get()).Wait();

  // Respond to the image loading.
  slow_img_response.Send(net::HTTP_OK, "image/png");
  slow_img_response.Send("image_body");
  slow_img_response.Done();

  // The extension should be destroyed without showing.
  popup_waiter.WaitForHostDestroyed();
}
