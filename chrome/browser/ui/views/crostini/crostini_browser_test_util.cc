// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"

#include <utility>

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// ChromeBrowserMainExtraParts used to install a FakeCrOSComponentManager.
class CrostiniBrowserTestChromeBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  explicit CrostiniBrowserTestChromeBrowserMainExtraParts(bool register_termina)
      : register_termina_(register_termina) {}

  component_updater::FakeCrOSComponentManager* cros_component_manager() {
    return cros_component_manager_ptr_;
  }

  content::NetworkConnectionChangeSimulator* connection_change_simulator() {
    return &connection_change_simulator_;
  }

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    auto cros_component_manager =
        std::make_unique<component_updater::FakeCrOSComponentManager>();
    cros_component_manager->set_supported_components(
        {imageloader::kTerminaComponentName});

    if (register_termina_) {
      cros_component_manager->set_registered_components(
          {imageloader::kTerminaComponentName});
      cros_component_manager->ResetComponentState(
          imageloader::kTerminaComponentName,
          component_updater::FakeCrOSComponentManager::ComponentInfo(
              component_updater::CrOSComponentManager::Error::NONE,
              base::FilePath("/dev/null"), base::FilePath("/dev/null")));
    }
    cros_component_manager_ptr_ = cros_component_manager.get();

    browser_process_platform_part_test_api_ =
        std::make_unique<BrowserProcessPlatformPartTestApi>(
            g_browser_process->platform_part());
    browser_process_platform_part_test_api_->InitializeCrosComponentManager(
        std::move(cros_component_manager));
  }
  // Ideally we'd call SetConnectionType in PostCreateThreads, but currently we
  // have to wait for PreProfileInit to complete, since that creatse the
  // ash::Shell that AshService needs in order to start.
  void PostProfileInit() override {
    connection_change_simulator_.SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
  }
  void PostMainMessageLoopRun() override {
    cros_component_manager_ptr_ = nullptr;
    browser_process_platform_part_test_api_->ShutdownCrosComponentManager();
    browser_process_platform_part_test_api_.reset();
  }

 private:
  const bool register_termina_;

  std::unique_ptr<BrowserProcessPlatformPartTestApi>
      browser_process_platform_part_test_api_;
  component_updater::FakeCrOSComponentManager* cros_component_manager_ptr_ =
      nullptr;

  content::NetworkConnectionChangeSimulator connection_change_simulator_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniBrowserTestChromeBrowserMainExtraParts);
};

CrostiniDialogBrowserTest::CrostiniDialogBrowserTest(bool register_termina)
    : register_termina_(register_termina) {
  scoped_feature_list_.InitAndEnableFeature(features::kCrostini);
}

CrostiniDialogBrowserTest::~CrostiniDialogBrowserTest() = default;

void CrostiniDialogBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  ChromeBrowserMainParts* chrome_browser_main_parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts);
  extra_parts_ =
      new CrostiniBrowserTestChromeBrowserMainExtraParts(register_termina_);
  chrome_browser_main_parts->AddParts(extra_parts_);
}

void CrostiniDialogBrowserTest::SetUp() {
  DialogBrowserTest::SetUp();
}

void CrostiniDialogBrowserTest::SetUpOnMainThread() {
  browser()->profile()->GetPrefs()->SetBoolean(
      crostini::prefs::kCrostiniEnabled, true);
}

void CrostiniDialogBrowserTest::SetConnectionType(
    network::mojom::ConnectionType connection_type) {
  extra_parts_->connection_change_simulator()->SetConnectionType(
      connection_type);
}

void CrostiniDialogBrowserTest::UnregisterTermina() {
  extra_parts_->cros_component_manager()->ResetComponentState(
      imageloader::kTerminaComponentName,
      component_updater::FakeCrOSComponentManager::ComponentInfo(
          component_updater::CrOSComponentManager::Error::INSTALL_FAILURE,
          base::FilePath(), base::FilePath()));
}
