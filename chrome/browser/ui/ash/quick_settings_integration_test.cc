// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

class QuickSettingsIntegrationTest : public InteractiveAshTest {
 public:
  QuickSettingsIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickSettingsIntegrationTest, OpenOsSettings) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-0a165660-211a-4bb6-9843-a1b1d55c8694");

  SetupContextWidget();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsElementId);

  RunTestSequence(
      Log("Opening quick settings bubble"),
      PressButton(kUnifiedSystemTrayElementId),
      WaitForShow(kQuickSettingsViewElementId),

      Log("Clicking settings button"),
      InstrumentNextTab(kOsSettingsElementId, AnyBrowser()),
      PressButton(kQuickSettingsSettingsButtonElementId),
      WaitForShow(kOsSettingsElementId),

      Log("Verifying that OS Settings loads"),
      WaitForWebContentsReady(kOsSettingsElementId,
                              GURL(chrome::kChromeUIOSSettingsURL)));
}

}  // namespace
}  // namespace ash
