// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_INTEGRATION_TESTS_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_INTEGRATION_TESTS_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_

#include "ash/accelerators/accelerator_lookup.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace ash {

inline int kDeviceId1 = 15;
inline constexpr char kClickFn[] = "e => e.click()";
inline constexpr char kFocusFn[] = "e => e.focus()";

class ShortcutCustomizationInteractiveUiTestBase
    : public InteractiveAshTest {
 public:
  ShortcutCustomizationInteractiveUiTestBase();

  ShortcutCustomizationInteractiveUiTestBase(
      const ShortcutCustomizationInteractiveUiTestBase&) = delete;
  ShortcutCustomizationInteractiveUiTestBase& operator=(
      const ShortcutCustomizationInteractiveUiTestBase&) = delete;
  ~ShortcutCustomizationInteractiveUiTestBase() override;

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

  auto OpenEditShortcutDialog(const DeepQuery& query) {
    CHECK(webcontents_id_);
    const auto edit_button_query = query + "cr-icon-button.edit-button";
    return Steps(ExecuteJsAt(webcontents_id_, query, kFocusFn),
                 ExecuteJsAt(webcontents_id_, edit_button_query, kClickFn));
  }

  auto AddCustomShortcut(ui::Accelerator new_accel) {
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

  auto ResetShortcut(const DeepQuery& query) {
    CHECK(webcontents_id_);
    const auto edit_button_query = query + "cr-icon-button.edit-button";
    return Steps(
        ExecuteJsAt(webcontents_id_, query, kFocusFn),
        ExecuteJsAt(webcontents_id_, edit_button_query, kClickFn),
        ExecuteJsAt(webcontents_id_, kRestoreDefaultsButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

 auto ClickAddShortcutButton() {
    CHECK(webcontents_id_);
    return Steps(ExecuteJsAt(webcontents_id_, kAddShortcutButtonQuery, kClickFn));
  }

 auto ClickDoneButton() {
    CHECK(webcontents_id_);
    return Steps(ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  ui::test::InteractiveTestApi::MultiStep LaunchShortcutCustomizationApp();

  // Ensure focusing web contents doesn't accidentally block accelerator
  // processing. When adding new accelerators, this method is called to
  // prevent the system from processing Ash accelerators.
  ui::InteractionSequence::StepBuilder EnsureAcceleratorsAreProcessed();

  ui::test::InteractiveTestApi::MultiStep SendShortcutAccelerator(
      ui::Accelerator accel);

  ui::Accelerator GetDefaultAcceleratorForAction(AcceleratorAction action) {
    return Shell::Get()
        ->accelerator_lookup()
        ->GetAcceleratorsForAction(action)
        .front()
        .accelerator;
  }

  ui::test::InteractiveTestApi::MultiStep AddKeyboard(bool is_external);

  ui::test::InteractiveTestApi::MultiStep
  WaitForShortcutToContainNumAcceleartors(const DeepQuery& query,
                                          const int expected);

  void SetUpOnMainThread() override;

 protected:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SHORTCUT_CUSTOMIZATION_INTEGRATION_TESTS_SHORTCUT_CUSTOMIZATION_TEST_BASE_H_
