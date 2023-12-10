// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/shortcut_customization_test_base.h"

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"

namespace ash {

ShortcutCustomizationInteractiveUiTestBase::
    ShortcutCustomizationInteractiveUiTestBase() {
  feature_list_.InitWithFeatures({::features::kShortcutCustomization,
                                  ::features::kShortcutCustomizationApp},
                                 {});
}

ShortcutCustomizationInteractiveUiTestBase::
    ~ShortcutCustomizationInteractiveUiTestBase() = default;

void ShortcutCustomizationInteractiveUiTestBase::SetUpOnMainThread() {
  SystemWebAppBrowserTestBase::SetUpOnMainThread();
  // Ensure the Shortcut Customization system web app (SWA) is installed.
  WaitForTestSystemAppInstall();
}

ui::InteractionSequence::StepBuilder
ShortcutCustomizationInteractiveUiTestBase::LaunchShortcutCustomizationApp() {
  return Do([&]() {
    LaunchApp(
        LaunchParamsForApp(ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION));
  });
}

// Ensure focusing web contents doesn't accidentally block accelerator
// processing. When adding new accelerators, this method is called to
// prevent the system from processing Ash accelerators.
ui::InteractionSequence::StepBuilder
ShortcutCustomizationInteractiveUiTestBase::EnsureAcceleratorsAreProcessed() {
  CHECK(webcontents_id_);
  return ExecuteJs(webcontents_id_,
                   "() => "
                   "document.querySelector('shortcut-customization-app')."
                   "shortcutProvider.preventProcessingAccelerators(false)");
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::SendShortcutAccelerator(
    ui::Accelerator accel) {
  CHECK(webcontents_id_);
  return Steps(SendAccelerator(webcontents_id_, accel), FlushEvents());
}

}  // namespace ash
