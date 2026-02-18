// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_overrides/simple_overrides.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

namespace {

using extensions::ExtensionId;

constexpr char kShownExtensionDataKey[] = "shown_for_extensions";

// A series of helpers to record which profiles the dialog has already shown
// for (in this session only).
struct ShownExtensionSet : public base::SupportsUserData::Data {
  std::set<ExtensionId> shown_ids;
};

ShownExtensionSet* GetShownExtensionSet(Profile& profile,
                                        bool create_if_missing) {
  auto* shown_set = static_cast<ShownExtensionSet*>(
      profile.GetUserData(kShownExtensionDataKey));
  if (!shown_set && create_if_missing) {
    auto new_shown_set = std::make_unique<ShownExtensionSet>();
    shown_set = new_shown_set.get();
    profile.SetUserData(kShownExtensionDataKey, std::move(new_shown_set));
  }
  return shown_set;
}

void MarkShownFor(Profile& profile, const ExtensionId& id) {
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
    ShowParams show_params)
    : controlling_extension_id(std::move(controlling_extension_id)),
      extension_acknowledged_preference_name(
          extension_acknowledged_preference_name),
      dialog_result_histogram_name(dialog_result_histogram_name),
      content(std::move(show_params)) {}

ExtensionSettingsOverriddenDialog::Params::~Params() = default;
ExtensionSettingsOverriddenDialog::Params::Params(Params&& params) = default;

ExtensionSettingsOverriddenDialog::ExtensionSettingsOverriddenDialog(
    Params params,
    Profile& profile)
    : params_(std::move(params)), profile_(profile) {
  DCHECK(!params_.controlling_extension_id.empty());
}

ExtensionSettingsOverriddenDialog::~ExtensionSettingsOverriddenDialog() =
    default;

// static
void ExtensionSettingsOverriddenDialog::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(kSimpleOverrideBeginConfirmationTimestamp,
                             base::Time());
}

// static
bool ExtensionSettingsOverriddenDialog::HasShownFor(Profile& profile,
                                                    const ExtensionId& id) {
  const ShownExtensionSet* shown_set = GetShownExtensionSet(profile, false);
  return shown_set && shown_set->shown_ids.count(id) > 0;
}

// static
bool ExtensionSettingsOverriddenDialog::ShouldShowForSimpleOverrideExtension(
    Profile& profile,
    const extensions::Extension& extension) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineUnconditionalDialog)) {
    // If the feature is disabled, clear the timestamp. This ensures that if the
    // feature is re-enabled later, the grandfathering timestamp will be reset
    // to the time of re-enabling. Any extensions installed while the feature
    // was disabled will be grandfathered.
    PrefService* prefs = profile.GetPrefs();
    prefs->ClearPref(kSimpleOverrideBeginConfirmationTimestamp);
    return false;
  }

  PrefService* prefs = profile.GetPrefs();
  base::Time enforcement_time =
      prefs->GetTime(kSimpleOverrideBeginConfirmationTimestamp);

  // If the preference is not set, this is the first time the new logic is
  // running. Set the timestamp to Now.
  if (enforcement_time.is_null()) {
    enforcement_time = base::Time::Now();
    prefs->SetTime(kSimpleOverrideBeginConfirmationTimestamp, enforcement_time);
  }

  base::Time install_time = extensions::GetFirstInstallTime(
      extensions::ExtensionPrefs::Get(&profile), extension.id());

  // If the extension was installed after the enforcement logic began,
  // show the dialog.
  return install_time >= enforcement_time;
}

// static
bool ExtensionSettingsOverriddenDialog::HasAcknowledgedExtension(
    Profile& profile,
    const ExtensionId& id,
    const std::string& extension_acknowledged_preference_name) {
  bool pref_state = false;
  return extensions::ExtensionPrefs::Get(&profile)->ReadPrefAsBoolean(
             id, extension_acknowledged_preference_name, &pref_state) &&
         pref_state;
}

bool ExtensionSettingsOverriddenDialog::ShouldShow() {
  if (params_.controlling_extension_id.empty()) {
    return false;
  }

  if (HasShownFor(*profile_, params_.controlling_extension_id)) {
    return false;
  }

  if (HasAcknowledgedExtension(
          *profile_, params_.controlling_extension_id,
          params_.extension_acknowledged_preference_name)) {
    return false;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(base::to_address(profile_))
          ->enabled_extensions()
          .GetByID(params_.controlling_extension_id);
  DCHECK(extension);

  // Don't display the dialog for force-installed extensions that can't be
  // disabled.
  if (extensions::ExtensionSystem::Get(base::to_address(profile_))
          ->management_policy()
          ->MustRemainEnabled(extension, nullptr)) {
    return false;
  }

  // Historically, "Simple Overrides" were exempt from this dialog. We are
  // removing that exemption, but we grandfather in extensions installed
  // before the policy change was enabled to prevent spamming existing users.
  // See bug: https://crbug.com/463711704.
  if (simple_overrides::IsSimpleOverrideExtension(*extension)) {
    return ShouldShowForSimpleOverrideExtension(*profile_, *extension);
  }

  return true;
}

SettingsOverriddenDialogController::ShowParams
ExtensionSettingsOverriddenDialog::GetShowParams() {
  DCHECK(ShouldShow());

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(base::to_address(profile_))
          ->enabled_extensions()
          .GetByID(params_.controlling_extension_id);

  DCHECK(extension);

  return params_.content;
}

void ExtensionSettingsOverriddenDialog::OnDialogShown() {
  DCHECK(ShouldShow());

  MarkShownFor(*profile_, params_.controlling_extension_id);
}

void ExtensionSettingsOverriddenDialog::HandleDialogResult(
    DialogResult result) {
  DCHECK(!params_.controlling_extension_id.empty());
  DCHECK(!HasAcknowledgedExtension(
      *profile_, params_.controlling_extension_id,
      params_.extension_acknowledged_preference_name));
  DCHECK(HasShownFor(*profile_, params_.controlling_extension_id));

  // It's possible the extension was removed or disabled while the dialog was
  // being displayed. If this is the case, bail early.
  if (!extensions::ExtensionRegistry::Get(base::to_address(profile_))
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

  if (dialog_result_callback_) {
    CHECK(base::FeatureList::IsEnabled(
        extensions_features::kSearchEngineExplicitChoiceDialog));
    std::move(dialog_result_callback_).Run(result);
  }

  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopSEHijacking) &&
      !base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        base::to_address(profile_), /*create_if_necessary=*/true);
    if (hats_service) {
      hats_service->LaunchDelayedSurvey(kHatsSurveyTriggerSEHijacking, 5000);
    }
  }
}

void ExtensionSettingsOverriddenDialog::SetDialogResultCallback(
    DialogResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kSearchEngineExplicitChoiceDialog));
  dialog_result_callback_ = std::move(callback);
}

void ExtensionSettingsOverriddenDialog::DisableControllingExtension() {
  extensions::ExtensionRegistrar::Get(base::to_address(profile_))
      ->DisableExtension(params_.controlling_extension_id,
                         {extensions::disable_reason::DISABLE_USER_ACTION});
}

void ExtensionSettingsOverriddenDialog::AcknowledgeControllingExtension() {
  extensions::ExtensionPrefs::Get(base::to_address(profile_))
      ->UpdateExtensionPref(params_.controlling_extension_id,
                            params_.extension_acknowledged_preference_name,
                            base::Value(true));
}
