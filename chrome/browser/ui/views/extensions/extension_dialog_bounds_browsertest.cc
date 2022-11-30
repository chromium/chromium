// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

using ::ash::MagnificationManager;

class ExtensionDialogBoundsTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ExtensionDialogBoundsTest() = default;

  ExtensionDialogBoundsTest(const ExtensionDialogBoundsTest&) = delete;
  ExtensionDialogBoundsTest& operator=(const ExtensionDialogBoundsTest&) =
      delete;

  ~ExtensionDialogBoundsTest() override = default;

  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extensions::ExtensionBrowserTest::SetUp();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (name == "OpenFileDialog")
      ShowOpenFileDialog();
    else if (name == "BigExtensionDialog")
      ShowBigExtensionDialog();
  }

  void EnableDockedMagnifier() const {
    extensions::TestExtensionRegistryObserver registry_observer(
        extensions::ExtensionRegistry::Get(
            ash::AccessibilityManager::Get()->profile()));
    MagnificationManager::Get()->SetDockedMagnifierEnabled(true);
    registry_observer.WaitForExtensionLoaded();
    ASSERT_TRUE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  }

 private:
  void ShowOpenFileDialog() { browser()->OpenFile(); }

  void ShowBigExtensionDialog() {
    ExtensionTestMessageListener init_listener("ready");

    scoped_refptr<const extensions::Extension> extension =
        LoadExtension(test_data_dir_.AppendASCII("uitest/tab_traversal"));
    ASSERT_TRUE(extension.get());

    // Dimensions of a dialog that would be bigger than the remaining display
    // work area when the docked magnifier is enabled.
    ExtensionDialog::InitParams params(gfx::Size(1000, 1000));
    params.is_modal = true;
    params.min_size = {640, 240};
    auto* dialog = ExtensionDialog::Show(
        extension->url().Resolve("main.html"),
        browser()->window()->GetNativeWindow(), browser()->profile(),
        nullptr /* web_contents */, nullptr /* observer */, params);
    ASSERT_TRUE(dialog);
    ASSERT_TRUE(init_listener.WaitUntilSatisfied());
  }
};

// Note that the underscores in the test names below are important as whatever
// comes after the underscore is used as the parameter for the ShowUi() above.

IN_PROC_BROWSER_TEST_F(ExtensionDialogBoundsTest, Test_OpenFileDialog) {
  EnableDockedMagnifier();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionDialogBoundsTest, Test_BigExtensionDialog) {
  EnableDockedMagnifier();
  ShowAndVerifyUi();
}

}  // namespace
