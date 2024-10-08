// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

using ScriptingPermissionsModifier = extensions::ScriptingPermissionsModifier;
using PermissionsManager = extensions::PermissionsManager;

constexpr char kInjectionSucceededMessage[] = "injection succeeded";

views::Widget* CreateBubble(views::View* anchor_point) {
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder().SetTitle(u"Title").Build();
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_point, views::BubbleBorder::TOP_RIGHT);

  return views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
}

}  // namespace

using UserSiteAccess = extensions::PermissionsManager::UserSiteAccess;
using SiteInteraction = extensions::SitePermissionsHelper::SiteInteraction;

class ExtensionsToolbarContainerUITest : public ExtensionsToolbarUITest {
 public:
  enum class ExtensionRemovalMethod {
    kDisable,
    kUninstall,
    kBlocklist,
    kTerminate,
  };

  ExtensionsToolbarContainerUITest() = default;
  ExtensionsToolbarContainerUITest(const ExtensionsToolbarContainerUITest&) =
      delete;
  ExtensionsToolbarContainerUITest& operator=(
      const ExtensionsToolbarContainerUITest&) = delete;
  ~ExtensionsToolbarContainerUITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionsToolbarUITest::SetUpOnMainThread();
    // InProcessBrowserTest will create the browser and request the hosting
    // NativeWidget's window activate. However, the NativeWidget's window can
    // resolve this activation request asynchronously. Showing the extension
    // popup is also done asynchronously.
    // Extension popups will close their bubble Widget if the hosting window
    // recieves activation. If we do not wait for the activation first this
    // results in a race condition whereby if the bubble is created first it is
    // immediately closed when the activation event is propagated. Thus ensure
    // we first wait for the browser window activation here.
    ui_test_utils::BrowserActivationWaiter(browser()).WaitForActivation();
  }

  void ClickOnAction(ToolbarActionView* action) {
    ui::MouseEvent click_down_event(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::EventType::kMouseReleased, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
    action->OnMouseEvent(&click_down_event);
    action->OnMouseEvent(&click_up_event);
  }

  void ShowUi(const std::string& name) override { NOTREACHED(); }

  void RemoveExtension(ExtensionRemovalMethod method,
                       const std::string& extension_id) {
    extensions::ExtensionService* const extension_service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    switch (method) {
      case ExtensionRemovalMethod::kDisable:
        extension_service->DisableExtension(
            extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
        break;
      case ExtensionRemovalMethod::kUninstall:
        extension_service->UninstallExtension(
            extension_id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
        break;
      case ExtensionRemovalMethod::kBlocklist:
        extension_service->BlocklistExtensionForTest(extension_id);
        break;
      case ExtensionRemovalMethod::kTerminate:
        extension_service->TerminateExtension(extension_id);
        break;
    }

    // Removing an extension can result in the container changing visibility.
    // Allow it to finish laying out appropriately.
    auto* container = GetExtensionsToolbarContainer();
    container->GetWidget()->LayoutRootViewIfNecessary();
  }

  void VerifyContainerVisibility(ExtensionRemovalMethod method,
                                 bool expected_visibility) {
    // An empty container should not be shown.
    EXPECT_FALSE(GetExtensionsToolbarContainer()->GetVisible());

    // Loading the first extension should show the button (and container).
    LoadTestExtension("extensions/uitest/long_name");
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Add another extension so we can make sure that removing some don't change
    // the visibility.
    LoadTestExtension("extensions/uitest/window_open");

    // Remove 1/2 extensions, should still be drawn.
    RemoveExtension(method, extensions()[0]->id());
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Removing the last extension. All actions now have the same state.
    RemoveExtension(method, extensions()[1]->id());

    // Container should remain visible during the removal animation.
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
    EXPECT_EQ(expected_visibility, GetExtensionsToolbarContainer()->IsDrawn());
  }
};

// TODO(devlin): There are probably some tests from
// ExtensionsMenuViewInteractiveUITest that should move here (if they test the
// toolbar container more than the menu).

// Tests that invocation metrics are properly recorded when triggering
// extensions from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest, InvocationMetrics) {
  base::HistogramTester histogram_tester;
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/uitest/extension_with_action_and_command");

  EXPECT_EQ(1u, GetToolbarActionViews().size());
  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);

  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(1u, GetVisibleToolbarActionViews().size());
  ToolbarActionView* const action = GetVisibleToolbarActionViews()[0];

  constexpr char kHistogramName[] = "Extensions.Toolbar.InvocationSource";
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Trigger the extension by clicking on it.
  ClickOnAction(action);

  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      ToolbarActionViewController::InvocationSource::kToolbarButton, 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       InvisibleWithoutExtension_Disable) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kDisable, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       InvisibleWithoutExtension_Uninstall) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kUninstall, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       InvisibleWithoutExtension_Blocklist) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kBlocklist, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       InvisibleWithoutExtension_Terminate) {
  // TODO(pbos): Keep the container visible when extensions are terminated
  // (crash). This lets users find and restart them. Then update this test
  // expectation to be kept visible by terminated extensions. Also update the
  // test name to reflect that the container should be visible with only
  // terminated extensions.
  VerifyContainerVisibility(ExtensionRemovalMethod::kTerminate, false);
}

