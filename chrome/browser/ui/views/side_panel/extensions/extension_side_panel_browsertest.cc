// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {
namespace {

SidePanelEntry::Key GetKey(const ExtensionId& id) {
  return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, id);
}

// A class which waits on various SidePanelEntryObserver events.
class TestSidePanelEntryWaiter : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelEntryWaiter(SidePanelEntry* entry) {
    side_panel_entry_observation_.Observe(entry);
  }

  ~TestSidePanelEntryWaiter() override = default;
  TestSidePanelEntryWaiter(const TestSidePanelEntryWaiter& other) = delete;
  TestSidePanelEntryWaiter& operator=(const TestSidePanelEntryWaiter& other) =
      delete;

  void WaitForEntryShown() { entry_shown_run_loop_.Run(); }

  void WaitForIconUpdated() { icon_updated_run_loop_.Run(); }

 private:
  void OnEntryShown(SidePanelEntry* entry) override {
    entry_shown_run_loop_.QuitWhenIdle();
  }

  void OnEntryIconUpdated(SidePanelEntry* entry) override {
    icon_updated_run_loop_.QuitWhenIdle();
  }

  base::RunLoop entry_shown_run_loop_;
  base::RunLoop icon_updated_run_loop_;
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      side_panel_entry_observation_{this};
};

// A class which waits for an extension's SidePanelEntry to be registered and/or
// deregistered.
class ExtensionSidePanelRegistryWaiter : public SidePanelRegistryObserver {
 public:
  explicit ExtensionSidePanelRegistryWaiter(SidePanelRegistry* registry,
                                            const ExtensionId& extension_id)
      : extension_id_(extension_id) {
    side_panel_registry_observation_.Observe(registry);
  }

  ~ExtensionSidePanelRegistryWaiter() override = default;
  ExtensionSidePanelRegistryWaiter(
      const ExtensionSidePanelRegistryWaiter& other) = delete;
  ExtensionSidePanelRegistryWaiter& operator=(
      const ExtensionSidePanelRegistryWaiter& other) = delete;

  // Waits until the entry for `extension_id_` is registered.
  void WaitForRegistration() { registration_run_loop_.Run(); }

  // Waits until the entry for `extension_id_` is deregistered.
  void WaitForDeregistration() { deregistration_run_loop_.Run(); }

 private:
  // SidePanelRegistryObserver implementation.
  void OnEntryRegistered(SidePanelRegistry* registry,
                         SidePanelEntry* entry) override {
    if (entry->key() == GetKey(extension_id_)) {
      registration_run_loop_.QuitWhenIdle();
    }
  }

  void OnEntryWillDeregister(SidePanelRegistry* registry,
                             SidePanelEntry* entry) override {
    if (entry->key() == GetKey(extension_id_)) {
      deregistration_run_loop_.QuitWhenIdle();
    }
  }

  ExtensionId extension_id_;
  base::RunLoop registration_run_loop_;
  base::RunLoop deregistration_run_loop_;
  base::ScopedObservation<SidePanelRegistry, SidePanelRegistryObserver>
      side_panel_registry_observation_{this};
};

class ExtensionSidePanelBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 protected:
  // Calls chrome.sidePanel.setOptions() for the given `extension`, `path` and
  // `enabled` and returns when the API call is complete.
  void RunSetOptions(const Extension& extension,
                     const std::string& path,
                     bool enabled) {
    auto function = base::MakeRefCounted<SidePanelSetOptionsFunction>();
    function->set_extension(&extension);

    std::string args =
        base::StringPrintf(R"([{"path":"%s","enabled":%s}])", path.c_str(),
                           enabled ? "true" : "false");
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
        << function->GetError();
  }

  SidePanelRegistry* global_registry() {
    return SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
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
  ExtensionTestMessageListener default_path_listener("default_path");

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
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Reset the `default_path_listener`.
  default_path_listener.Reset();

  // Close and reopen the side panel. The extension's view should be recreated.
  side_panel_coordinator()->Close();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Show(extension_key);

  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Now unload the extension. The key should no longer exist in the global
  // registry and the side panel should close as a result.
  UnloadExtension(extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that an extension's SidePanelEntry is registered for new browser
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
      SidePanelCoordinator::GetGlobalSidePanelRegistry(second_browser);
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

// Test that the extension's side panel entry shows the extension's icon.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, EntryShowsExtensionIcon) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  SidePanelEntry* extension_entry =
      global_registry()->GetEntryForKey(extension_key);

  // At this point, we don't know if the extension's icon has finished loading
  // or not, since the first icon load is initiated right when the extension
  // loads. Attempting to wait on OnEntryIconUpdated will hang forever if the
  // icon has been loaded after setting up the waiter. To ensure the icon is
  // loaded and the OnEntryIconUpdated event is broadcast, initiate a reload for
  // the extension's icon manually.
  {
    TestSidePanelEntryWaiter icon_updated_waiter(extension_entry);
    extension_coordinator->LoadExtensionIconForTesting();
    icon_updated_waiter.WaitForIconUpdated();
  }

  // Check that the entry's icon bitmap is identical to the bitmap of the
  // extension's icon scaled down to `extension_misc::EXTENSION_ICON_BITTY`.
  SkBitmap expected_icon_bitmap = TestImageLoader::LoadAndGetExtensionBitmap(
      extension.get(), "icon.png", extension_misc::EXTENSION_ICON_BITTY);
  const SkBitmap& actual_icon_bitmap =
      *extension_entry->icon().GetImage().ToSkBitmap();
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(expected_icon_bitmap, actual_icon_bitmap));
}

