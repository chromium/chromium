// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace {

using ::testing::ElementsAre;

enum ChooserType {
  kWebUsb,
  kWebHid,
};

class DeviceChooserExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<ChooserType> {
 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    extensions::TestExtensionDir dir;
    dir.WriteManifest(R"(
          {
            "name": "test-extension",
            "version": "1.0",
            "manifest_version": 3
          }
        )");
    // Showing a device chooser requires a Window context. Create an empty
    // extension page so we can open the chooser from that page.
    dir.WriteFile(FILE_PATH_LITERAL("page.html"), {});
    extension_ = LoadExtension(dir.UnpackedPath());
    ASSERT_TRUE(extension_);
    ASSERT_TRUE(extensions_container()->GetVisible());
    ASSERT_TRUE(extensions_container()->GetViewForId(extension_->id()));

    // Navigate to the extension page.
    auto url = extension_->GetResourceURL("page.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_EQ(url, web_contents()->GetLastCommittedURL());
  }

  void TearDownOnMainThread() override {
    extension_ = nullptr;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  const std::string& extension_id() { return extension_->id(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  bool ShowChooser() {
    auto show_chooser_script = [](ChooserType type) -> std::string {
      switch (type) {
        case kWebUsb:
          return "navigator.usb.requestDevice({filters:[]});";
        case kWebHid:
          return "navigator.hid.requestDevice({filters:[]});";
      }
    };
    return content::ExecJs(web_contents(), show_chooser_script(GetParam()),
                           content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);
  }

  std::vector<ToolbarActionView*> GetPinnedExtensionViews() {
    auto is_visible = [&](ToolbarActionView* const action) -> bool {
#if BUILDFLAG(IS_MAC)
      // TODO(crbug.com/40670141): Use IsActionVisibleOnToolbar() because it
      // queries the underlying model and not GetVisible(), as that relies on an
      // animation running, which is not reliable in unit tests on Mac.
      return extensions_container()->IsActionVisibleOnToolbar(
          action->view_controller()->GetId());
#else
      return action->GetVisible();
#endif
    };

    std::vector<ToolbarActionView*> result;
    for (views::View* child : extensions_container()->children()) {
      // Ensure we don't downcast the ExtensionsToolbarButton.
      if (views::IsViewClass<ToolbarActionView>(child)) {
        auto* action = static_cast<ToolbarActionView*>(child);
        if (is_visible(action)) {
          result.push_back(action);
        }
      }
    }
    return result;
  }

  std::vector<std::string> GetPinnedExtensionNames() {
    return base::ToVector(GetPinnedExtensionViews(), [](auto* view) {
      return base::UTF16ToUTF8(view->view_controller()->GetActionName());
    });
  }

  void WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40670141): we avoid using animations on Mac due to the
    // lack of support in unit tests. Therefore this is a no-op.
#else
    views::test::WaitForAnimatingLayoutManager(extensions_container());
#endif
  }

 private:
  raw_ptr<const extensions::Extension> extension_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(DeviceChooserExtensionBrowserTest,
                       ChooserAnchoredToExtensionIcon) {
  // Check that no widget is anchored to the extension icon.
  EXPECT_FALSE(extensions_container()->GetAnchoredWidgetForExtensionForTesting(
      extension_id()));

  // Check that no extensions are pinned in the toolbar.
  EXPECT_TRUE(GetPinnedExtensionNames().empty());

  // Open the chooser dialog and leave it open without making a selection.
  auto chooser_bubble_ui_waiter = test::ChooserBubbleUiWaiter::Create();
  EXPECT_TRUE(ShowChooser());

  // Wait for the chooser widget to be created.
  chooser_bubble_ui_waiter->WaitForChange();
  EXPECT_TRUE(chooser_bubble_ui_waiter->has_shown());

  // Ensure the widget is visible and anchored to the extension icon.
  auto* chooser_widget =
      extensions_container()->GetAnchoredWidgetForExtensionForTesting(
          extension_id());
  ASSERT_TRUE(chooser_widget);
  views::test::WidgetVisibleWaiter(chooser_widget).Wait();

  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(chooser_widget);

  // Showing the chooser dialog temporarily pins the extension in the toolbar.
  EXPECT_THAT(GetPinnedExtensionNames(), ElementsAre("test-extension"));

  // Navigate away from the extension page. This dismisses the chooser.
  auto url = GURL("https://chromium.org");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents()->GetLastCommittedURL());

  // Wait for the widget to be destroyed and for the extension icon to be
  // unpinned.
  widget_destroyed_waiter.Wait();
  WaitForAnimation();

  // Check that no widget is anchored to the extension icon.
  EXPECT_FALSE(extensions_container()->GetAnchoredWidgetForExtensionForTesting(
      extension_id()));

  // Check that no extensions are pinned in the toolbar.
  EXPECT_TRUE(GetPinnedExtensionNames().empty());
}

INSTANTIATE_TEST_SUITE_P(DeviceChooserExtensionBrowserTests,
                         DeviceChooserExtensionBrowserTest,
                         testing::Values(ChooserType::kWebUsb,
                                         ChooserType::kWebHid),
                         [](testing::TestParamInfo<ChooserType> info) {
                           switch (info.param) {
                             case ChooserType::kWebUsb:
                               return "WebUsb";
                             case ChooserType::kWebHid:
                               return "WebHid";
                           }
                         });

}  // namespace