// Tests that clicking on a second extension action will close a first if its
// popup was open.
// TODO(crbug.com/332299695): Test failing on linux-lacros-tester-rel.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ClickingOnASecondActionClosesTheFirst \
  DISABLED_ClickingOnASecondActionClosesTheFirst
#else
#define MAYBE_ClickingOnASecondActionClosesTheFirst \
  ClickingOnASecondActionClosesTheFirst
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       MAYBE_ClickingOnASecondActionClosesTheFirst) {
  std::vector<extensions::TestExtensionDir> test_dirs;
  auto load_extension = [&](const char* extension_name) {
    constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "action": { "default_popup": "popup.html" },
             "version": "0.1"
           })";
    constexpr char kPopupHtml[] =
        R"(<html><script src="popup.js"></script></html>)";
    constexpr char kPopupJsTemplate[] =
        R"(chrome.test.sendMessage('%s popup opened');)";

    extensions::TestExtensionDir test_dir;
    test_dir.WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name));
    test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
    test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"),
                       base::StringPrintf(kPopupJsTemplate, extension_name));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(browser()->profile())
            .LoadExtension(test_dir.UnpackedPath());
    test_dirs.push_back(std::move(test_dir));
    return extension;
  };

  // Load up a couple extensions with actions in the toolbar.
  scoped_refptr<const extensions::Extension> alpha = load_extension("alpha");
  ASSERT_TRUE(alpha);
  scoped_refptr<const extensions::Extension> beta = load_extension("beta");
  ASSERT_TRUE(beta);

  // Pin each to the toolbar, and grab their views.
  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(alpha->id(), true);
  model->SetActionVisibility(beta->id(), true);

  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  auto toolbar_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(2u, toolbar_views.size());

  ToolbarActionView* const alpha_action = toolbar_views[0];
  EXPECT_EQ(alpha->id(), alpha_action->view_controller()->GetId());
  ToolbarActionView* const beta_action = toolbar_views[1];
  EXPECT_EQ(beta->id(), beta_action->view_controller()->GetId());

  extensions::ProcessManager* const process_manager =
      extensions::ProcessManager::Get(profile());

  // To start, neither extensions should have any render frames (which here
  // equates to no open popus).
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());

  {
    // Click on Alpha and wait for it to open the popup.
    ExtensionTestMessageListener listener("alpha popup opened");
    ClickOnAction(alpha_action);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that Alpha (and only Alpha) has an active frame (i.e., popup).
  ASSERT_EQ(
      1u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());
  // And confirm this matches the underlying controller's state.
  EXPECT_TRUE(alpha_action->view_controller()->IsShowingPopup());
  EXPECT_FALSE(beta_action->view_controller()->IsShowingPopup());

  {
    // Click on Beta. This should result in Beta's popup opening and Alpha's
    // closing.
    content::RenderFrameHost* const popup_frame =
        *process_manager->GetRenderFrameHostsForExtension(alpha->id()).begin();
    content::WebContentsDestroyedWatcher popup_destroyed(
        content::WebContents::FromRenderFrameHost(popup_frame));
    ExtensionTestMessageListener listener("beta popup opened");
    ClickOnAction(beta_action);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    popup_destroyed.Wait();
  }

  // Beta (and only Beta) should have an active popup.
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  ASSERT_EQ(
      1u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());
  EXPECT_FALSE(alpha_action->view_controller()->IsShowingPopup());
  EXPECT_TRUE(beta_action->view_controller()->IsShowingPopup());
}

// Tests that clicking an extension toolbar icon when the popup is open closes
// the popup.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       DoubleClickToolbarActionToClose) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/ui/browser_action_popup");
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);
  EXPECT_TRUE(toolbar_model->IsActionPinned(extension->id()));

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  views::test::WaitForAnimatingLayoutManager(container);

  EXPECT_TRUE(container->IsActionVisibleOnToolbar(extension->id()));
  ToolbarActionView* const action_view =
      container->GetViewForId(extension->id());
  EXPECT_TRUE(action_view->GetVisible());

  ExtensionTestMessageListener listener("Popup opened");
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
      ui_test_utils::GetCenterInScreenCoordinates(action_view)));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ToolbarActionViewController* const view_controller =
      container->GetActionForId(extension->id());
  EXPECT_TRUE(view_controller->IsShowingPopup());
  EXPECT_EQ(view_controller, container->popup_owner_for_testing());

  extensions::ExtensionHostTestHelper host_helper(profile(), extension->id());
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));
  host_helper.WaitForHostDestroyed();

  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, container->popup_owner_for_testing());

  // Releasing the mouse shouldn't result in the popup being shown again.
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, container->popup_owner_for_testing());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       ShowWidgetForExtension_Pinned) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);
  container->GetWidget()->LayoutRootViewIfNecessary();

  auto visible_actions = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_actions.size());
  EXPECT_EQ(extension->id(), visible_actions[0]->view_controller()->GetId());

  views::Widget* bubble = CreateBubble(container->GetExtensionsButton());
  container->ShowWidgetForExtension(bubble, extension->id());

  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();

  EXPECT_TRUE(container->IsActionVisibleOnToolbar(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       ShowWidgetForExtension_Unpinned) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  views::Widget* bubble = CreateBubble(container->GetExtensionsButton());
  container->ShowWidgetForExtension(bubble, extension->id());

  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_TRUE(container->IsActionVisibleOnToolbar(extension->id()));

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();

  EXPECT_FALSE(container->IsActionVisibleOnToolbar(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       ShowWidgetForExtension_NoAction) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  // Disable the extension. Disabled extensions don't display in the toolbar.
  extensions::ExtensionService* const extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->DisableExtension(
      extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  EXPECT_FALSE(container->GetActionForId(extension->id()));

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  views::Widget* bubble = CreateBubble(container->GetExtensionsButton());
  container->ShowWidgetForExtension(bubble, extension->id());

  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       UninstallExtensionWithActivelyShownWidget) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);
  container->GetWidget()->LayoutRootViewIfNecessary();

  auto visible_actions = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_actions.size());
  EXPECT_EQ(extension->id(), visible_actions[0]->view_controller()->GetId());

  views::Widget* bubble = CreateBubble(container->GetExtensionsButton());
  container->ShowWidgetForExtension(bubble, extension->id());

  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  extensions::ExtensionService* const extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());
  EXPECT_FALSE(container->GetActionForId(extension->id()));

  // TODO(devlin): When the extension is removed, we don't currently remove any
  // widgets associated with it. This test ensures we don't crash (yay!), but we
  // should very likely close the bubble as well. I wouldn't be surprised if
  // some bubble handlers don't expect the extension to be gone.
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  destroyed_waiter.Wait();
}

