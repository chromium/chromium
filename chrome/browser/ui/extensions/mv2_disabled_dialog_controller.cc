// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"

namespace extensions {

namespace {

// Stores a bit for whether the user acknowledged the dialog informing the
// extension being disabled durint the MV2 deprecation 'disable with re-enable'
// experiment stage.
constexpr PrefMap kMV2DeprecationDisabledDialogAcknowledgedPref = {
    "mv2_deprecation_disabled_dialog_ack", PrefType::kBool,
    PrefScope::kExtensionSpecific};

// Stores a bit for whether the user acknowledged the dialog informing the
// extension being disabled during the MV2 deprecation 'unsupported' experiment
// stage.
constexpr PrefMap kMV2DeprecationUnsupportedDisabledDialogAcknowledgedPref = {
    "mv2_deprecation_unsupported_disabled_dialog_ack", PrefType::kBool,
    PrefScope::kExtensionSpecific};

// Returns the pref that stores whether the user has acknowledged the MV2
// deprecation disabled dialog for a given extension in `experiment_stage`.
const PrefMap& GetDisabledDialogAcknowledgedPref(
    MV2ExperimentStage experiment_stage) {
  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
    case MV2ExperimentStage::kWarning:
      // There is no disabled dialog for this stage, thus extension cannot be
      // acknowledged.
      NOTREACHED();
    case MV2ExperimentStage::kDisableWithReEnable:
      return kMV2DeprecationDisabledDialogAcknowledgedPref;
    case MV2ExperimentStage::kUnsupported:
      return kMV2DeprecationUnsupportedDisabledDialogAcknowledgedPref;
  }
}

// Returns whether `extension` should be included in the disabled dialog.
bool IsExtensionAffected(const Extension& extension,
                         ExtensionPrefs* extension_prefs,
                         ManagementPolicy* policy,
                         const PrefMap& dialog_ack_pref) {
  // Exclude extensions that are not disabled due to the MV2 deprecation.
  if (!extension_prefs->HasDisableReason(
          extension.id(),
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
    return false;
  }

  // Exclude extensions that cannot be uninstalled.
  if (policy->MustRemainInstalled(&extension, nullptr) ||
      !policy->UserMayModifySettings(&extension, nullptr)) {
    return false;
  }

  // Exclude extensions that were already acknowledged on a previous disabled
  // dialog.
  bool was_acknowledged = false;
  extension_prefs->ReadPrefAsBoolean(extension.id(), dialog_ack_pref,
                                     &was_acknowledged);
  return !was_acknowledged;
}

}  // namespace

Mv2DisabledDialogController::Mv2DisabledDialogController(Browser* browser)
    : browser_(browser) {
  ManifestV2ExperimentManager* experiment_manager_ =
      ManifestV2ExperimentManager::Get(browser_->profile());
  CHECK(experiment_manager_);
  experiment_stage_ = experiment_manager_->GetCurrentExperimentStage();

  // Dialog should only be visible once.
  if (experiment_manager_->has_triggered_disabled_dialog()) {
    return;
  }

  experiment_manager_->SetHasTriggeredDisabledDialog(true);
  if (experiment_manager_->is_manager_ready()) {
    ComputeAffectedExtensions();
  } else {
    show_dialog_subscription_ =
        experiment_manager_->RegisterOnManagerReadyCallback(base::BindRepeating(
            &Mv2DisabledDialogController::ComputeAffectedExtensions,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

Mv2DisabledDialogController::~Mv2DisabledDialogController() = default;

void Mv2DisabledDialogController::TearDown() {
  show_dialog_subscription_ = base::CallbackListSubscription();
}

void Mv2DisabledDialogController::MaybeShowDisabledDialogForTesting() {
  CHECK_IS_TEST();
  ComputeAffectedExtensions();
}

void Mv2DisabledDialogController::ComputeAffectedExtensions() {
  auto* extension_registry = ExtensionRegistry::Get(browser_->profile());
  auto* extension_prefs = ExtensionPrefs::Get(browser_->profile());
  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_->profile())->management_policy();
  const PrefMap& dialog_ack_pref =
      GetDisabledDialogAcknowledgedPref(experiment_stage_);

  std::vector<const Extension*> affected_extensions;
  for (const scoped_refptr<const Extension>& extension :
       extension_registry->disabled_extensions()) {
    if (IsExtensionAffected(*extension, extension_prefs, policy,
                            dialog_ack_pref)) {
      affected_extensions.push_back(extension.get());
    }
  }

  // No extensions to show, do nothing.
  if (affected_extensions.empty()) {
    return;
  }

  // Retrieve the extension icon for all the affected extensions.
  // MaybeShowDisabledDialog will be called once icon is loaded for every
  // extension.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      affected_extensions.size(),
      base::BindOnce(&Mv2DisabledDialogController::MaybeShowDisabledDialog,
                     weak_ptr_factory_.GetWeakPtr()));
  const int icon_size = affected_extensions.size() == 1
                            ? extension_misc::EXTENSION_ICON_SMALL
                            : extension_misc::EXTENSION_ICON_SMALLISH;
  auto* image_loader = ImageLoader::Get(browser_->profile());

  for (const Extension* extension : affected_extensions) {
    ExtensionResource icon = IconsInfo::GetIconResource(
        extension, icon_size, ExtensionIconSet::Match::kBigger);
    if (icon.empty()) {
      gfx::Image placeholder_icon =
          ExtensionIconPlaceholder::CreateImage(icon_size, extension->name());
      OnExtensionIconLoaded(extension->id(), extension->name(), barrier_closure,
                            placeholder_icon);
    } else {
      gfx::Size max_size(icon_size, icon_size);
      image_loader->LoadImageAsync(
          extension, icon, max_size,
          base::BindOnce(&Mv2DisabledDialogController::OnExtensionIconLoaded,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         extension->name(), barrier_closure));
    }
  }
}

void Mv2DisabledDialogController::OnExtensionIconLoaded(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    base::OnceClosure done_callback,
    const gfx::Image& icon) {
  ExtensionInfo extension_info;
  extension_info.id = extension_id;
  extension_info.name = extension_name;
  extension_info.icon = icon;

  affected_extensions_info_.push_back(extension_info);
  std::move(done_callback).Run();
}

void Mv2DisabledDialogController::MaybeShowDisabledDialog() {
  if (!browser_->window()) {
    return;
  }

  // Extensions can be updated while this call happens. Only include extensions
  // that are still affected.
  auto* extension_registry = ExtensionRegistry::Get(browser_->profile());
  auto* extension_prefs = ExtensionPrefs::Get(browser_->profile());
  const PrefMap& dialog_ack_pref =
      GetDisabledDialogAcknowledgedPref(experiment_stage_);

  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_->profile())->management_policy();
  affected_extensions_info_.erase(
      std::remove_if(
          affected_extensions_info_.begin(), affected_extensions_info_.end(),
          [&](const ExtensionInfo& extension_info) {
            const Extension* extension =
                extension_registry->disabled_extensions().GetByID(
                    extension_info.id);
            return !extension ||
                   !IsExtensionAffected(*extension, extension_prefs, policy,
                                        dialog_ack_pref);
          }),
      affected_extensions_info_.end());

  // No extensions to show, do nothing.
  if (affected_extensions_info_.empty()) {
    return;
  }

  // Sort extensions alphabetically.
  std::sort(affected_extensions_info_.begin(), affected_extensions_info_.end(),
            [](const ExtensionInfo& a, const ExtensionInfo& b) {
              return base::ToLowerASCII(a.name) < base::ToLowerASCII(b.name);
            });

  ShowMv2DeprecationDisabledDialog(
      browser_, affected_extensions_info_,
      base::BindOnce(&Mv2DisabledDialogController::OnRemoveSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&Mv2DisabledDialogController::OnManageSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&Mv2DisabledDialogController::UserAcknowledgedDialog,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Mv2DisabledDialogController::OnRemoveSelected() {
  CHECK(!affected_extensions_info_.empty());
  UserAcknowledgedDialog();

  auto* extension_service =
      ExtensionSystem::Get(browser_->profile())->extension_service();
  auto* extension_registry = ExtensionRegistry::Get(browser_->profile());

  for (const auto& extension_info : affected_extensions_info_) {
    const Extension* current_extension = extension_registry->GetExtensionById(
        extension_info.id, ExtensionRegistry::EVERYTHING);
    // Extensions can be uninstalled externally while the dialog is open. Only
    // uninstall extensions that are still existent.
    if (!current_extension) {
      continue;
    }

    // If an extension fails to be uninstalled, it will not pause the
    // uninstall of the other extensions on the list.
    extension_service->UninstallExtension(
        extension_info.id, UNINSTALL_REASON_USER_INITIATED, nullptr);
  }

  if (experiment_stage_ == MV2ExperimentStage::kDisableWithReEnable) {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Mv2Deprecation.Disabled.RemoveExtension.DisabledDialog"));
  } else {
    CHECK_EQ(experiment_stage_, MV2ExperimentStage::kUnsupported);
    base::RecordAction(
        base::UserMetricsAction("Extensions.Mv2Deprecation.Unsupported."
                                "RemoveExtension.DisabledDialog"));
  }
}

void Mv2DisabledDialogController::OnManageSelected() {
  CHECK(!affected_extensions_info_.empty());
  UserAcknowledgedDialog();
  chrome::ShowExtensions(browser_);
}

void Mv2DisabledDialogController::UserAcknowledgedDialog() {
  CHECK(!affected_extensions_info_.empty());

  // Store the extensions included in the dialog, so we don't show the dialog
  // for them again during the current experiment stage.
  auto* extension_prefs = ExtensionPrefs::Get(browser_->profile());
  const PrefMap& dialog_ack_pref =
      GetDisabledDialogAcknowledgedPref(experiment_stage_);
  for (const auto& extension_info : affected_extensions_info_) {
    extension_prefs->SetBooleanPref(extension_info.id, dialog_ack_pref, true);
  }

  if (experiment_stage_ == MV2ExperimentStage::kDisableWithReEnable) {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Mv2Deprecation.Disabled.ManageExtension.DisabledDialog"));
  } else {
    CHECK_EQ(experiment_stage_, MV2ExperimentStage::kUnsupported);
    base::RecordAction(
        base::UserMetricsAction("Extensions.Mv2Deprecation.Unsupported."
                                "ManageExtension.DisabledDialog"));
  }
}

}  // namespace extensions
