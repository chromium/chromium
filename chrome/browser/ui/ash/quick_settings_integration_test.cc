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
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

EnterpriseDomainModel* GetEnterpriseDomainModel() {
  return Shell::Get()->system_tray_model()->enterprise_domain();
}

class QuickSettingsIntegrationTest : public InteractiveAshTest {
 public:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

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

// Observes the aura environment to detect the Lacros window title.
class LacrosWindowTitleObserver
    : public ui::test::ObservationStateObserver<std::u16string,
                                                aura::Env,
                                                aura::EnvObserver>,
      public aura::WindowObserver {
 public:
  LacrosWindowTitleObserver()
      : ObservationStateObserver(aura::Env::GetInstance()) {}

  ~LacrosWindowTitleObserver() override {
    if (lacros_window_) {
      lacros_window_->RemoveObserver(this);
      lacros_window_ = nullptr;
    }
  }

  // ui::test::ObservationStateObserver:
  std::u16string GetStateObserverInitialState() const override {
    // Tests in this suite do not have a lacros browser window open at start.
    return {};
  }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    CHECK(window);
    if (crosapi::browser_util::IsLacrosWindow(window)) {
      lacros_window_ = window;
      OnStateObserverStateChanged(lacros_window_->GetTitle());
      // Observe for window title changes (the initial window title is set
      // asynchronously).
      lacros_window_->AddObserver(this);
    }
  }

  // aura::WindowObserver:
  void OnWindowTitleChanged(aura::Window* window) override {
    CHECK_EQ(window, lacros_window_);
    OnStateObserverStateChanged(lacros_window_->GetTitle());
  }

  void OnWindowDestroying(aura::Window* window) override {
    CHECK_EQ(window, lacros_window_);
    lacros_window_->RemoveObserver(this);
    lacros_window_ = nullptr;
  }

 private:
  raw_ptr<aura::Window> lacros_window_ = nullptr;
};

// Tests of ash quick settings that assume the primary browser is Lacros.
class QuickSettingsLacrosIntegrationTest : public QuickSettingsIntegrationTest {
 public:
  QuickSettingsLacrosIntegrationTest() {
    feature_list_.InitAndEnableFeature(
        ash::standalone_browser::features::kLacrosOnly);
  }

  // InteractiveAshTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    QuickSettingsIntegrationTest::SetUpCommandLine(command_line);
    SetUpCommandLineForLacros(command_line);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Flaky because Lacros can be older than Ash in chromeos_integration_tests,
// causing NOTREACHED failures at the crosapi level. b/303359438
IN_PROC_BROWSER_TEST_F(QuickSettingsLacrosIntegrationTest,
                       DISABLED_ManagedDeviceInfo) {
  ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());

  base::AddFeatureIdTagToTestResult(
      "screenplay-f0a4bd11-a2bc-4596-a4fc-2a62c7277965");

  SetupContextWidget();

  // Simulate enterprise information being available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(LacrosWindowTitleObserver,
                                      kLacrosWindowTitle);

  // The test will launch Lacros, so ensure the wayland server is
  // running and crosapi is ready.
  WaitForAshFullyStarted();
  ASSERT_TRUE(crosapi::CrosapiManager::Get());
  ASSERT_TRUE(crosapi::CrosapiManager::Get()->crosapi_ash());

  RunTestSequence(
      Log("Opening quick settings bubble"),
      PressButton(kUnifiedSystemTrayElementId),
      WaitForShow(kQuickSettingsViewElementId),

      Log("Pressing enterprise managed view"),
      ObserveState(kLacrosWindowTitle,
                   std::make_unique<LacrosWindowTitleObserver>()),
      PressButton(kEnterpriseManagedView),

      Log("Waiting for the lacros browser to load the management page"),
      WaitForState(kLacrosWindowTitle,
                   l10n_util::GetStringUTF16(IDS_MANAGEMENT_TITLE)),

      Log("Test complete"));
}

#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

}  // namespace
}  // namespace ash