// Verifies that dragging extension icons is disabled in incognito windows.
// https://crbug.com/1203833.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       IncognitoDraggingIsDisabled) {
  // Load an extension, pin it, and enable it in incognito.
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);

  {
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(profile()), extension->id());
    extensions::util::SetIsIncognitoEnabled(extension->id(), profile(), true);
    ASSERT_TRUE(observer.WaitForExtensionLoaded());
  }

  Browser* incognito_browser = CreateIncognitoBrowser();

  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  views::test::WaitForAnimatingLayoutManager(
      GetExtensionsToolbarContainerForBrowser(incognito_browser));

  // Verify the extension has a (visible) action for both the incognito and
  // on-the-record browser.
  std::vector<ToolbarActionView*> on_the_record_views = GetToolbarActionViews();
  ASSERT_EQ(1u, on_the_record_views.size());
  ToolbarActionView* on_the_record_view = on_the_record_views[0];
  EXPECT_EQ(extension->id(), on_the_record_view->view_controller()->GetId());
  EXPECT_TRUE(on_the_record_view->GetVisible());

  std::vector<ToolbarActionView*> incognito_views =
      GetToolbarActionViewsForBrowser(incognito_browser);
  ASSERT_EQ(1u, incognito_views.size());
  ToolbarActionView* incognito_view = incognito_views[0];
  EXPECT_EQ(extension->id(), incognito_view->view_controller()->GetId());
  EXPECT_TRUE(incognito_view->GetVisible());

  // Dragging should be enabled for the on-the-record view, but not the
  // incognito view.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            on_the_record_view->GetDragOperationsForTest(gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            incognito_view->GetDragOperationsForTest(gfx::Point()));

  // The two views should have the same notifiable event. This is important to
  // test, since it can be dependent on draggability.
  EXPECT_EQ(on_the_record_view->button_controller()->notify_action(),
            incognito_view->button_controller()->notify_action());
}

// Tests unloading an extension while the action is being slid out prior to the
// popup being shown. Regression test for https://crbug.com/1345477.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       UnloadingExtensionWhileAboutToShowPopup) {
  // Load an extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  // It should be unpinned.
  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  EXPECT_FALSE(container->IsActionVisibleOnToolbar(extension->id()));

  // Execute the action, which results in the extension sliding out while we
  // get ready to show the popup.
  ToolbarActionViewController* const view_controller =
      container->GetActionForId(extension->id());
  view_controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kMenuEntry);

  // Unload the extension (before the popup is ready). This results in the
  // toolbar action being removed. The pending popup will never be shown. This
  // shouldn't crash.
  RemoveExtension(ExtensionRemovalMethod::kDisable, extension->id());

  EXPECT_EQ(nullptr, container->GetActionForId(extension->id()));
}

namespace {

class IncognitoExtensionsToolbarContainerUITest
    : public ExtensionsToolbarContainerUITest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionsToolbarContainerUITest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

}  // namespace

// Tests that first loading an extension action in an incognito profile, then
// removing the incognito profile and using the extension action in a normal
// profile doesn't crash.
// Regression test for crbug.com/663726.
IN_PROC_BROWSER_TEST_F(IncognitoExtensionsToolbarContainerUITest,
                       TestExtensionFirstLoadedInIncognitoMode) {
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());

  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/api_test/browser_action_with_icon",
                        /*allow_incognito=*/true);
  ASSERT_TRUE(extension);
  Browser* second_browser = CreateBrowser(profile()->GetOriginalProfile());
  EXPECT_FALSE(second_browser->profile()->IsOffTheRecord());

  CloseBrowserSynchronously(browser());

  std::vector<ToolbarActionView*> extension_views =
      GetToolbarActionViewsForBrowser(second_browser);
  ASSERT_EQ(1u, extension_views.size());

  gfx::ImageSkia icon = extension_views[0]->GetIconForTest();
  // Force the image to try and load a representation.
  icon.GetRepresentation(2.0);
}

