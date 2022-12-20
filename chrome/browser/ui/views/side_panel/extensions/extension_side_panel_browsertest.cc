// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {
namespace {

// A class which waits for a SidePanelEntry's view to be shown in the side
// panel.
class ExtensionSidePanelWaiter : public SidePanelEntryObserver {
 public:
  explicit ExtensionSidePanelWaiter(SidePanelEntry* entry) {
    side_panel_entry_observation_.Observe(entry);
  }

  ~ExtensionSidePanelWaiter() override = default;
  ExtensionSidePanelWaiter(const ExtensionSidePanelWaiter& other) = delete;
  ExtensionSidePanelWaiter& operator=(const ExtensionSidePanelWaiter& other) =
      delete;

  // Waits until the view for the extension_id is created.
  void Wait() { run_loop_.Run(); }

 private:
  void OnEntryShown(SidePanelEntry* entry) override {
    run_loop_.QuitWhenIdle();
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      side_panel_entry_observation_{this};
};

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

  ExtensionSidePanelWaiter waiter(extension_entry);
  side_panel_coordinator()->Show(extension_key);
  waiter.Wait();

  // Wait until the view is shown, and check that the view is active by
  // listening for the message sent from the view's script.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_TRUE(view_shown_listener.WaitUntilSatisfied());

  // Reset the `view_shown_listener` and create a second waiter.
  ExtensionSidePanelWaiter waiter_2(extension_entry);
  view_shown_listener.Reset();

  // Close and reopen the side panel. The extension's view should be recreated.
  side_panel_coordinator()->Close();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Show(extension_key);

  waiter_2.Wait();
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_TRUE(view_shown_listener.WaitUntilSatisfied());

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
