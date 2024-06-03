// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/shortcut_integration_browsertest_base.h"

#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut_delegate.h"
#include "chrome/browser/ui/views/shortcuts/shortcut_integration_browsertest_internal.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/textfield/textfield.h"
#include "url/gurl.h"

namespace shortcuts {

ShortcutIntegrationBrowserTestApi::ShortcutIntegrationBrowserTestApi()
    : InteractiveBrowserTestApi(
          std::make_unique<ShortcutIntegrationBrowserTestPrivate>()) {
  platform_util::internal::DisableShellOperationsForTesting();
}

ShortcutIntegrationBrowserTestApi::~ShortcutIntegrationBrowserTestApi() =
    default;

ui::test::InteractiveTestApi::MultiStep
ShortcutIntegrationBrowserTestApi::ShowCreateShortcutDialog() {
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
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId),
      // Need to flush events so we're not trying to close the dialog while
      // showing it is still on the stack.
      FlushEvents());
}

ui::test::InteractiveTestApi::MultiStep
ShortcutIntegrationBrowserTestApi::ShowAndAcceptCreateShortcutDialog() {
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
ShortcutIntegrationBrowserTestApi::ShowCreateShortcutDialogSetTitleAndAccept(
    const std::u16string& title) {
  constexpr char kTitleTextFieldName[] = "title_text_field";
  return Steps(
      ShowCreateShortcutDialog(),
      NameChildViewByType<views::Textfield>(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogTitleFieldId,
          kTitleTextFieldName),
      EnterText(kTitleTextFieldName, title),
      PressButton(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId),
      // Wait for the dialog to go away, to make sure showing the dialog again
      // can correctly detect presence of the Ok button.
      WaitForHide(
          CreateDesktopShortcutDelegate::kCreateShortcutDialogOkButtonId));
}

ui::test::InteractiveTestApi::StepBuilder
ShortcutIntegrationBrowserTestApi::InstrumentNextShortcut(
    ui::ElementIdentifier identifier) {
  return Do([this, identifier] {
    test_impl().SetNextShortcutIdentifier(identifier);
  });
}

ui::test::InteractiveTestApi::StepBuilder
ShortcutIntegrationBrowserTestApi::LaunchShortcut(
    ui::ElementIdentifier identifier) {
  return InAnyContext(WithElement(identifier, [](ui::TrackedElement* element) {
    ShortcutCreationTestSupport::LaunchShortcut(GetShortcutPath(element));
  }));
}

// static
base::FilePath ShortcutIntegrationBrowserTestApi::GetShortcutPath(
    ui::TrackedElement* element) {
  return ShortcutIntegrationBrowserTestPrivate::GetShortcutPath(element);
}

ShortcutIntegrationBrowserTestPrivate&
ShortcutIntegrationBrowserTestApi::test_impl() {
  return static_cast<ShortcutIntegrationBrowserTestPrivate&>(
      private_test_impl());
}

}  // namespace shortcuts
