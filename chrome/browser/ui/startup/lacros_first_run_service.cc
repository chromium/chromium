// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/lacros_first_run_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/startup/silent_sync_enabler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

bool IsFirstRunEligibleProfile(Profile* profile) {
  // Skip for users without Gaia account (e.g. Active Directory, Kiosk, Guest…)
  if (!profiles::SessionHasGaiaAccount())
    return false;

  // Having secondary profiles implies that the user already used Chrome and so
  // should not have to see the FRE. So we never want to run it for these.
  if (!profile->IsMainProfile())
    return false;

  // Don't show the FRE if we are in a Guest user pod or in a Guest profile.
  if (profile->IsGuestSession())
    return false;

  if (profile->IsOffTheRecord())
    return false;

  return true;
}

// Whether policies and device settings require Sync to be always enabled.
bool IsSyncRequired(Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(prefs::kEnableSyncConsent))
    return true;

  crosapi::mojom::DeviceSettings* device_settings =
      g_browser_process->browser_policy_connector()->GetDeviceSettings();
  if (device_settings->device_ephemeral_users_enabled ==
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue)
    return true;

  return false;
}

void SetFirstRunFinished() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(lacros_prefs::kPrimaryProfileFirstRunFinished, true);
}

// Processes the outcome from the FRE and resumes the user's interrupted task.
// `original_intent_callback` should be run to allow the caller to resume what
// they were trying to do before they stopped to show the FRE. If the FRE's
// `status` is not `ProfilePicker::FirstRunExitStatus::kCompleted`, that
// `original_intent_callback` will be called with `proceed` set to false,
// otherwise it will be called with true. `post_first_run_callback` will be
// executed for completed flows, to perform tasks that the FRE requires after
// the interrupted task is resumed.
void OnFirstRunHasExited(ResumeTaskCallback original_intent_callback,
                         ProfilePicker::FirstRunExitStatus status,
                         base::OnceClosure post_first_run_callback) {
  if (status != ProfilePicker::FirstRunExitStatus::kQuitEarly) {
    // The user got to the last step, we can mark the FRE as finished, whether
    // we eventually proceed with the original intent or not.
    SetFirstRunFinished();
  }

  bool proceed = status == ProfilePicker::FirstRunExitStatus::kCompleted;
  LOG_IF(ERROR, !proceed) << "Not proceeding FirstRun: "
                          << static_cast<int>(status);
  std::move(original_intent_callback).Run(proceed);

  if (proceed) {
    DCHECK(post_first_run_callback);
    std::move(post_first_run_callback).Run();
  }
}

}  // namespace

// LacrosFirstRunService -------------------------------------------------------

LacrosFirstRunService::LacrosFirstRunService(Profile* profile)
    : profile_(profile) {}
LacrosFirstRunService::~LacrosFirstRunService() = default;

bool LacrosFirstRunService::ShouldOpenFirstRun() const {
  DCHECK(IsFirstRunEligibleProfile(profile_));

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoFirstRun))
    return false;

  const PrefService* const pref_service = g_browser_process->local_state();
  return !pref_service->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished);
}

void LacrosFirstRunService::TryMarkFirstRunAlreadyFinished(
    base::OnceClosure callback) {
  DCHECK(ShouldOpenFirstRun());  // Caller should check.

  // The method has multiple exit points, this ensures `callback` gets called.
  base::ScopedClosureRunner scoped_closure_runner(std::move(callback));

  // If the FRE is already open, it is obviously not finished and we also don't
  // want to preemptively mark it completed. Skip all the below, the profile
  // picker can handle being called while already shown.
  if (ProfilePicker::IsLacrosFirstRunOpen())
    return;

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing);
    SetFirstRunFinished();
    return;
  }

  if (IsSyncRequired(profile_)) {  // Enable Sync silently.
    // At this point, Sync is about to be enabled, or can't be enabled at
    // all for some reason. In any case, we should consider the FRE
    // triggering complete and ensure it doesn't open after this.
    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies);
    SetFirstRunFinished();

    StartSilentSync(scoped_closure_runner.Release());
    return;
  }

  // Fallthrough: let the FRE be shown when the user opens a browser UI for the
  // first time.
}

