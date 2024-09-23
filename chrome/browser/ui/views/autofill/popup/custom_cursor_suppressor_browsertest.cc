// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CustomCursorSuppressorBrowsertest
    : public extensions::ExtensionBrowserTest {
 protected:
  // Installs an extension and shows it in its side panel.
  scoped_refptr<const extensions::Extension> LoadExtensionInSidePanel() {
    scoped_refptr<const extensions::Extension> extension = LoadExtension(
        test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
    CHECK(extension);
    SidePanelEntry::Key extension_key =
        SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
    CHECK(global_registry()->GetEntryForKey(extension_key));

    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    CHECK(default_path_listener.WaitUntilSatisfied());
    CHECK(side_panel_coordinator()->IsSidePanelShowing());
    return extension;
  }

  SidePanelRegistry* global_registry() {
    return browser()
        ->browser_window_features()
        ->side_panel_coordinator()
        ->GetWindowRegistry();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }
};

// Tests that starting custom cursor suppression disables custom cursors in
// extension `WebContents` objects that were created before the suppressor is
// started.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowsertest,
                       SuppressionWorksForAlreadyLoadedExtensions) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionInSidePanel();
  auto* extension_coordinator =
      browser()
          ->GetFeatures()
          .extension_side_panel_manager()
          ->GetExtensionCoordinatorForTesting(extension->id());
  content::WebContents* host_contents =
      extension_coordinator->GetHostWebContentsForTesting();

  CustomCursorSuppressor suppressor;
  EXPECT_FALSE(suppressor.IsSuppressing(*host_contents));
  suppressor.Start();
  EXPECT_TRUE(suppressor.IsSuppressing(*host_contents));
}

// Tests that starting custom cursor suppression disables custom cursors in
// extensions `WebContents` objects that are created after the suppressor
// is started.
IN_PROC_BROWSER_TEST_F(
    CustomCursorSuppressorBrowsertest,
    SuppressionWorksForExtensionsLoadedAfterSuppressorStart) {
  CustomCursorSuppressor suppressor;
  suppressor.Start();

  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionInSidePanel();
  auto* extension_coordinator =
      browser()
          ->GetFeatures()
          .extension_side_panel_manager()
          ->GetExtensionCoordinatorForTesting(extension->id());
  content::WebContents* host_contents =
      extension_coordinator->GetHostWebContentsForTesting();
  EXPECT_TRUE(suppressor.IsSuppressing(*host_contents));
}

}  // namespace
