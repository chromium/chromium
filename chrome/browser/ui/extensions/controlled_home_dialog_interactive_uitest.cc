// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller_interface.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/test/test_extension_dir.h"

namespace {

class TestDialogController : public ControlledHomeDialogControllerInterface {
 public:
  explicit TestDialogController(std::string action_id)
      : action_id_(action_id) {}
  TestDialogController(const TestDialogController&) = delete;
  const TestDialogController& operator=(const TestDialogController&) = delete;
  ~TestDialogController() override = default;

  bool ShouldShow() override { return true; }
  std::u16string GetHeadingText() override { return u"heading"; }
  std::u16string GetBodyText() override { return u"body"; }
  std::u16string GetActionButtonText() override { return u"action"; }
  std::u16string GetDismissButtonText() override { return u"dismiss"; }
  std::string GetAnchorActionId() override { return action_id_; }
  void OnBubbleShown() override {}
  void OnBubbleClosed(CloseAction action) override {}
  bool IsPolicyIndicationNeeded() const override { return false; }

 private:
  std::string action_id_;
};

}  // namespace

class ControlledHomeDialogUITest
    : public InteractiveBrowserTestMixin<extensions::ExtensionBrowserTest> {
 public:
  ControlledHomeDialogUITest() = default;
  ControlledHomeDialogUITest(const ControlledHomeDialogUITest&) = delete;
  ControlledHomeDialogUITest& operator=(const ControlledHomeDialogUITest&) =
      delete;
  ~ControlledHomeDialogUITest() override = default;

  scoped_refptr<const extensions::Extension> LoadExtensionOverridingHome(
      const std::string& name) {
    scoped_refptr<const extensions::Extension> extension =
        LoadExtension(test_data_dir_.AppendASCII("api_test/override/newtab"));
    return extension;
  }

  auto ShowControlledHomeDialog(const std::string& extension_id) {
    return Do([&]() {
      extensions::ShowControlledHomeDialog(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          std::make_unique<TestDialogController>(extension_id));
    });
  }

  auto DisableExtension(const extensions::ExtensionId& extension_id) {
    return Do([&]() {
      extension_registrar()->DisableExtension(
          extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
    });
  }

 private:
  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogUITest,
                       DialogClosedWhenExtensionIsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome("Extension");

  RunTestSequence(
      ShowControlledHomeDialog(extension->id()),
      WaitForShow(extensions::kControlledHomeDialogCancelButtonElementId),
      DisableExtension(extension->id()),
      WaitForHide(extensions::kControlledHomeDialogCancelButtonElementId));
}