void LacrosFirstRunService::StartSilentSync(base::OnceClosure callback) {
  // We should not be able to re-enter here as the FRE should be marked
  // already finished.
  DCHECK(!silent_sync_enabler_);

  auto reset_enabler_callback =
      base::BindOnce(&LacrosFirstRunService::ClearSilentSyncEnabler,
                     weak_ptr_factory_.GetWeakPtr());
  silent_sync_enabler_ = std::make_unique<SilentSyncEnabler>(profile_);
  silent_sync_enabler_->StartAttempt(
      callback ? std::move(reset_enabler_callback).Then(std::move(callback))
               : std::move(reset_enabler_callback));
}

void LacrosFirstRunService::ClearSilentSyncEnabler() {
  silent_sync_enabler_.reset();
}

void LacrosFirstRunService::OpenFirstRunIfNeeded(EntryPoint entry_point,
                                                 ResumeTaskCallback callback) {
  TryMarkFirstRunAlreadyFinished(base::BindOnce(
      &LacrosFirstRunService::OpenFirstRunInternal,
      weak_ptr_factory_.GetWeakPtr(), entry_point, std::move(callback)));
}

void LacrosFirstRunService::OpenFirstRunInternal(EntryPoint entry_point,
                                                 ResumeTaskCallback callback) {
  if (!ShouldOpenFirstRun()) {
    // Opening the First Run is not needed, it might have been marked finished
    // silently for example.
    std::move(callback).Run(/*proceed=*/true);
    return;
  }

  base::UmaHistogramEnumeration(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", entry_point);

  // Note: we call `Show()` even if the FRE might be already open and rely on
  // the ProfilePicker to decide what it wants to do with `callback`.
  ProfilePicker::Show(ProfilePicker::Params::ForLacrosPrimaryProfileFirstRun(
      base::BindOnce(&OnFirstRunHasExited, std::move(callback))));
}

// LacrosFirstRunServiceFactory ------------------------------------------------

LacrosFirstRunServiceFactory::LacrosFirstRunServiceFactory()
    : ProfileKeyedServiceFactory(
          "LacrosFirstRunServiceFactory",
          // TODO(crbug.com/1375277): Update this instead of checking
          // the profile compatibility with `IsFirstRunEligibleProfile()`?
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  // Used for checking Sync consent level.
  DependsOn(IdentityManagerFactory::GetInstance());
}

LacrosFirstRunServiceFactory::~LacrosFirstRunServiceFactory() = default;

// static
LacrosFirstRunServiceFactory* LacrosFirstRunServiceFactory::GetInstance() {
  static base::NoDestructor<LacrosFirstRunServiceFactory> factory;
  return factory.get();
}

// static
LacrosFirstRunService* LacrosFirstRunServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LacrosFirstRunService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* LacrosFirstRunServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!IsFirstRunEligibleProfile(profile))
    return nullptr;

  auto* instance = new LacrosFirstRunService(profile);

  // Check if we should turn Sync on from the background and skip the FRE.
  // TODO(dgn): maybe post task? For example see
  // //chrome/browser/permissions/permission_auditing_service_factory.cc
  if (instance->ShouldOpenFirstRun()) {
    // If we don't manage to set it, we will just have to defer silent or visual
    // handling of the FRE to when the user attempts to open a browser UI. So
    // we don't need to do anything when the attempt finishes.
    instance->TryMarkFirstRunAlreadyFinished(base::OnceClosure());
  }

  return instance;
}

bool LacrosFirstRunServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Helpers ---------------------------------------------------------------------

bool ShouldOpenPrimaryProfileFirstRun(Profile* profile) {
  auto* instance = LacrosFirstRunServiceFactory::GetForBrowserContext(profile);
  return instance && instance->ShouldOpenFirstRun();
}
