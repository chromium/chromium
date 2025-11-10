// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_service.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/read_anything/read_anything_service_factory.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "extensions/browser/extension_system.h"
#include "ui/accessibility/accessibility_features.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"
#include "chrome/browser/extensions/component_loader.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace {

// The number of seconds to wait before removing the extension. This avoids
// removing an extension only to add it back immediately.
constexpr int kRemoveExtensionDelaySeconds = 30;

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS)
const base::FilePath::CharType kManifestV3FileName[] =
    FILE_PATH_LITERAL("wasm_tts_manifest_v3.json");
#endif  // !BUILDFLAG(IS_CHROMEOS)

ReadAnythingService::ReadAnythingService(Profile* profile) : profile_(profile) {
  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    EmbeddedA11yExtensionLoader::GetInstance()->Init();

    // The extension may still be installed from a previous session. Queue the
    // timer to uninstall it.
    // TODO(https://crbug.com/362787711): This logic also needs to run if the
    // feature is disabled.
    local_side_panel_switch_delay_timer_.Start(
        FROM_HERE, base::Seconds(kRemoveExtensionDelaySeconds),
        base::BindRepeating(
            &ReadAnythingService::OnLocalSidePanelSwitchDelayTimeout,
            weak_ptr_factory_.GetWeakPtr()));
  }
  if (features::IsDataCollectionModeForScreen2xEnabled() &&
      profile_->AllowsBrowserWindows()) {
    browser_collection_observer_.Observe(
        ProfileBrowserCollection::GetForProfile(profile_));
  }
}

// The service is shutting down which means the profile is destroying, at which
// point we should not be re-entrantly trying to modify the profile by removing
// the extension. Instead remove the extension at startup.
ReadAnythingService::~ReadAnythingService() = default;

// static
ReadAnythingService* ReadAnythingService::Get(Profile* profile) {
  return ReadAnythingServiceFactory::GetInstance()->GetForBrowserContext(
      profile);
}

void ReadAnythingService::OnReadAnythingSidePanelEntryShown() {
// The TTS download extension should only be installed on non-ChromeOS devices
// when the Read Aloud flag is enabled.
#if !BUILDFLAG(IS_CHROMEOS)
  SetupDesktopEngine();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  active_local_side_panel_count_++;
  InstallGDocsHelperExtension();
}

#if !BUILDFLAG(IS_CHROMEOS)
void ReadAnythingService::SetupDesktopEngine() {
  // If the extension was previously installed but now the Read Aloud flag
  // is disabled, or if the component updater flag is enabled, we should
  // uninstall the component extension.
  // TODO(crbug.com/428043296): RemoveTtsDownloadExtension should be left in
  // until the IsWasmTtsComponentUpdaterEnabled flag has been removed for
  // enough time to be sure that no one has that extension installed. If they
  // do, it could cause issues when the component updater extension is
  // installed.
  RemoveTtsDownloadExtension();

  // Install the TTS extension via the component updater if the
  // component updater flag is enabled.
  if (features::IsReadAnythingReadAloudEnabled() &&
      !features::IsWasmTtsEngineAutoInstallDisabled()) {
    // Signal that the reading mode panel is opened and it's now safe to
    // install the WasmTtsEngineComponent.
    component_updater::WasmTtsEngineComponentInstallerPolicy::
        GetWasmTTSEngineDirectory(base::BindOnce(InstallComponent));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void ReadAnythingService::OnReadAnythingSidePanelEntryHidden() {
  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  active_local_side_panel_count_--;
  local_side_panel_switch_delay_timer_.Reset();
}

void ReadAnythingService::InstallGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS)
  EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
#else
  auto* component_loader = extensions::ComponentLoader::Get(profile_);
  if (!component_loader) {
    // In tests, the loader might not be created.
    CHECK_IS_TEST();
    return;
  }
  if (!component_loader->Exists(
          extension_misc::kReadingModeGDocsHelperExtensionId)) {
    component_loader->Add(
        IDR_READING_MODE_GDOCS_HELPER_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("reading_mode_gdocs_helper")));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ReadAnythingService::RemoveGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS)
  EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId);
#else
  auto* component_loader = extensions::ComponentLoader::Get(profile_);
  if (!component_loader) {
    // In tests, the loader might not be created.
    CHECK_IS_TEST();
    return;
  }
  component_loader->Remove(extension_misc::kReadingModeGDocsHelperExtensionId);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ReadAnythingService::OnLocalSidePanelSwitchDelayTimeout() {
  if (active_local_side_panel_count_ > 0) {
    return;
  }

  RemoveGDocsHelperExtension();
}

void ReadAnythingService::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (!features::IsDataCollectionModeForScreen2xEnabled()) {
    return;
  }

  // This code is called as part of a screen2x data generation workflow, where
  // the browser is opened by a CLI and the read-anything side panel is
  // automatically opened. Therefore we force the UI to show right away, as in
  // tests.
  // TODO(https://crbug.com/358191922): Remove this code.
  auto* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (!side_panel_ui->IsSidePanelEntryShowing(
          SidePanelEntryKey(SidePanelEntryId::kReadAnything))) {
    side_panel_ui->SetNoDelaysForTesting(true);  // IN-TEST
    side_panel_ui->Show(SidePanelEntryId::kReadAnything);
  }
}

void ReadAnythingService::RemoveTtsDownloadExtension() {
#if !BUILDFLAG(IS_CHROMEOS)
  // Remove the legacy TTS extension for all profiles.

  // This code for removing the extension installed in the legacy way
  // should remain in place until at least milestone 141 to ensure there
  // are no conflicts with installing the component loader extension.
  EmbeddedA11yExtensionLoader::GetInstance()->Init();
  EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
      extension_misc::kTTSEngineExtensionId);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

#if !BUILDFLAG(IS_CHROMEOS)
void ReadAnythingService::InstallComponent(const base::FilePath& new_dir) {
  RecordEngineVersion(new_dir.BaseName());
  EmbeddedA11yExtensionLoader::GetInstance()->Init();
  EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithIdAndPath(
      extension_misc::kComponentUpdaterTTSEngineExtensionId, new_dir,
      kManifestV3FileName,
      /*should_localize=*/false);

  // Store the last time reading mode was opened and the TTS engine was
  // installed to be used to uninstall voices if reading mode is unopened for a
  // long time.
  g_browser_process->local_state()->SetTime(
      prefs::kAccessibilityReadAnythingDateLastOpened, base::Time::Now());
  g_browser_process->local_state()->SetBoolean(
      prefs::kAccessibilityReadAnythingTTSEngineReinstalled, false);
}
void ReadAnythingService::RecordEngineVersion(
    const base::FilePath& engine_version) {
// Per FilePath documentation, Windows uses std::wstring, so string
// so string manipulations must be handled slightly differently.
#if BUILDFLAG(IS_WIN)
  using path_string_t = std::wstring;
  constexpr auto delimiter = L'.';
#else
  using path_string_t = std::string;
  constexpr auto delimiter = '.';
#endif

  path_string_t file = engine_version.value();

  int version_number = 0;

  size_t pos = file.find(delimiter);
  if (pos != std::string::npos) {
    file.erase(pos, 1);
  }

  // In order for the engine to be recognized by component updater, it must be
  // of the format YYYYMMDD.x. Convert the string representation of the engine
  // version to an integer of format YYYMMDDx in order to be logged.
  if (base::StringToInt(file, &version_number)) {
    base::UmaHistogramSparse(
        "Accessibility.ReadAnything.ReadAloud.EngineVersion", version_number);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
