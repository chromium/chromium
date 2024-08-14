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

namespace {

base::TimeDelta kDelaySeconds = base::Seconds(2);

}  // namespace

ReadAnythingCoordinator::ReadAnythingCoordinator(Browser* browser)
    : delay_timer_(FROM_HERE,
                   kDelaySeconds,
                   base::BindRepeating(
                       &ReadAnythingCoordinator::OnTabChangeDelayComplete,
                       base::Unretained(this))),
      browser_(browser) {}

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
  browser_->tab_strip_model()->RemoveObserver(this);
  Observe(nullptr);
}

void ReadAnythingCoordinator::Initialize() {
  browser_->tab_strip_model()->AddObserver(this);
  Observe(GetActiveWebContents());

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

void ReadAnythingCoordinator::StartPageChangeDelay() {
  // Reset the delay status.
  post_tab_change_delay_complete_ = false;
  // Cancel any existing page change delay and start again.
  delay_timer_.Reset();
}

void ReadAnythingCoordinator::OnTabChangeDelayComplete() {
  CHECK(!post_tab_change_delay_complete_);
  post_tab_change_delay_complete_ = true;
  auto* web_contents = GetActiveWebContents();
  // Web contents should be checked before starting the delay, and the timer
  // will be canceled if the user navigates or leaves the tab.
  CHECK(web_contents);
  if (!web_contents->IsLoading()) {
    // Ability to show was already checked before timer was started.
    ActivePageDistillable();
  }
}

void ReadAnythingCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  Observe(GetActiveWebContents());
  if (IsActivePageDistillable()) {
    StartPageChangeDelay();
  } else {
    ActivePageNotDistillable();
  }
}

void ReadAnythingCoordinator::DidStopLoading() {
  if (!post_tab_change_delay_complete_) {
    return;
  }
  if (IsActivePageDistillable()) {
    ActivePageDistillable();
  } else {
    ActivePageNotDistillable();
  }
}

void ReadAnythingCoordinator::PrimaryPageChanged(content::Page& page) {
  // On navigation, cancel any running delays.
  delay_timer_.Stop();

  if (!IsActivePageDistillable()) {
    // On navigation, if we shouldn't show the IPH hide it. Otherwise continue
    // to show it.
    ActivePageNotDistillable();
  }
}

content::WebContents* ReadAnythingCoordinator::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool ReadAnythingCoordinator::IsActivePageDistillable() const {
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  auto url = web_contents->GetLastCommittedURL();

  for (const std::string& distillable_domain : a11y::GetDistillableDomains()) {
    // If the url's domain is found in distillable domains AND the url has a
    // filename (i.e. it is not a home page or sub-home page), show the promo.
    if (url.DomainIs(distillable_domain) && !url.ExtractFileName().empty()) {
      return true;
    }
  }
  return false;
}

void ReadAnythingCoordinator::ActivePageNotDistillable() {
  browser_->window()->CloseFeaturePromo(
      feature_engagement::kIPHReadingModeSidePanelFeature);
  for (Observer& obs : observers_) {
    obs.OnActivePageDistillable(false);
  }
}

void ReadAnythingCoordinator::ActivePageDistillable() {
  browser_->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHReadingModeSidePanelFeature);
  for (Observer& obs : observers_) {
    obs.OnActivePageDistillable(true);
  }
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

void ReadAnythingCoordinator::ActivePageNotDistillableForTesting() {
  ActivePageNotDistillable();
}

void ReadAnythingCoordinator::ActivePageDistillableForTesting() {
  ActivePageDistillable();
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
