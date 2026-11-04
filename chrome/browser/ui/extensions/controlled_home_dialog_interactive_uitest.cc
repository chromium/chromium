// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/controlled_home_dialog.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller_interface.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/controls/styled_label.h"

namespace {

class TestDialogController : public ControlledHomeDialogControllerInterface {
 public:
  TestDialogController(Profile* profile,
                       std::string action_id,
                       std::optional<CloseAction>* close_action_out = nullptr)
      : profile_(profile),
        action_id_(std::move(action_id)),
        close_action_out_(close_action_out) {}
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
  void OnBubbleClosed(CloseAction action) override {
    EXPECT_FALSE(close_action_.has_value());
    close_action_ = action;
    if (close_action_out_) {
      *close_action_out_ = action;
    }
    if (action == CLOSE_EXECUTE) {
      extensions::ExtensionRegistrar::Get(profile_)->DisableExtension(
          action_id_, {extensions::disable_reason::DISABLE_USER_ACTION});
    }
  }
  bool IsPolicyIndicationNeeded() const override { return false; }

 private:
  raw_ptr<Profile> profile_;
  std::string action_id_;
  std::optional<CloseAction> close_action_;
  raw_ptr<std::optional<CloseAction>> close_action_out_;
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

  auto ShowControlledHomeDialog(
      const std::string& extension_id,
      std::optional<ControlledHomeDialogControllerInterface::CloseAction>*
          close_action_out = nullptr) {
    return Do([this, extension_id, close_action_out]() {
      extensions::ShowControlledHomeDialog(
          browser()->GetProfile(), browser()->GetWindow()->GetNativeWindow(),
          std::make_unique<TestDialogController>(
              browser()->GetProfile(), extension_id, close_action_out));
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
    return extensions::ExtensionRegistrar::Get(browser()->GetProfile());
  }
};

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogUITest,
                       DialogClosedWhenExtensionIsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome("Extension");
  std::optional<ControlledHomeDialogControllerInterface::CloseAction>
      close_action;

  RunTestSequence(
      ShowControlledHomeDialog(extension->id(), &close_action),
      WaitForShow(extensions::kControlledHomeDialogCancelButtonElementId),
      DisableExtension(extension->id()),
      WaitForHide(extensions::kControlledHomeDialogCancelButtonElementId));

  EXPECT_EQ(ControlledHomeDialogControllerInterface::CLOSE_DISMISS_DEACTIVATION,
            close_action);
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogUITest,
                       ClickActionButtonDisablesExtensionAndClosesDialog) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome("Extension");
  std::optional<ControlledHomeDialogControllerInterface::CloseAction>
      close_action;

  RunTestSequence(
      ShowControlledHomeDialog(extension->id(), &close_action),
      WaitForShow(extensions::kControlledHomeDialogActionButtonElementId),
      PressButton(extensions::kControlledHomeDialogActionButtonElementId),
      WaitForHide(extensions::kControlledHomeDialogActionButtonElementId));

  EXPECT_EQ(ControlledHomeDialogControllerInterface::CLOSE_EXECUTE,
            close_action);
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogUITest,
                       ClickCancelButtonClosesDialog) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome("Extension");
  std::optional<ControlledHomeDialogControllerInterface::CloseAction>
      close_action;

  RunTestSequence(
      ShowControlledHomeDialog(extension->id(), &close_action),
      WaitForShow(extensions::kControlledHomeDialogCancelButtonElementId),
      PressButton(extensions::kControlledHomeDialogCancelButtonElementId),
      WaitForHide(extensions::kControlledHomeDialogCancelButtonElementId));

  EXPECT_EQ(ControlledHomeDialogControllerInterface::CLOSE_DISMISS_USER_ACTION,
            close_action);
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogUITest,
                       ClickLearnMoreLinkClosesDialog) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome("Extension");
  std::optional<ControlledHomeDialogControllerInterface::CloseAction>
      close_action;

  RunTestSequence(
      ShowControlledHomeDialog(extension->id(), &close_action),
      WaitForShow(extensions::kControlledHomeDialogLearnMoreLinkElementId),
      WithView(extensions::kControlledHomeDialogLearnMoreLinkElementId,
               [](views::StyledLabel* learn_more_label) {
                 learn_more_label->ClickFirstLinkForTesting();
               }),
      WaitForHide(extensions::kControlledHomeDialogLearnMoreLinkElementId));

  EXPECT_EQ(ControlledHomeDialogControllerInterface::CLOSE_LEARN_MORE,
            close_action);
}
