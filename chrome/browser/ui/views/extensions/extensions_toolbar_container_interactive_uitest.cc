// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

constexpr char kInjectionSucceededMessage[] = "injection succeeded";

class BlockedActionWaiter
    : public extensions::ExtensionActionRunner::TestObserver {
 public:
  explicit BlockedActionWaiter(extensions::ExtensionActionRunner* runner)
      : runner_(runner), run_loop_(std::make_unique<base::RunLoop>()) {
    runner_->set_observer_for_testing(this);
  }
  BlockedActionWaiter(const BlockedActionWaiter&) = delete;
  BlockedActionWaiter& operator=(const BlockedActionWaiter&) = delete;
  ~BlockedActionWaiter() { runner_->set_observer_for_testing(nullptr); }

  void WaitAndReset() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  // ExtensionActionRunner::TestObserver:
  void OnBlockedActionAdded() override { run_loop_->Quit(); }

  raw_ptr<extensions::ExtensionActionRunner> runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

views::Widget* CreateBubble(views::View* anchor_point) {
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder().SetTitle(u"Title").Build();
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_point, views::BubbleBorder::TOP_RIGHT);

  return views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
}

}  // namespace

using SiteAccess = extensions::SitePermissionsHelper::SiteAccess;
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
    ui::MouseEvent click_down_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
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
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerUITest,
                       ClickingOnASecondActionClosesTheFirst) {
  std::vector<std::unique_ptr<extensions::TestExtensionDir>> test_dirs;
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

    auto test_dir = std::make_unique<extensions::TestExtensionDir>();
    test_dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name));
    test_dir->WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
    test_dir->WriteFile(FILE_PATH_LITERAL("popup.js"),
                        base::StringPrintf(kPopupJsTemplate, extension_name));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(browser()->profile())
            .LoadExtension(test_dir->UnpackedPath());
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
    : public ExtensionsToolbarContainerUITest {
 public:
  enum class ContentScriptRunLocation {
    DOCUMENT_START,
    DOCUMENT_IDLE,
  };

  ExtensionsToolbarRuntimeHostPermissionsBrowserTest() = default;
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
  extensions::TestExtensionDir extension_dir_;
  scoped_refptr<const extensions::Extension> extension_;
};

// Tests page access modifications through the context menu which require a page
// refresh.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_START);
  std::u16string tooltip_wants_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
      u"\n");
  std::u16string tooltip_has_access = base::JoinString(
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
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
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

  // Allow the extension to run on this site. This should show a refresh page
  // bubble. Accept the bubble.
  {
    content::TestNavigationObserver observer(web_contents);
    runner->accept_bubble_for_testing(true);
    extension_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        0 /* event_flags */);
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The extension should have injected and the extension should no longer want
  // to run.
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  injection_listener.Reset();
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension(), url));
  EXPECT_EQ(tooltip_has_access, GetActionTooltip());
  EXPECT_FALSE(runner->WantsToRun(extension()));

  // Now navigate to a different host. The extension should have blocked
  // actions.
  {
    url = embedded_test_server()->GetURL("abc.com", "/title1.html");
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension(), url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());

  // Allow the extension to run on all sites this time. This should again show a
  // refresh bubble. Dismiss it.
  runner->accept_bubble_for_testing(false);
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES,
      0 /* event_flags */);

  // Permissions to the extension shouldn't have been granted, and the extension
  // should still be in wants-to-run state.
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension(), url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());
}

// Tests page access modifications through the context menu which don't require
// a page refresh.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshNotRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_IDLE);
  std::u16string tooltip_wants_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
      u"\n");
  std::u16string tooltip_has_access = base::JoinString(
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
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
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
    content::TestNavigationObserver observer(web_contents_);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
    WaitForAnimation();
  }

  ExtensionsRequestAccessButton* request_access_button() {
    return GetExtensionsToolbarContainer()
        ->GetExtensionsToolbarControls()
        ->request_access_button_for_testing();
  }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

