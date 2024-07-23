// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/test/gtest_tags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

EnterpriseDomainModel* GetEnterpriseDomainModel() {
  return Shell::Get()->system_tray_model()->enterprise_domain();
}

class QuickSettingsIntegrationTest : public AshIntegrationTest {
 public:
  // AshIntegrationTest:
  void SetUpOnMainThread() override {
    AshIntegrationTest::SetUpOnMainThread();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();
  }
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

IN_PROC_BROWSER_TEST_F(QuickSettingsIntegrationTest, ManagedDeviceInfo) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3d8236e6-8c42-428c-87e6-9c9e3bac7ddb");

  SetupContextWidget();

  // Simulate enterprise information being available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(DeviceEnterpriseInfo{
      "example.com", ManagementDeviceMode::kChromeEnterprise});

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kManagementElementId);

  RunTestSequence(Log("Opening quick settings bubble"),
                  PressButton(kUnifiedSystemTrayElementId),
                  WaitForShow(kQuickSettingsViewElementId),

                  Log("Pressing enterprise managed view"),
                  InstrumentNextTab(kManagementElementId, AnyBrowser()),
                  PressButton(kEnterpriseManagedView),

                  Log("Waiting for chrome://management to load"),
                  WaitForWebContentsReady(kManagementElementId,
                                          GURL("chrome://management")),

                  Log("Test complete"));
}

}  // namespace
}  // namespace ash
