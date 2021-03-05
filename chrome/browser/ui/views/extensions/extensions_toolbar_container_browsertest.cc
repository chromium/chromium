// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

class ExtensionsToolbarContainerBrowserTest
    : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsToolbarContainerBrowserTest() = default;
  ExtensionsToolbarContainerBrowserTest(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ExtensionsToolbarContainerBrowserTest& operator=(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ~ExtensionsToolbarContainerBrowserTest() override = default;

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
};

// TODO(devlin): There are probably some tests from
// ExtensionsMenuViewBrowserTest that should move here (if they test the
// toolbar container more than the menu).

// Tests that invocation metrics are properly recorded when triggering
// extensions from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvocationMetrics) {
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

// Tests that clicking on a second extension action will close a first if its
// popup was open.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
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
    ExtensionTestMessageListener listener("alpha popup opened",
                                          /*will_reply=*/false);
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
    ExtensionTestMessageListener listener("beta popup opened",
                                          /*will_reply=*/false);
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
