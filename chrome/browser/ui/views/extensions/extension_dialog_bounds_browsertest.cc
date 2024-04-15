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
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"

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
  void ShowUi(const std::string& name) override { browser()->OpenFile(); }

  void EnableDockedMagnifier() const {
    extensions::TestExtensionRegistryObserver registry_observer(
        extensions::ExtensionRegistry::Get(
            ash::AccessibilityManager::Get()->profile()));
    MagnificationManager::Get()->SetDockedMagnifierEnabled(true);
    registry_observer.WaitForExtensionLoaded();
    ASSERT_TRUE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  }
};

// Note that the underscores in the test names below are important as whatever
// comes after the underscore is used as the parameter for the ShowUi() above.
// TODO(crbug.com/40283636): File dialog no longer uses deprecated extension
// dialog. Thus, move this test to SelectFileDialogExtensionBrowserTest and
// remove this file.
IN_PROC_BROWSER_TEST_F(ExtensionDialogBoundsTest, Test_OpenFileDialog) {
  EnableDockedMagnifier();
  ShowAndVerifyUi();
}

}  // namespace