class ExtensionsToolbarRuntimeHostPermissionsBrowserTest
    : public ExtensionsToolbarContainerUITest,
      public testing::WithParamInterface<bool> {
 public:
  enum class ContentScriptRunLocation {
    DOCUMENT_START,
    DOCUMENT_IDLE,
  };

  ExtensionsToolbarRuntimeHostPermissionsBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        extensions_features::kExtensionsMenuAccessControl, GetParam());
  }
  ExtensionsToolbarRuntimeHostPermissionsBrowserTest(
      const ExtensionsToolbarRuntimeHostPermissionsBrowserTest&) = delete;
  ExtensionsToolbarRuntimeHostPermissionsBrowserTest& operator=(
      const ExtensionsToolbarRuntimeHostPermissionsBrowserTest&) = delete;
  ~ExtensionsToolbarRuntimeHostPermissionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionsToolbarContainerUITest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void LoadAllUrlsExtension(ContentScriptRunLocation run_location) {
    std::string run_location_str;
    switch (run_location) {
      case ContentScriptRunLocation::DOCUMENT_START:
        run_location_str = "document_start";
        break;
      case ContentScriptRunLocation::DOCUMENT_IDLE:
        run_location_str = "document_idle";
        break;
    }
    extension_dir_.WriteManifest(base::StringPrintf(R"({
             "name": "All Urls Extension",
             "description": "Runs a content script everywhere",
             "manifest_version": 2,
             "version": "0.1",
             "content_scripts": [{
               "matches": ["<all_urls>"],
               "js": ["script.js"],
               "run_at": "%s"
             }]
           })",
                                                    run_location_str.c_str()));
    extension_dir_.WriteFile(
        FILE_PATH_LITERAL("script.js"),
        base::StringPrintf("chrome.test.sendMessage('%s');",
                           kInjectionSucceededMessage));

    extension_ = extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
        extension_dir_.UnpackedPath());
    ASSERT_TRUE(extension_);
    AppendExtension(extension_);
    extensions::ScriptingPermissionsModifier(profile(), extension_)
        .SetWithholdHostPermissions(true);
  }

  const extensions::Extension* extension() const { return extension_.get(); }

  extensions::ExtensionContextMenuModel* GetExtensionContextMenu() {
    ToolbarActionViewController* const controller =
        GetExtensionsToolbarContainer()->GetActionForId(extension_->id());
    return static_cast<extensions::ExtensionContextMenuModel*>(
        controller->GetContextMenu(extensions::ExtensionContextMenuModel::
                                       ContextMenuSource::kToolbarAction));
  }

  std::u16string GetActionTooltip() {
    return GetExtensionsToolbarContainer()
        ->GetViewForId(extension_->id())
        ->GetTooltipText();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  extensions::TestExtensionDir extension_dir_;
  scoped_refptr<const extensions::Extension> extension_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
    // False disables kExtensionsMenuAccessControl feature, true enables it.
    testing::Bool(),
    [](const testing::TestParamInfo<
        ExtensionsToolbarRuntimeHostPermissionsBrowserTest::ParamType>& info) {
      return info.param ? "ExtensionsMenuAccessControlFeatureEnabled"
                        : "ExtensionsMenuAccessControlFeatureDisabled";
    });

// Tests page access modifications through the context menu which require a page
// refresh.
IN_PROC_BROWSER_TEST_P(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_START);

  // When feature is enabled, tooltip is empty since a hover card is shown
  // instead. Testing for the hover card is done separately.
  bool feature_enabled = GetParam();
  std::u16string tooltip_wants_access =
      feature_enabled
          ? std::u16string()
          : base::JoinString({u"All Urls Extension",
                              l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
                             u"\n");
  std::u16string tooltip_has_access =
      feature_enabled
          ? std::u16string()
          : base::JoinString(
                {u"All Urls Extension",
                 l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
                u"\n");

  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage);
  injection_listener.set_extension_id(extension()->id());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  extensions::PermissionsManager* permissions_manager =
      extensions::PermissionsManager::Get(profile());

  // Navigate to urlA. The extension should have withheld access.
  GURL urlA = embedded_test_server()->GetURL("example.com", "/title1.html");
  {
    content::TestNavigationObserver observer(web_contents);
    extensions::browsertest_util::BlockedActionWaiter blocked_action_waiter(
        runner);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), urlA));
    EXPECT_TRUE(observer.last_navigation_succeeded());

    blocked_action_waiter.Wait();
    EXPECT_TRUE(runner->WantsToRun(extension()));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension(), urlA));
    EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
    EXPECT_FALSE(injection_listener.was_satisfied());
  }

  {
    // Open the extension's context menu.
    extensions::ExtensionContextMenuModel* extension_menu =
        GetExtensionContextMenu();
    ASSERT_TRUE(extension_menu);

    // Allow the extension to run on this site. This should show a refresh page
    // bubble. Accept the bubble.
    content::TestNavigationObserver observer(web_contents);
    runner->accept_bubble_for_testing(true);
    extension_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        /*event_flags=*/0);
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // The extension should have injected and the extension should no longer
    // want to run.
    ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
    injection_listener.Reset();
    EXPECT_FALSE(runner->WantsToRun(extension()));
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension(), urlA));
    EXPECT_EQ(tooltip_has_access, GetActionTooltip());
  }

  // Now navigate to a different host. The extension should have blocked
  // actions.
  GURL urlB = embedded_test_server()->GetURL("abc.com", "/title1.html");
  {
    content::TestNavigationObserver observer(web_contents);
    extensions::browsertest_util::BlockedActionWaiter blocked_action_waiter(
        runner);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), urlB));
    EXPECT_TRUE(observer.last_navigation_succeeded());

    blocked_action_waiter.Wait();
    EXPECT_TRUE(runner->WantsToRun(extension()));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension(), urlB));
    EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
    EXPECT_FALSE(injection_listener.was_satisfied());
  }

  {
    // Re open the menu again, since the menu contents don't update dynamically.
    extensions::ExtensionContextMenuModel* extension_menu =
        GetExtensionContextMenu();
    ASSERT_TRUE(extension_menu);

    // Allow the extension to run on all sites this time. This should again show
    // a refresh bubble. Dismiss it.
    runner->accept_bubble_for_testing(false);
    extension_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES,
        /*event_flags=*/0);

    // Permissions to the extension should now be been granted, and the
    // extension should still be in wants-to-run state because we didn't refresh
    // the page.
    EXPECT_TRUE(runner->WantsToRun(extension()));
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension(), urlB));
    EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
    EXPECT_FALSE(injection_listener.was_satisfied());
  }
}

