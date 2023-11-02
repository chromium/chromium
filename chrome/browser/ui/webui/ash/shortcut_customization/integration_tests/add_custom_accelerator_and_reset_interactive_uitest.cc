// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>

#include "ash/ash_element_identifiers.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kClickFn[] = "e => e.click()";
constexpr char kFocusFn[] = "e => e.focus()";

class ShortcutCustomizationInteractiveUiTest
    : public SystemWebAppBrowserTestBase {
 public:
  ShortcutCustomizationInteractiveUiTest() {
    feature_list_.InitWithFeatures({::features::kShortcutCustomization,
                                    ::features::kShortcutCustomizationApp},
                                   {});
  }

  auto LaunchShortcutCustomizationApp() {
    return Do([&]() {
      LaunchApp(
          LaunchParamsForApp(ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION));
    });
  }

  // Query to pierce through Shadow DOM to find the keyboard.
  const DeepQuery kEditButtonQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 93 corresponds to the "Open/Close Calendar" shortcut.
      "accelerator-row[action='93']",
      "cr-icon-button.edit-button",
  };

  const DeepQuery kCalendarAcceleratorRowQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 93 corresponds to the "Open/Close Calendar" shortcut.
      "accelerator-row[action='93']",
  };

  const DeepQuery kAddShortcutButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#addAcceleratorButton",
  };

  const DeepQuery kDoneButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#doneButton",
  };

  const DeepQuery kRestoreDefaultsButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#restoreDefault",
  };

  auto OpenCalendarShortcutDialog() {
    return Steps(
        ExecuteJsAt(webcontents_id, kCalendarAcceleratorRowQuery, kFocusFn),
        ExecuteJsAt(webcontents_id, kEditButtonQuery, kClickFn));
  }

  auto AddCustomCalendarShortcut(ui::Accelerator new_accel) {
    return Steps(ExecuteJsAt(webcontents_id, kAddShortcutButtonQuery, kClickFn),
                 InAnyContext(SendAccelerator(webcontents_id, new_accel)),
                 ExecuteJsAt(webcontents_id, kDoneButtonQuery, kClickFn));
  }

  auto ResetCalendarShortcuts() {
    return Steps(
        ExecuteJsAt(webcontents_id, kCalendarAcceleratorRowQuery, kFocusFn),
        ExecuteJsAt(webcontents_id, kEditButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id, kRestoreDefaultsButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id, kDoneButtonQuery, kClickFn));
  }

  // Ensure focusing web contents doesn't accidentally block accelerator
  // processing. When adding new accelerators, this method is called to
  // prevent the system from processing Ash accelerators.
  ui::InteractionSequence::StepBuilder EnsureAcceleratorsAreProcessed() {
    return ExecuteJs(webcontents_id,
                     "() => "
                     "document.querySelector('shortcut-customization-app')."
                     "shortcutProvider.preventProcessingAccelerators(false)");
  }

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();

    // Ensure the Shortcut Customization system web app (SWA) is installed.
    WaitForTestSystemAppInstall();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id;
};

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddCustomAcceleratorAndReset) {
  ui::Accelerator default_accel(ui::VKEY_C, ui::EF_COMMAND_DOWN);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id = kShortcutAppWebContentsId;
  RunTestSequence(
      InstrumentNextTab(kShortcutAppWebContentsId, AnyBrowser()),
      LaunchShortcutCustomizationApp(),
      WaitForWebContentsReady(kShortcutAppWebContentsId,
                              GURL("chrome://shortcut-customization")),
      InAnyContext(Steps(
          SendAccelerator(webcontents_id, new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Verify that the custom shortcut does not open the calendar "
              "before it's added as a shortcut"),
          OpenCalendarShortcutDialog(), AddCustomCalendarShortcut(new_accel),
          Log("Adding Search + Ctrl + n as a custom open/close calendar "
              "shortcut"),
          FocusWebContents(webcontents_id), EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id, new_accel), FlushEvents(),
          WaitForShow(kCalendarViewElementId),
          Log("Custom shortcut opens calendar"),
          SendAccelerator(webcontents_id, new_accel), FlushEvents(),
          WaitForHide(kCalendarViewElementId),
          Log("Custom shortcut closes calendar"), ResetCalendarShortcuts(),
          Log("Open/Close calendar shortcut reset to defaults"),
          FocusWebContents(webcontents_id), EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id, default_accel), FlushEvents(),
          WaitForShow(kCalendarViewElementId),
          SendAccelerator(webcontents_id, default_accel), FlushEvents(),
          WaitForHide(kCalendarViewElementId),
          Log("Default shortcut still works"),
          SendAccelerator(webcontents_id, new_accel), FlushEvents(),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Custom shortcut no longer works"))));
}

}  // namespace
}  // namespace ash
