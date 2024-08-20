// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "extensions/browser/extension_system.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

ReadAnythingCoordinator::ReadAnythingCoordinator(Browser* browser)
    : browser_(browser) {}

ReadAnythingCoordinator::~ReadAnythingCoordinator() {
  local_side_panel_switch_delay_timer_.Stop();

  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    RemoveGDocsHelperExtension();
  }

  // Deregister Read Anything from the global side panel registry. This removes
  // Read Anything as a side panel entry observer.

  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    BrowserList::GetInstance()->RemoveObserver(this);
  }
}

void ReadAnythingCoordinator::Initialize() {
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    BrowserList::GetInstance()->AddObserver(this);
  }

  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    EmbeddedA11yExtensionLoader::GetInstance()->Init();
  }
}

void ReadAnythingCoordinator::AddObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingCoordinator::RemoveObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingCoordinator::OnEntryShown(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  OnReadAnythingSidePanelEntryShown();
}

void ReadAnythingCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  OnReadAnythingSidePanelEntryHidden();
}

void ReadAnythingCoordinator::OnReadAnythingSidePanelEntryShown() {
  for (Observer& obs : observers_) {
    obs.Activate(true);
  }

  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  active_local_side_panel_count_++;
  InstallGDocsHelperExtension();
}

void ReadAnythingCoordinator::OnReadAnythingSidePanelEntryHidden() {
  for (Observer& obs : observers_) {
    obs.Activate(false);
  }

  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  active_local_side_panel_count_--;
  local_side_panel_switch_delay_timer_.Stop();
  local_side_panel_switch_delay_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindRepeating(
          &ReadAnythingCoordinator::OnLocalSidePanelSwitchDelayTimeout,
          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<views::View> ReadAnythingCoordinator::CreateContainerView() {
  auto web_view =
      std::make_unique<ReadAnythingSidePanelWebView>(browser_->profile());

  return std::move(web_view);
}

void ReadAnythingCoordinator::InstallGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(true);
#else
  EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#else
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser_->profile())
          ->extension_service();
  if (!service) {
    // In tests, the service might not be created.
    CHECK_IS_TEST();
    return;
  }
  extensions::ComponentLoader* component_loader = service->component_loader();
  if (!component_loader->Exists(
          extension_misc::kReadingModeGDocsHelperExtensionId)) {
    component_loader->Add(
        IDR_READING_MODE_GDOCS_HELPER_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("reading_mode_gdocs_helper")));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ReadAnythingCoordinator::RemoveGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(false);
#else
  EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#else
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser_->profile())
          ->extension_service();
  if (!service) {
    // In tests, the service might not be created.
    CHECK_IS_TEST();
    return;
  }
  service->component_loader()->Remove(
      extension_misc::kReadingModeGDocsHelperExtensionId);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ReadAnythingCoordinator::OnBrowserSetLastActive(Browser* browser) {
  if (!features::IsDataCollectionModeForScreen2xEnabled() ||
      browser != browser_) {
    return;
  }
  // This code is called as part of a screen2x data generation workflow, where
  // the browser is opened by a CLI and the read-anything side panel is
  // automatically opened. Therefore we force the UI to show right away, as in
  // tests.
  auto* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (side_panel_ui->GetCurrentEntryId() != SidePanelEntryId::kReadAnything) {
    side_panel_ui->SetNoDelaysForTesting(true);  // IN-TEST
    side_panel_ui->Show(SidePanelEntryId::kReadAnything);
  }
}

void ReadAnythingCoordinator::OnLocalSidePanelSwitchDelayTimeout() {
  if (active_local_side_panel_count_ > 0) {
    return;
  }

  RemoveGDocsHelperExtension();
}
