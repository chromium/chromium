// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace ash {

inline constexpr char kClickFn[] = "e => e.click()";
inline constexpr char kFocusFn[] = "e => e.focus()";

class ShortcutCustomizationInteractiveUiTestBase
    : public SystemWebAppBrowserTestBase {
 public:
  ShortcutCustomizationInteractiveUiTestBase();

  ShortcutCustomizationInteractiveUiTestBase(
      const ShortcutCustomizationInteractiveUiTestBase&) = delete;
  ShortcutCustomizationInteractiveUiTestBase& operator=(
      const ShortcutCustomizationInteractiveUiTestBase&) = delete;
  ~ShortcutCustomizationInteractiveUiTestBase() override;

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

  const DeepQuery kEditShortcutButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "accelerator-edit-view",
      "#editButton",
  };

  const DeepQuery kRestoreDefaultsButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#restoreDefault",
  };

  auto OpenCalendarShortcutDialog() {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kCalendarAcceleratorRowQuery, kFocusFn),
        ExecuteJsAt(webcontents_id_, kEditButtonQuery, kClickFn));
  }

  auto AddCustomCalendarShortcut(ui::Accelerator new_accel) {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kAddShortcutButtonQuery, kClickFn),
        InAnyContext(SendAccelerator(webcontents_id_, new_accel)),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto EditDefaultShortcut(ui::Accelerator new_accel) {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kEditShortcutButtonQuery, kClickFn),
        InAnyContext(SendAccelerator(webcontents_id_, new_accel)),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto ResetCalendarShortcuts() {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kCalendarAcceleratorRowQuery, kFocusFn),
        ExecuteJsAt(webcontents_id_, kEditButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id_, kRestoreDefaultsButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  ui::InteractionSequence::StepBuilder LaunchShortcutCustomizationApp();

  // Ensure focusing web contents doesn't accidentally block accelerator
  // processing. When adding new accelerators, this method is called to
  // prevent the system from processing Ash accelerators.
  ui::InteractionSequence::StepBuilder EnsureAcceleratorsAreProcessed();

  ui::test::InteractiveTestApi::MultiStep SendShortcutAccelerator(
      ui::Accelerator accel);

  void SetUpOnMainThread() override;

 protected:
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_
