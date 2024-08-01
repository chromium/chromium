// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace {

// Stores a bit for whether the user acknowledged the dialog informing the
// extension being disabled as part of the MV2 deprecation.
constexpr PrefMap kMV2DeprecationDisabledDialogAcknowledgedPref = {
    "mv2_deprecation_disabled_dialog_ack", PrefType::kBool,
    PrefScope::kExtensionSpecific};

}  // namespace

Mv2DisabledDialogController::Mv2DisabledDialogController(Browser* browser)
    : browser_(browser) {
  ManifestV2ExperimentManager* experiment_manager_ =
      ManifestV2ExperimentManager::Get(browser_->profile());
  CHECK(experiment_manager_);

  MV2ExperimentStage experiment_stage =
      experiment_manager_->GetCurrentExperimentStage();
  CHECK_EQ(experiment_stage, MV2ExperimentStage::kDisableWithReEnable);

  // Dialog should only be visible once.
  if (experiment_manager_->has_triggered_disabled_dialog()) {
    return;
  }

  experiment_manager_->SetHasTriggeredDisabledDialog(true);
  if (experiment_manager_->is_manager_ready()) {
    MaybeShowDisabledDialog();
  } else {
    show_dialog_subscription_ =
        experiment_manager_->RegisterOnManagerReadyCallback(base::BindRepeating(
            &Mv2DisabledDialogController::MaybeShowDisabledDialog,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

Mv2DisabledDialogController::~Mv2DisabledDialogController() = default;

void Mv2DisabledDialogController::TearDown() {
  show_dialog_subscription_ = base::CallbackListSubscription();
}

void Mv2DisabledDialogController::MaybeShowDisabledDialogForTesting() {
  CHECK_IS_TEST();
  MaybeShowDisabledDialog();
}

void Mv2DisabledDialogController::MaybeShowDisabledDialog() {
  Profile* profile = browser_->profile();
  auto* extension_registry = ExtensionRegistry::Get(profile);
  auto* extension_prefs = ExtensionPrefs::Get(profile);
  ManagementPolicy* policy = ExtensionSystem::Get(profile)->management_policy();

  std::vector<ExtensionId> extensions_to_include;
  for (const auto& extension : extension_registry->disabled_extensions()) {
    // Exclude extensions that are not disabled due to the MV2 deprecation.
    if (!extension_prefs->HasDisableReason(
            extension->id(),
            disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
      continue;
    }

    // Exclude extensions that cannot be uninstalled.
    if (policy->MustRemainInstalled(extension.get(), nullptr) ||
        !policy->UserMayModifySettings(extension.get(), nullptr)) {
      continue;
    }

    // Exclude extensions that were already acknowledged on a previous disabled
    // dialog.
    bool was_acknowledged = false;
    extension_prefs->ReadPrefAsBoolean(
        extension->id(), kMV2DeprecationDisabledDialogAcknowledgedPref,
        &was_acknowledged);
    if (was_acknowledged) {
      continue;
    }

    extensions_to_include.push_back(extension->id());
  }

  if (extensions_to_include.empty()) {
    return;
  }

  ShowMv2DeprecationDisabledDialog(
      browser_, extensions_to_include,
      base::BindOnce(&Mv2DisabledDialogController::OnRemoveSelected,
                     weak_ptr_factory_.GetWeakPtr(), extensions_to_include),
      base::BindOnce(&Mv2DisabledDialogController::OnManageSelected,
                     weak_ptr_factory_.GetWeakPtr(), extensions_to_include),
      base::BindOnce(&Mv2DisabledDialogController::UserAcknowledgedDialog,
                     weak_ptr_factory_.GetWeakPtr(), extensions_to_include));
}

void Mv2DisabledDialogController::OnRemoveSelected(
    const std::vector<ExtensionId>& extension_ids) {
  UserAcknowledgedDialog(extension_ids);

  auto* extension_service =
      ExtensionSystem::Get(browser_->profile())->extension_service();
  auto* extension_registry = ExtensionRegistry::Get(browser_->profile());

  for (const auto& extension_id : extension_ids) {
    const Extension* current_extension = extension_registry->GetExtensionById(
        extension_id, ExtensionRegistry::EVERYTHING);
    // Extensions can be uninstalled externally while the dialog is open. Only
    // uninstall extensions that are still existent.
    if (!current_extension) {
      continue;
    }

    // If an extension fails to be uninstalled, it will not pause the
    // uninstall of the other extensions on the list.
    extension_service->UninstallExtension(
        extension_id, UNINSTALL_REASON_USER_INITIATED, nullptr);
  }
}

void Mv2DisabledDialogController::OnManageSelected(
    const std::vector<ExtensionId>& extension_ids) {
  UserAcknowledgedDialog(extension_ids);
  chrome::ShowExtensions(browser_);
}

void Mv2DisabledDialogController::UserAcknowledgedDialog(
    const std::vector<ExtensionId>& extension_ids) {
  // Store the extensions included in the dialog, so we don't show the
  // dialog for them again.
  auto* extension_prefs = ExtensionPrefs::Get(browser_->profile());
  for (const auto& extension_id : extension_ids) {
    extension_prefs->SetBooleanPref(
        extension_id, kMV2DeprecationDisabledDialogAcknowledgedPref, true);
  }
}

}  // namespace extensions