// Tests that clicking the request access button grants one time access to the
// extensions listed which requires a page refresh.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerFeatureUITest,
                       ClickingRequestAccessButtonRunsAction_RefreshRequired) {
  constexpr char kExtensionAName[] = "A Extension";
  constexpr char kExtensionBName[] = "B Extension";
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionA = InstallExtensionWithHostPermissions(
      kExtensionAName, "<all_urls>",
      /*content_script_run_location=*/"document_start");
  auto extensionB = InstallExtensionWithHostPermissions(kExtensionBName,
                                                        "http://example.com/");
  auto extensionC =
      InstallExtensionWithHostPermissions(kExtensionCName, "<all_urls>");

  // Withheld site access for extensions A and B.
  extensions::ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  extensions::ScriptingPermissionsModifier(profile(), extensionB)
      .SetWithholdHostPermissions(true);

  // Navigate to a site where extensions A and B have withheld access.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);

  // Verify request access button is visible because extensions A and B have
  // pending site interaction.
  extensions::SitePermissionsHelper permissions(browser()->profile());
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionsNamesForTesting(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));

  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionA, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionB, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionC, url),
            SiteAccess::kOnAllSites);

  // Click the request access button to grant one-time access. A reload page
  // dialog will appear since extension A needs a page reload to run its action.
  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents());
  action_runner->accept_bubble_for_testing(false);
  ClickButton(request_access_button());
  WaitForAnimation();

  // Site interaction should stay the same because dialog wasn't accepted.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);

  // Click the request access button again, and this time accept the dialog and
  // wait for the page refresh.
  content::TestNavigationObserver observer(web_contents());
  extensions::ExtensionActionRunner::GetForWebContents(web_contents())
      ->accept_bubble_for_testing(true);
  ClickButton(request_access_button());
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  WaitForAnimation();

  // Extension A and B should have active site interaction, since their actions
  // ran, but keep the same site access since this is a one-time access grant.
  // The request access button should be hidden.
  EXPECT_FALSE(request_access_button()->GetVisible());
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionA, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionB, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionC, url),
            SiteAccess::kOnAllSites);

  // Re-navigate to the same url. Refreshing the page doesn't remove the action,
  // thus we need to navigate to another page and then navigate back to the
  // original page.
  NavigateToUrl(embedded_test_server()->GetURL("other.com", "/title1.html"));
  NavigateToUrl(url);

  // Extension A and B should have pending access again and the request access
  // button should be visible.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionsNamesForTesting(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
}

// Tests that clicking the request access button grants one time access to the
// extensions listed without needing a page refresh.
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
  extensions::ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  extensions::ScriptingPermissionsModifier(profile(), extensionB)
      .SetWithholdHostPermissions(true);

  // Navigate to a site where extensions A and B have withheld access.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateToUrl(url);

  // Verify request access button is visible because extensions A and B have
  // pending site interaction.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionsNamesForTesting(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));
  extensions::SitePermissionsHelper permissions(browser()->profile());
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionA, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionB, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionC, url),
            SiteAccess::kOnAllSites);

  // Click the request access button to grant one-time access. Since no
  // extensions need page refresh to run their actions, it immediately grants
  // access.
  ClickButton(request_access_button());
  WaitForAnimation();

  // Extension A and B should have active site interaction, since their action
  // run, but keep the same site access since this is a one-time access grant.
  // The request access button should be hidden.
  EXPECT_FALSE(request_access_button()->GetVisible());
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionA, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionB, url), SiteAccess::kOnClick);
  EXPECT_EQ(permissions.GetSiteAccess(*extensionC, url),
            SiteAccess::kOnAllSites);

  // Re-navigate to the same url. Refreshing the page doesn't remove the action,
  // thus we need to navigate to another page and then navigate back to the
  // original page.
  NavigateToUrl(embedded_test_server()->GetURL("other.com", "/title1.html"));
  NavigateToUrl(url);

  // Extension A and B should have pending access again and the request access
  // button should be visible.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionsNamesForTesting(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionA, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionB, web_contents()),
            SiteInteraction::kWithheld);
  EXPECT_EQ(permissions.GetSiteInteraction(*extensionC, web_contents()),
            SiteInteraction::kGranted);
}
