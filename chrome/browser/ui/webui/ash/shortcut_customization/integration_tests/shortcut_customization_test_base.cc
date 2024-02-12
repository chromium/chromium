// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"

namespace ash {

ShortcutCustomizationInteractiveUiTestBase::
    ShortcutCustomizationInteractiveUiTestBase() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id_ = kShortcutAppWebContentsId;

  feature_list_.InitWithFeatures({::features::kShortcutCustomization,
                                  ::features::kShortcutCustomizationApp},
                                 {});
}

ShortcutCustomizationInteractiveUiTestBase::
    ~ShortcutCustomizationInteractiveUiTestBase() = default;

void ShortcutCustomizationInteractiveUiTestBase::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();

  // Set up context for element tracking for InteractiveBrowserTest.
  SetupContextWidget();

  // Ensure the Shortcut Customization system web app (SWA) is installed.
  InstallSystemApps();
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::LaunchShortcutCustomizationApp() {
  return Steps(
      Log("Opening Shortcut Customization app"),
      InstrumentNextTab(webcontents_id_, AnyBrowser()), Do([&]() {
        CreateBrowserWindow(GURL(kChromeUIShortcutCustomizationAppURL));
      }),
      WaitForShow(webcontents_id_),
      Log("Waiting for Shortcut Customization app to load"),
      WaitForWebContentsReady(webcontents_id_,
                              GURL(kChromeUIShortcutCustomizationAppURL)));
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
