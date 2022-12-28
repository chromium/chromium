// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {
namespace {

class ExtensionSidePanelBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 protected:
  SidePanelRegistry* global_registry() {
    return side_panel_coordinator()->GetGlobalSidePanelRegistry();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->side_panel_coordinator();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that only extensions with side panel content will have a SidePanelEntry
// registered.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ExtensionEntryVisibleInSidePanel) {
  // Load two extensions: one with a side panel entry in its manifest and one
  // without.
  scoped_refptr<const extensions::Extension> no_side_panel_extension =
      LoadExtension(test_data_dir_.AppendASCII("common/background_script"));
  ASSERT_TRUE(no_side_panel_extension);

  scoped_refptr<const extensions::Extension> side_panel_extension =
      LoadExtension(
          test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(side_panel_extension);

  // Check that only the extension with the side panel entry in its manifest is
  // shown as an entry in the global side panel registry.
  EXPECT_TRUE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, side_panel_extension->id())));
  EXPECT_FALSE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, no_side_panel_extension->id())));

  // Unloading the extension should remove it from the registry.
  UnloadExtension(side_panel_extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, side_panel_extension->id())));
}

// Test that an extension's view is shown/behaves correctly in the side panel.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ExtensionViewVisibleInsideSidePanel) {
  ExtensionTestMessageListener view_shown_listener("side_panel_shown");

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  SidePanelEntry* extension_entry =
      global_registry()->GetEntryForKey(extension_key);
  ASSERT_TRUE(extension_entry);

  // The key for the extension should be registered, but the side panel isn't
  // shown yet.
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  side_panel_coordinator()->Show(extension_key);

  // Wait until the view in the side panel is active by listening for the
  // message sent from the view's script.
  ASSERT_TRUE(view_shown_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Reset the `view_shown_listener`.
  view_shown_listener.Reset();

  // Close and reopen the side panel. The extension's view should be recreated.
  side_panel_coordinator()->Close();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Show(extension_key);

  ASSERT_TRUE(view_shown_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Now unload the extension. The key should no longer exist in the global
  // registry and the side panel should close as a result.
  UnloadExtension(extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Tests that an extension's SidePanelEntry is registered for new browser
// windows.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, MultipleBrowsers) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Open a new browser window. The extension's SidePanelEntry should also be
  // registered for the new window's global SidePanelRegistry.
  Browser* second_browser = CreateBrowser(browser()->profile());
  SidePanelRegistry* second_global_registry =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->side_panel_coordinator()
          ->GetGlobalSidePanelRegistry();
  EXPECT_TRUE(second_global_registry->GetEntryForKey(extension_key));
}

// Test that if the side panel is closed while the extension's side panel view
// is still loading, there will not be a crash. Regression for
// crbug.com/1403168.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SidePanelQuicklyClosed) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Quickly open the side panel showing the extension's side panel entry then
  // close it. The test should not cause any crashes after it is complete.
  side_panel_coordinator()->Show(extension_key);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Close();
}

class ExtensionSidePanelDisabledBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 protected:
  SidePanelRegistry* global_registry() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->side_panel_coordinator()
        ->GetGlobalSidePanelRegistry();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that an extension's SidePanelEntry is not registered if the
// `kExtensionSidePanelIntegration` feature flag is not enabled.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelDisabledBrowserTest,
                       NoSidePanelEntry) {
  // Load an extension and verify that it does not have a registered
  // SidePanelEntry as the feature is disabled.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
}

}  // namespace
}  // namespace extensions