// Tests page access modifications through the context menu which don't require
// a page refresh.
IN_PROC_BROWSER_TEST_P(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshNotRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_IDLE);

  // When feature is enabled, tooltip is empty since a hover card is shown
  // instead. Testing for the hover card is done separately.
  bool feature_enabled = GetParam();
  std::u16string tooltip_wants_access =
      feature_enabled
          ? std::u16string()
          : base::JoinString({u"All Urls Extension",
                              l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
                             u"\n");
  std::u16string tooltip_has_access =
      feature_enabled
          ? std::u16string()
          : base::JoinString(
                {u"All Urls Extension",
                 l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
                u"\n");

  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage);
  injection_listener.set_extension_id(extension()->id());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  extensions::browsertest_util::BlockedActionWaiter blocked_action_waiter(
      runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.Wait();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  extensions::PermissionsManager* permissions_manager =
      extensions::PermissionsManager::Get(profile());
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension(), url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());

  extensions::ExtensionContextMenuModel* extension_menu =
      GetExtensionContextMenu();
  ASSERT_TRUE(extension_menu);

  // Allow the extension to run on this site. Since the blocked actions don't
  // require a refresh, the permission should be granted and the page actions
  // should run.
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
      0 /* event_flags */);
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  EXPECT_FALSE(runner->WantsToRun(extension()));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension(), url));
  EXPECT_EQ(tooltip_has_access, GetActionTooltip());
}

// Avoid adding test to this class. Instead, prefer using
// ExtensionsToolbarContainerFeatureInteractiveTest that uses a better test
// framework.
// TODO(crbug.com/368205259): Migrate tests to use
// ExtensionsToolbarContainerFeatureInteractiveTest.
class ExtensionsToolbarContainerFeatureUITest
    : public ExtensionsToolbarContainerUITest {
 public:
  ExtensionsToolbarContainerFeatureUITest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ExtensionsToolbarContainerFeatureUITest(
      const ExtensionsToolbarContainerFeatureUITest&) = delete;
  const ExtensionsToolbarContainerFeatureUITest& operator=(
      const ExtensionsToolbarContainerFeatureUITest&) = delete;
  ~ExtensionsToolbarContainerFeatureUITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionsToolbarContainerUITest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateToUrl(const GURL& url) {
    NavigateTo(url);
    WaitForAnimation();
    GetExtensionsToolbarContainer()->GetWidget()->LayoutRootViewIfNecessary();
  }

  ExtensionsRequestAccessButton* request_access_button() {
    return GetExtensionsToolbarContainer()->GetRequestAccessButton();
  }

  ExtensionsToolbarButton* extensions_toolbar_button() {
    return GetExtensionsToolbarContainer()->GetExtensionsButton();
  }

  ExtensionsMenuCoordinator* extensions_menu_coordinator() {
    return GetExtensionsToolbarContainer()
        ->GetExtensionsMenuCoordinatorForTesting();
  }

  extensions::ExtensionContextMenuModel* GetContextMenuForExtension(
      const extensions::ExtensionId& extension_id) {
    return static_cast<extensions::ExtensionContextMenuModel*>(
        GetExtensionsToolbarContainer()
            ->GetActionForId(extension_id)
            ->GetContextMenu(extensions::ExtensionContextMenuModel::
                                 ContextMenuSource::kMenuItem));
  }

  // Returns the visible children in the order they appear on the extensions
  // toolbar container.
  std::vector<views::View*> GetVisibleChildrenInContainer() {
    std::vector<views::View*> visible_children;
    for (views::View* child : GetExtensionsToolbarContainer()->children()) {
      if (child->GetVisible()) {
        visible_children.push_back(child);
      }
    }
    return visible_children;
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
};

class ExtensionsToolbarContainerFeatureUIReloadBubbleAcceptanceTest
    : public ExtensionsToolbarContainerFeatureUITest,
      public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    ExtensionsToolbarContainerFeatureUIReloadBubbleAcceptanceTest,
    // False does not accept the refresh bubble, true accepts.
    testing::Bool(),
    [](const testing::TestParamInfo<
        ExtensionsToolbarContainerFeatureUIReloadBubbleAcceptanceTest::
            ParamType>& info) {
      return info.param ? "ReloadBubbleAccepted" : "ReloadBubbleDismissed";
    });

