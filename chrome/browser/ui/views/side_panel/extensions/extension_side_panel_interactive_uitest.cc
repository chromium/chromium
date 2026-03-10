// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class ExtensionSidePanelInteractiveTest : public ExtensionBrowserTest {
 protected:
  SidePanelEntry::Key GetKey(const ExtensionId& id) {
    return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, id);
  }

  SidePanelUI* GetSidePanelUI() {
    return browser()->GetFeatures().side_panel_ui();
  }

  ExtensionSidePanelCoordinator* GetCoordinator(
      const ExtensionId& extension_id,
      content::WebContents* web_contents) {
    auto* manager =
        web_contents ? tabs::TabInterface::GetFromContents(web_contents)
                           ->GetTabFeatures()
                           ->extension_side_panel_manager()
                     : browser()->GetFeatures().extension_side_panel_manager();
    return manager->GetExtensionCoordinatorForTesting(extension_id);
  }
};

// Test that the extension's side panel web contents is focused when shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelInteractiveTest,
                       SidePanelWebContentsFocusedOnShow) {
  TestExtensionDir dir;
  dir.WriteManifest(R"({
    "name": "Autofocus Test",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["sidePanel"],
    "side_panel": {
      "default_path": "panel.html"
    }
  })");
  dir.WriteFile(FILE_PATH_LITERAL("panel.html"), R"(
    <html>
      <body>
        <input id="autofocus-input" autofocus>
        <script src="panel.js"></script>
      </body>
    </html>
  )");
  dir.WriteFile(FILE_PATH_LITERAL("panel.js"), R"(
    const input = document.getElementById("autofocus-input");
    if (document.activeElement === input) {
      chrome.test.sendMessage("ready");
    } else {
      input.addEventListener("focus", () => chrome.test.sendMessage("ready"), { once: true });
    }
  )");

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  SidePanelUI* const side_panel_ui = GetSidePanelUI();

  side_panel_ui->Show(extension_key, std::nullopt,
                      /*suppress_animations=*/true);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionSidePanelCoordinator* coordinator =
      GetCoordinator(extension->id(), /*web_contents=*/nullptr);
  ASSERT_TRUE(coordinator);
  content::WebContents* side_panel_contents =
      coordinator->GetHostWebContentsForTesting();
  ASSERT_TRUE(side_panel_contents);

  // Verifying that the document.activeElement within the side panel's
  // WebContents is indeed the input element.
  EXPECT_EQ("autofocus-input",
            content::EvalJs(side_panel_contents, "document.activeElement.id"));
}

}  // namespace extensions
