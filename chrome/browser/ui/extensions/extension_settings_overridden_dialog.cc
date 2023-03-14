// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_overrides/simple_overrides.h"
#include "chrome/browser/ui/ui_features.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace {

using extensions::ExtensionId;

constexpr char kShownExtensionDataKey[] = "shown_for_extensions";

// A series of helpers to record which profiles the dialog has already shown
// for (in this session only).
struct ShownExtensionSet : public base::SupportsUserData::Data {
  std::set<ExtensionId> shown_ids;
};

ShownExtensionSet* GetShownExtensionSet(Profile* profile,
                                        bool create_if_missing) {
  auto* shown_set = static_cast<ShownExtensionSet*>(
      profile->GetUserData(kShownExtensionDataKey));
  if (!shown_set && create_if_missing) {
    auto new_shown_set = std::make_unique<ShownExtensionSet>();
    shown_set = new_shown_set.get();
    profile->SetUserData(kShownExtensionDataKey, std::move(new_shown_set));
  }
  return shown_set;
}

bool HasShownFor(Profile* profile, const ExtensionId& id) {
  const ShownExtensionSet* shown_set = GetShownExtensionSet(profile, false);
  return shown_set && shown_set->shown_ids.count(id) > 0;
}

void MarkShownFor(Profile* profile, const ExtensionId& id) {
  ShownExtensionSet* shown_set = GetShownExtensionSet(profile, true);
  DCHECK(shown_set);
  bool inserted = shown_set->shown_ids.insert(id).second;
  DCHECK(inserted);
}

}  // namespace

ExtensionSettingsOverriddenDialog::Params::Params(
    extensions::ExtensionId controlling_extension_id,
    const char* extension_acknowledged_preference_name,
    const char* dialog_result_histogram_name,
    std::u16string dialog_title,
    std::u16string dialog_message,
    const gfx::VectorIcon* icon)
    : controlling_extension_id(std::move(controlling_extension_id)),
      extension_acknowledged_preference_name(
          extension_acknowledged_preference_name),
      dialog_result_histogram_name(dialog_result_histogram_name),
      dialog_title(std::move(dialog_title)),
      dialog_message(std::move(dialog_message)),
      icon(icon) {}
ExtensionSettingsOverriddenDialog::Params::~Params() = default;
ExtensionSettingsOverriddenDialog::Params::Params(Params&& params) = default;

ExtensionSettingsOverriddenDialog::ExtensionSettingsOverriddenDialog(
    Params params,
    Profile* profile)
    : params_(std::move(params)), profile_(profile) {
  DCHECK(!params_.controlling_extension_id.empty());
}

ExtensionSettingsOverriddenDialog::~ExtensionSettingsOverriddenDialog() =
    default;

bool ExtensionSettingsOverriddenDialog::ShouldShow() {
  if (params_.controlling_extension_id.empty())
    return false;

  if (HasShownFor(profile_, params_.controlling_extension_id))
    return false;

  if (HasAcknowledgedExtension(params_.controlling_extension_id))
    return false;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(params_.controlling_extension_id);
  DCHECK(extension);

  // Don't display the dialog for force-installed extensions that can't be
  // disabled.
  if (extensions::ExtensionSystem::Get(profile_)
          ->management_policy()
          ->MustRemainEnabled(extension, nullptr)) {
    return false;
  }

  // Don't show the extension if it's considered a "simple override" extension.
  if (base::FeatureList::IsEnabled(
          features::kLightweightExtensionOverrideConfirmations) &&
      simple_overrides::IsSimpleOverrideExtension(*extension)) {
    return false;
  }

  return true;
}

SettingsOverriddenDialogController::ShowParams
ExtensionSettingsOverriddenDialog::GetShowParams() {
  DCHECK(ShouldShow());

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(params_.controlling_extension_id);

  DCHECK(extension);

  return {params_.dialog_title, params_.dialog_message, params_.icon};
}

void ExtensionSettingsOverriddenDialog::OnDialogShown() {
  DCHECK(ShouldShow());

  MarkShownFor(profile_, params_.controlling_extension_id);
}

void ExtensionSettingsOverriddenDialog::HandleDialogResult(
    DialogResult result) {
  DCHECK(!params_.controlling_extension_id.empty());
  DCHECK(!HasAcknowledgedExtension(params_.controlling_extension_id));
  DCHECK(HasShownFor(profile_, params_.controlling_extension_id));

  // It's possible the extension was removed or disabled while the dialog was
  // being displayed. If this is the case, bail early.
  if (!extensions::ExtensionRegistry::Get(profile_)
           ->enabled_extensions()
           .Contains(params_.controlling_extension_id)) {
    return;
  }

  switch (result) {
    case DialogResult::kChangeSettingsBack:
      DisableControllingExtension();
      break;
    case DialogResult::kKeepNewSettings:
      AcknowledgeControllingExtension();
      break;
    case DialogResult::kDialogDismissed:
    case DialogResult::kDialogClosedWithoutUserAction:
      // Do nothing; the dialog will display on the next run of Chrome.
      break;
  }

  base::UmaHistogramEnumeration(params_.dialog_result_histogram_name, result);
}

void ExtensionSettingsOverriddenDialog::DisableControllingExtension() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->DisableExtension(params_.controlling_extension_id,
                            extensions::disable_reason::DISABLE_USER_ACTION);
}

void ExtensionSettingsOverriddenDialog::AcknowledgeControllingExtension() {
  extensions::ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
      params_.controlling_extension_id,
      params_.extension_acknowledged_preference_name, base::Value(true));
}

bool ExtensionSettingsOverriddenDialog::HasAcknowledgedExtension(
    const ExtensionId& id) {
  bool pref_state = false;
  return extensions::ExtensionPrefs::Get(profile_)->ReadPrefAsBoolean(
             id, params_.extension_acknowledged_preference_name, &pref_state) &&
         pref_state;
}
