// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chromeos/chromeos_switches.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

class ExtensionDialogBoundsTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ExtensionDialogBoundsTest() = default;
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
    chromeos::MagnificationManager::Get()->SetDockedMagnifierEnabled(true);
    ASSERT_TRUE(
        chromeos::MagnificationManager::Get()->IsDockedMagnifierEnabled());
  }

 private:
  void ShowOpenFileDialog() { browser()->OpenFile(); }

  void ShowBigExtensionDialog() {
    ExtensionTestMessageListener init_listener("ready", false /* will_reply */);

    scoped_refptr<const extensions::Extension> extension =
        LoadExtension(test_data_dir_.AppendASCII("uitest/tab_traversal"));
    ASSERT_TRUE(extension.get());

    // Dimensions of a dialog that would be bigger than the remaining display
    // work area when the docked magnifier is enabled.
    constexpr int kDialogWidth = 1000;
    constexpr int kDialogHeight = 1000;
    constexpr int kDialogMinimumWidth = 640;
    constexpr int kDialogMinimumHeight = 240;
    auto* dialog = ExtensionDialog::Show(
        extension->url().Resolve("main.html"),
        browser()->window()->GetNativeWindow(), browser()->profile(),
        nullptr /* web_contents */, kDialogWidth, kDialogHeight,
        kDialogMinimumWidth, kDialogMinimumHeight, base::string16() /* title */,
        nullptr /* observer */);
    ASSERT_TRUE(dialog);
    ASSERT_TRUE(init_listener.WaitUntilSatisfied());
  }

  DISALLOW_COPY_AND_ASSIGN(ExtensionDialogBoundsTest);
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
