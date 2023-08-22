// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {
namespace {

EnterpriseDomainModel* GetEnterpriseDomainModel() {
  return Shell::Get()->system_tray_model()->enterprise_domain();
}

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

IN_PROC_BROWSER_TEST_F(QuickSettingsIntegrationTest, ManagedDeviceInfo) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3d8236e6-8c42-428c-87e6-9c9e3bac7ddb");

  SetupContextWidget();

  // Simulate enterprise information being available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});

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

// Testing with Lacros requires a VM or DUT.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)

// Observes the crosapi browser manager to detect Lacros startup. Signals with a
// boolean that is true if lacros is running, false otherwise.
class TestBrowserManagerObserver : public ui::test::ObservationStateObserver<
                                       bool,
                                       crosapi::BrowserManager,
                                       crosapi::BrowserManagerObserver> {
 public:
  TestBrowserManagerObserver()
      : ObservationStateObserver(crosapi::BrowserManager::Get()) {}

  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override {
    // Tests in this suite do not have a lacros browser window open at start.
    return false;
  }

  // crosapi::BrowserManagerObserver:
  void OnStateChanged() override {
    const bool is_lacros_running =
        crosapi::BrowserManager::Get()->IsRunningOrWillRun();
    OnStateObserverStateChanged(is_lacros_running);
  }
};

// Tests of ash quick settings that assume the primary browser is Lacros.
class QuickSettingsLacrosIntegrationTest : public QuickSettingsIntegrationTest {
 public:
  QuickSettingsLacrosIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kLacrosOnly);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickSettingsLacrosIntegrationTest, ManagedDeviceInfo) {
  ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());

  base::AddFeatureIdTagToTestResult(
      "screenplay-f0a4bd11-a2bc-4596-a4fc-2a62c7277965");

  SetupContextWidget();

  // Simulate enterprise information being available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TestBrowserManagerObserver, kLacrosState);

  RunTestSequence(
      Log("Opening quick settings bubble"),
      PressButton(kUnifiedSystemTrayElementId),
      WaitForShow(kQuickSettingsViewElementId),

      Log("Pressing enterprise managed view"),
      ObserveState(kLacrosState,
                   std::make_unique<TestBrowserManagerObserver>()),
      PressButton(kEnterpriseManagedView),

      Log("Waiting for the lacros browser to start"),
      WaitForState(kLacrosState, true),

      // TODO(jamescook): Verify that Lacros loaded the management URL.

      Log("Test complete"));
}

#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

}  // namespace
}  // namespace ash