// Test that sidePanel.setOptions() will register and deregister the extension's
// SidePanelEntry when called with enabled: true/false.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SetOptions_Enabled) {
  ExtensionTestMessageListener panel_2_listener("panel_2");

  // Load an extension without a default side panel path.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/setoptions_default_tab"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));

  {
    // Call setOptions({enabled: true}) and wait for the extension's
    // SidePanelEntry to be registered.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, "panel_1.html", /*enabled=*/true);
    waiter.WaitForRegistration();
  }

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    // Call setOptions({enabled: false}) and wait for the extension's
    // SidePanelEntry to be deregistered.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, "panel_1.html", /*enabled=*/false);
    waiter.WaitForDeregistration();
  }

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));

  {
    // Sanity check that re-enabling the side panel will register the entry
    // again and a view with the new side panel path can be shown.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, "panel_2.html", /*enabled=*/true);
    waiter.WaitForRegistration();
  }

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
  side_panel_coordinator()->Show(extension_key);

  // Wait until the view in the side panel is active by listening for the
  // message sent from the view's script.
  ASSERT_TRUE(panel_2_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  {
    // Calling setOptions({enabled: false}) when the extension's SidePanelEntry
    // is shown should close the side panel.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, "panel_2.html", /*enabled=*/false);
    waiter.WaitForDeregistration();
  }

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that sidePanel.setOptions() will change what is shown in the extension's
// SidePanelEntry's view when called with different paths.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SetOptions_Path) {
  ExtensionTestMessageListener default_path_listener("default_path");
  ExtensionTestMessageListener panel_1_listener("panel_1");

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Check that the extension's side panel view shows the most recently set
  // path.
  RunSetOptions(*extension, "panel_1.html", /*enabled=*/true);
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
  EXPECT_FALSE(default_path_listener.was_satisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Check that changing the path while the view is active will cause the view
  // to navigate to the new path.
  RunSetOptions(*extension, "default_path.html", /*enabled=*/true);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Switch to the reading list in the side panel and check that the extension
  // view is cached (i.e. the view exists but is not shown, and its web contents
  // still exists).
  {
    TestSidePanelEntryWaiter reading_list_waiter(
        global_registry()->GetEntryForKey(
            SidePanelEntry::Key(SidePanelEntry::Id::kReadingList)));
    side_panel_coordinator()->Show(SidePanelEntry::Id::kReadingList);
    reading_list_waiter.WaitForEntryShown();
  }

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key)->CachedView());

  panel_1_listener.Reset();
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());

  // Test calling setOptions with a different path when the extension's view is
  // cached. The cached view should then be invalidated and its web contents are
  // destroyed.
  RunSetOptions(*extension, "panel_1.html", /*enabled=*/true);
  destroyed_watcher.Wait();

  // When the extension's entry is shown again, the view with the updated path
  // should be active.
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
}

// Test that calling window.close() from an extension side panel deletes the
// panel's web contents and closes the extension's side panel if it's also
// shown.
// TODO(crbug.com/1423302): Add a test case for contextual extension panels.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, WindowCloseCalled) {
  // Install an extension and show its side panel.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  // Call window.close() from the extension's side panel page and wait for the
  // web contents to be destroyed.
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        extension_coordinator->GetHostWebContentsForTesting());
    ASSERT_TRUE(content::ExecuteScript(
        extension_coordinator->GetHostWebContentsForTesting(),
        "window.close();"));
    destroyed_watcher.Wait();
  }

  // The side panel should now be closed.
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Show the extension's side panel again.
  ExtensionTestMessageListener default_path_listener("default_path");
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Show another side panel type so the extension's panel's view gets cached.
  TestSidePanelEntryWaiter reading_list_waiter(
      global_registry()->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadingList)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kReadingList);
  reading_list_waiter.WaitForEntryShown();
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key)->CachedView());

  // Calling window.close() from within the panel should invalidate the cached
  // view when the extension panel is not shown.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());
  ASSERT_TRUE(content::ExecuteScript(
      extension_coordinator->GetHostWebContentsForTesting(),
      "window.close();"));
  destroyed_watcher.Wait();

  // The side panel should be open because the reading list entry is still
  // shown.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that calling window.close() from an extension side panel when it is
// shown closes the side panel even if another entry is loading and will be
// shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       WindowCloseCalledWhenLoading) {
  // Install an extension and show its side panel.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  // Start showing another entry and call window.close() from the extension's
  // side panel page while the other entry is still loading but not shown. The
  // extension's side panel web content should still be destroyed and the side
  // panel will close.
  {
    side_panel_coordinator()->Show(SidePanelEntry::Id::kReadingList);

    content::WebContentsDestroyedWatcher destroyed_watcher(
        extension_coordinator->GetHostWebContentsForTesting());
    ASSERT_TRUE(content::ExecuteScript(
        extension_coordinator->GetHostWebContentsForTesting(),
        "window.close();"));
    destroyed_watcher.Wait();
  }

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

class ExtensionSidePanelDisabledBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 protected:
  SidePanelRegistry* global_registry() {
    return SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
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