// Tests that when clicking the request access button (and a refresh should be
// required to run blocked actions) it always grants site access to the
// extensions listed. Blocked actions are only run if the refresh is accepted.
IN_PROC_BROWSER_TEST_P(
    ExtensionsToolbarContainerFeatureUIReloadBubbleAcceptanceTest,
    ClickingRequestAccessButtonRunsAction_RefreshRequired) {
  auto extensionA = InstallExtensionWithHostPermissions(
      "A Extension", "<all_urls>",
      /*content_script_run_location=*/"document_start");
  auto extensionB =
      InstallExtensionWithHostPermissions("B Extension", "http://example.com/");
  auto extensionC =
      InstallExtensionWithHostPermissions("C Extension", "<all_urls>");

  // Withheld site access for extensions A and B.
  ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  ScriptingPermissionsModifier(profile(), extensionB)
      .SetWithholdHostPermissions(true);

  // Navigate to a site where extensions A and B have withheld access.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);

  // Add site access requests for extensions A and B.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extensionA, web_contents);
  AddSiteAccessRequest(*extensionB, web_contents);
  WaitForAnimation();

  // Verify request access button is visible because extensions A and B have
  // site access requests.
  extensions::SitePermissionsHelper permissions_helper(browser()->profile());
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser()->profile());
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extensionA->id(), extensionB->id()));

  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionA, web_contents),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionB, web_contents),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionC, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionA, url),
            UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionB, url),
            UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionC, url),
            UserSiteAccess::kOnAllSites);

  // Don't show the confirmation since it's dependent on time, and we have other
  // tests for it.
  request_access_button()->remove_confirmation_for_testing(true);

  // Click the request access button to always grants site access. A reload
  // page dialog will appear since extension A needs a page reload to run its
  // action.
  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  const bool kReloadBubbleAccepted = GetParam();
  action_runner->accept_bubble_for_testing(kReloadBubbleAccepted);
  ExtensionTestMessageListener script_injection_listener("injection succeeded");
  ClickButton(request_access_button());
  WaitForAnimation();

  if (kReloadBubbleAccepted) {
    // Site interaction should change and script should be injected since
    // permission granted and page was reloaded. The request access button
    // should be hidden since we reloaded.
    EXPECT_TRUE(script_injection_listener.WaitUntilSatisfied());
    EXPECT_FALSE(request_access_button()->GetVisible());
  } else {
    // Site interaction should change but script should not be injected since
    // permission was granted but page was not reloaded. The request access
    // button should be hidden, even without a reload, because the user granted
    // access to the extensions.
    EXPECT_FALSE(request_access_button()->GetVisible());
    // TODO(crbug.com/40883928): Is there a way to confirm we didn't inject the
    // script besides reusing the
    // chrome/test/data/extensions/blocked_actions/content_scripts/ test
    // extension?
  }

  // Extension A and B should have 'granted' site interaction, since their
  // actions ran, and 'on site' site access.
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionA, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionB, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionC, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionA, url),
            UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionB, url),
            UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionC, url),
            UserSiteAccess::kOnAllSites);
}

// Tests that the extension menu (puzzle piece menu) closes alongside the
// extensions context menu (3 dot while puzzle piece menu is open) when the side
// panel context menu item is selected.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerFeatureUITest,
                       SidePanelContextMenuItemClosesExtensionsMenu) {
  scoped_refptr<const extensions::Extension> extension =
      InstallExtension("Extension");

  EXPECT_FALSE(extensions_menu_coordinator()->IsShowing());

  ClickButton(extensions_toolbar_button());
  EXPECT_TRUE(extensions_menu_coordinator()->IsShowing());

  // Simulate selecting the "Open side panel" menu item.
  extensions::ExtensionContextMenuModel* menu =
      GetContextMenuForExtension(extension->id());
  menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::TOGGLE_SIDE_PANEL_VISIBILITY, 0);
  menu->MenuClosed(menu);

  EXPECT_FALSE(extensions_menu_coordinator()->IsShowing());
}

// Tests that clicking the request access button always grants site access to
// the extensions listed without needing a page refresh.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarContainerFeatureUITest,
    ClickingRequestAccessButtonRunsAction_RefreshNotRequired) {
  constexpr char kExtensionAName[] = "A Extension";
  constexpr char kExtensionBName[] = "B Extension";
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionA = InstallExtensionWithHostPermissions(
      kExtensionAName, "<all_urls>", "document_idle");
  auto extensionB = InstallExtensionWithHostPermissions(kExtensionBName,
                                                        "http://example.com/");
  auto extensionC =
      InstallExtensionWithHostPermissions(kExtensionCName, "<all_urls>");

  // Withheld site access for extensions A and B.
  ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  ScriptingPermissionsModifier(profile(), extensionB)
      .SetWithholdHostPermissions(true);

  // Navigate to a site where extensions A and B have withheld access.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);

  // Add site access requests for extensions A and B.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extensionA, web_contents);
  AddSiteAccessRequest(*extensionB, web_contents);
  WaitForAnimation();

  // Verify request access button is visible because extensions A and B have
  // site access requests.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extensionA->id(), extensionB->id()));
  extensions::SitePermissionsHelper permissions_helper(browser()->profile());
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionA, web_contents),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionB, web_contents),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionC, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionA, url),
            UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionB, url),
            UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionC, url),
            UserSiteAccess::kOnAllSites);

  // Don't show the confirmation since it's dependent on time, and we have other
  // tests for it.
  request_access_button()->remove_confirmation_for_testing(true);

  // Click the request access button to always grant site access. Since no
  // extensions need page refresh to run their actions, it immediately grants
  // access and the script is injected.
  ExtensionTestMessageListener script_injection_listener("injection succeeded");
  ClickButton(request_access_button());
  EXPECT_TRUE(script_injection_listener.WaitUntilSatisfied());
  WaitForAnimation();

  // Extension A and B should have 'granted' site interaction, since their
  // actions ran, and 'on site' site access. The request access button should be
  // hidden.
  EXPECT_FALSE(request_access_button()->GetVisible());
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionA, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionB, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_helper.GetSiteInteraction(*extensionC, web_contents),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionA, url),
            UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionB, url),
            UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extensionC, url),
            UserSiteAccess::kOnAllSites);
}

