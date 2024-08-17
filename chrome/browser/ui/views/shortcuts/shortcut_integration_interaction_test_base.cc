// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/shortcut_integration_interaction_test_base.h"

#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut_delegate.h"
#include "chrome/browser/ui/views/shortcuts/shortcut_integration_interaction_test_internal.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/textfield/textfield.h"
#include "url/gurl.h"

namespace shortcuts {

ShortcutIntegrationInteractionTestApi::ShortcutIntegrationInteractionTestApi()
    : InteractiveBrowserTestApi(
          std::make_unique<ShortcutIntegrationInteractionTestPrivate>()) {
  platform_util::internal::DisableShellOperationsForTesting();
}

ShortcutIntegrationInteractionTestApi
    ::~ShortcutIntegrationInteractionTestApi() = default;

ui::test::InteractiveTestApi::MultiStep
ShortcutIntegrationInteractionTestApi::ShowCreateShortcutDialog() {
  return Steps(
      PressButton(kToolbarAppMenuButtonElementId),
      // Sometimes the "Save and Share" item isn't immediately present, so
      // explicitly wait for it to show.
      WaitForShow(AppMenuModel::kSaveAndShareMenuItem),
      SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
      // Sometimes the "Create Shortcut" item isn't immediately present, so
      // explicitly wait for it.
      WaitForShow(AppMenuModel::kCreateShortcutItem),
      CheckViewProperty(AppMenuModel::kCreateShortcutItem,
                        &views::View::GetEnabled, true),
      SelectMenuItem(AppMenuModel::kCreateShortcutItem),
      WaitForShow(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogTitleFieldId),
      WaitForShow(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId));
}

ui::test::InteractiveTestApi::MultiStep
ShortcutIntegrationInteractionTestApi::ShowAndAcceptCreateShortcutDialog() {
  return Steps(
      ShowCreateShortcutDialog(),
      PressButton(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId),
      // Wait for the dialog to go away, to make sure showing the dialog again
      // can correctly detect presence of the Ok button.
      WaitForHide(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId));
}

ui::test::InteractiveTestApi::MultiStep
ShortcutIntegrationInteractionTestApi
    ::ShowCreateShortcutDialogSetTitleAndAccept(const std::u16string& title) {
  return Steps(
      ShowCreateShortcutDialog(),
      EnterText(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogTitleFieldId,
          title),
      PressButton(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId),
      // Wait for the dialog to go away, to make sure showing the dialog again
      // can correctly detect presence of the Ok button.
      WaitForHide(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId));
}

ui::test::InteractiveTestApi::StepBuilder
ShortcutIntegrationInteractionTestApi::InstrumentNextShortcut(
    ui::ElementIdentifier identifier) {
  return Do([this, identifier] {
    test_impl().SetNextShortcutIdentifier(identifier);
  });
}

ui::test::InteractiveTestApi::StepBuilder
ShortcutIntegrationInteractionTestApi::LaunchShortcut(
    ui::ElementIdentifier identifier) {
  return InAnyContext(WithElement(identifier, [](ui::TrackedElement* element) {
    ShortcutCreationTestSupport::LaunchShortcut(GetShortcutPath(element));
  }));
}

// static
base::FilePath ShortcutIntegrationInteractionTestApi::GetShortcutPath(
    ui::TrackedElement* element) {
  return ShortcutIntegrationInteractionTestPrivate::GetShortcutPath(element);
}

ShortcutIntegrationInteractionTestPrivate&
ShortcutIntegrationInteractionTestApi::test_impl() {
  return static_cast<ShortcutIntegrationInteractionTestPrivate&>(
      private_test_impl());
}

}  // namespace shortcuts