// Tests that when the user clicks on the request access button and immediately
// navigates to a different site, the confirmation text is collapsed and the
// button displays the extensions requesting access to the new site (if any).
// TODO(crbug.com/40918196): Flaky due to button's confirmation text animation.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarContainerFeatureUITest,
    DISABLED_ClickingRequestAccessButton_ConfirmationCollapsedOnNavigation) {
  auto extension = InstallExtensionWithHostPermissions(
      "Extension", "<all_urls>", "document_idle");
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // Navigate to a site where the extension has withheld access and add a site
  // access request for the extension.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAnimation();

  // Verify request access button is visible because extension added a site
  // access request.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Click the button to grant one-time access on example.com. Verify
  // confirmation message appears on the request access button.
  ClickButton(request_access_button());
  WaitForAnimation();
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // While the confirmation message is still visible, navigate to a site where
  // the extension has withheld access. Verify the button is not visible because
  // there are no site access requests added and confirmation is gone.
  NavigateToUrl(embedded_test_server()->GetURL("other.com", "/title1.html"));
  EXPECT_FALSE(request_access_button()->GetVisible());
}

// Tests that the container has its visible children in the correct order when
// there are dynamic updates (e.g extension is installed).
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerFeatureUITest,
                       CorrectReorderAfterDynamicChanges) {
  // Install extension A, withhold its host permissions (for request access
  // button to be visible) and pin it.
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", "<all_urls>");
  extensions::ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extensionA->id(), true);

  // Navigate to a site where extension A has withheld host permissions.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);

  // Add site access request for extension A. This should make the request
  // access button visible.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extensionA, web_contents);

  // Verify order of visible items in container:
  //   A | ExtensionsRequestAccessButton | ExtensionsToolbarButton
  std::vector<views::View*> visible_children = GetVisibleChildrenInContainer();
  EXPECT_EQ(visible_children.size(), 3u);
  EXPECT_TRUE(views::IsViewClass<ToolbarActionView>(visible_children[0]));
  EXPECT_EQ(views::AsViewClass<ToolbarActionView>(visible_children[0])
                ->view_controller()
                ->GetActionName(),
            u"Extension A");
  EXPECT_TRUE(
      views::IsViewClass<ExtensionsRequestAccessButton>(visible_children[1]));
  EXPECT_TRUE(views::IsViewClass<ExtensionsToolbarButton>(visible_children[2]));

  // Add a new extension B that has an action (so action can pop out when
  // triggered).
  constexpr char kExtensionBManifest[] =
      R"({
             "name": "Extension B",
             "manifest_version": 3,
             "action": { "default_popup": "popup.html" },
             "version": "0.1"
           })";
  constexpr char kPopupHtml[] =
      R"(<html><script src="popup.js"></script></html>)";
  constexpr char kPopupJsTemplate[] =
      R"(chrome.test.sendMessage('popup opened');)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kExtensionBManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJsTemplate);
  scoped_refptr<const extensions::Extension> extensionB =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(test_dir.UnpackedPath());

  // Trigger the extension B action.
  ExtensionTestMessageListener listener("popup opened");
  ToolbarActionViewController* const view_controller =
      GetExtensionsToolbarContainer()->GetActionForId(extensionB->id());
  view_controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kMenuEntry);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Verify order of visible items in container:
  //   A | B | ExtensionsRequestAccessButton | ExtensionsToolbarButton
  visible_children = GetVisibleChildrenInContainer();
  EXPECT_EQ(visible_children.size(), 4u);
  EXPECT_TRUE(views::IsViewClass<ToolbarActionView>(visible_children[0]));
  EXPECT_EQ(views::AsViewClass<ToolbarActionView>(visible_children[0])
                ->view_controller()
                ->GetActionName(),
            u"Extension A");
  EXPECT_TRUE(views::IsViewClass<ToolbarActionView>(visible_children[1]));
  EXPECT_EQ(views::AsViewClass<ToolbarActionView>(visible_children[1])
                ->view_controller()
                ->GetActionName(),
            u"Extension B");
  EXPECT_TRUE(
      views::IsViewClass<ExtensionsRequestAccessButton>(visible_children[2]));
  EXPECT_TRUE(views::IsViewClass<ExtensionsToolbarButton>(visible_children[3]));
}

class ExtensionsToolbarContainerFeatureInteractiveTest
    : public InteractiveBrowserTest {
 public:
  ExtensionsToolbarContainerFeatureInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ExtensionsToolbarContainerFeatureInteractiveTest(
      const ExtensionsToolbarContainerFeatureInteractiveTest&) = delete;
  ExtensionsToolbarContainerFeatureInteractiveTest& operator=(
      const ExtensionsToolbarContainerFeatureInteractiveTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    permissions_manager_ = PermissionsManager::Get(browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Null explicitly to avoid dangling pointers.
    permissions_manager_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  scoped_refptr<const extensions::Extension>
  InstallExtensionWithHostPermissions(const std::string& name,
                                      const std::string& host_permission) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(base::StringPrintf(
        R"({
              "name": "%s",
              "manifest_version": 3,
              "host_permissions": ["%s"],
              "version": "0.1"
            })",
        name.c_str(), host_permission.c_str()));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(browser()->profile())
            .LoadExtension(extension_dir.UnpackedPath());
    return extension;
  }

  // Adds a site access request for `extension` on `tab_index`.
  void AddSiteAccessRequest(int tab_index,
                            const extensions::Extension& extension) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    CHECK(web_contents);
    int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
    permissions_manager_->AddSiteAccessRequest(web_contents, tab_id, extension);
  }

  // Removes site access requests for `extension` on `tab_index`.
  void RemoveSiteAccessRequest(int tab_index,
                               const extensions::Extension& extension) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    CHECK(web_contents);
    int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
    permissions_manager_->RemoveSiteAccessRequest(tab_id, extension.id());
  }

  // Returns whether `expected_extensions` match the extensions in the request
  // access button.
  static base::OnceCallback<bool(ExtensionsRequestAccessButton*)>
  CheckExtensionsInRequestAccessButton(
      const std::vector<extensions::ExtensionId>& expected_extensions) {
    return base::BindOnce(
        [](const std::vector<extensions::ExtensionId>& expected_extensions,
           ExtensionsRequestAccessButton* request_access_button) {
          std::vector<extensions::ExtensionId> actual_extensions =
              request_access_button->GetExtensionIdsForTesting();
          return expected_extensions == actual_extensions;
        },
        expected_extensions);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<PermissionsManager> permissions_manager_;
};

// Verifies extensions can add site access requests on active and inactive tabs,
// but the request access button only shows extensions's requests for the
// current tab.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerFeatureInteractiveTest,
                       SiteAccessRequestsForMultipleTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  const GURL first_url("https://one.com/");
  const GURL second_url("https://two.com/");
  int first_tab_index = 0;
  int second_tab_index = 1;

  // Install two extensions and withhold their host permissions, so extensions
  // can add site access requests.
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", "<all_urls>");
  auto extensionB =
      InstallExtensionWithHostPermissions("Extension B", "<all_urls>");
  extensions::ScriptingPermissionsModifier(browser()->profile(), extensionA)
      .SetWithholdHostPermissions(true);
  extensions::ScriptingPermissionsModifier(browser()->profile(), extensionB)
      .SetWithholdHostPermissions(true);

  RunTestSequence(
      // Open two tabs.
      InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, first_url),
      AddInstrumentedTab(kSecondTab, second_url),

      // Activate the first tab. Verify request access button is not visible
      // since no extension has added a request for such tab.
      SelectTab(kTabStripElementId, first_tab_index),
      EnsureNotPresent(kExtensionsRequestAccessButtonElementId),

      // Add a site access request for extension A on the (active) first tab.
      // Verify extension A is visible on the request access button.
      Do([&]() { AddSiteAccessRequest(first_tab_index, *extensionA); }),
      WaitForShow(kExtensionsRequestAccessButtonElementId),
      CheckView(kExtensionsRequestAccessButtonElementId,
                CheckExtensionsInRequestAccessButton({extensionA->id()})),

      // Add a site access request for extension B on the (inactive) second tab.
      // Verify only extension A is visible on the request access button.
      Do([&]() { AddSiteAccessRequest(second_tab_index, *extensionB); }),
      WaitForShow(kExtensionsRequestAccessButtonElementId),
      CheckView(kExtensionsRequestAccessButtonElementId,
                CheckExtensionsInRequestAccessButton({extensionA->id()})),

      // Activate the second tab. Verify extension B is visible on the request
      // access button.
      SelectTab(kTabStripElementId, second_tab_index),
      WaitForShow(kExtensionsRequestAccessButtonElementId),
      CheckView(kExtensionsRequestAccessButtonElementId,
                CheckExtensionsInRequestAccessButton({extensionB->id()})),

      // Remove site access request from the second tab. Verify request access
      // button is no longer visible since no extension have a request for such
      // tab.
      Do([&]() { RemoveSiteAccessRequest(second_tab_index, *extensionB); }),
      WaitForHide(kExtensionsRequestAccessButtonElementId),

      // Activate the first tab. Verify request access button is visible because
      // Extension A request wasn't removed from that tab.
      SelectTab(kTabStripElementId, first_tab_index),
      WaitForShow(kExtensionsRequestAccessButtonElementId),
      CheckView(kExtensionsRequestAccessButtonElementId,
                CheckExtensionsInRequestAccessButton({extensionA->id()})));
}
