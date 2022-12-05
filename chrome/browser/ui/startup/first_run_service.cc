// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/startup/silent_sync_enabler.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#endif

namespace {

bool IsFirstRunEligibleProfile(Profile* profile) {
  // Profile selections should exclude these already.
  DCHECK(!profile->IsOffTheRecord());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Skip for users without Gaia account (e.g. Active Directory, Kiosk, Guest…)
  if (!profiles::SessionHasGaiaAccount())
    return false;

  // The profile in Guest user sessions is considered "regular" but should
  // also be excluded here.
  if (profile->IsGuestSession())
    return false;

  // Having secondary profiles implies that the user already used Chrome and so
  // should not have to see the FRE. So we never want to run it for these.
  if (!profile->IsMainProfile())
    return false;
#else
  DCHECK(!profile->IsGuestSession());
#endif

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif

void SetFirstRunFinished() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kFirstRunFinished, true);
}

// Processes the outcome from the FRE and resumes the user's interrupted task.
// `original_intent_callback` should be run to allow the caller to resume what
// they were trying to do before they stopped to show the FRE. If the FRE's
// `status` is not `ProfilePicker::FirstRunExitStatus::kCompleted`, that
// `original_intent_callback` will be called with `proceed` set to false,
// otherwise it will be called with true.
void OnFirstRunHasExited(ResumeTaskCallback original_intent_callback,
                         ProfilePicker::FirstRunExitStatus status) {
  if (status != ProfilePicker::FirstRunExitStatus::kQuitEarly) {
    // The user got to the last step, we can mark the FRE as finished, whether
    // we eventually proceed with the original intent or not.
    SetFirstRunFinished();
  }

  bool proceed = status == ProfilePicker::FirstRunExitStatus::kCompleted;
  LOG_IF(ERROR, !proceed) << "Not proceeding FirstRun: "
                          << static_cast<int>(status);
  std::move(original_intent_callback).Run(proceed);
}

}  // namespace

// FirstRunService -------------------------------------------------------------

// static
void FirstRunService::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFirstRunFinished, false);
}

FirstRunService::FirstRunService(Profile* profile) : profile_(profile) {}
FirstRunService::~FirstRunService() = default;

bool FirstRunService::ShouldOpenFirstRun() const {
  DCHECK(IsFirstRunEligibleProfile(profile_));

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros we want to run the FRE beyond the strict first run as defined by
  // `IsChromeFirstRun()` for a few reasons:
  // - Migrated profiles will have their first run sentinel imported from the
  //   ash data dir, but we need to run the FRE in silent mode to re-enable sync
  //   on the Lacros primary profile.
  // - If the user exits the FRE without advancing beyond the first step, we
  //   need to show the FRE again next time they open Chrome, this is definitely
  //   not the "first run" anymore.
  if (!first_run::IsChromeFirstRun())
    return false;
#endif

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoFirstRun))
    return false;

  const PrefService* const pref_service = g_browser_process->local_state();
  return !pref_service->GetBoolean(prefs::kFirstRunFinished);
}

void FirstRunService::TryMarkFirstRunAlreadyFinished(
    base::OnceClosure callback) {
  DCHECK(ShouldOpenFirstRun());  // Caller should check.

  // The method has multiple exit points, this ensures `callback` gets called.
  base::ScopedClosureRunner scoped_closure_runner(std::move(callback));

  // If the FRE is already open, it is obviously not finished and we also don't
  // want to preemptively mark it completed. Skip all the below, the profile
  // picker can handle being called while already shown.
  if (ProfilePicker::IsFirstRunOpen())
    return;

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
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
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The FRE focuses on identity and offering the user to sign in. If the
    // profile already has an account (e.g. the sentinel file was deleted or
    // `--force-first-run` was passed) ensure we still skip it.
    SetFirstRunFinished();
    return;
  }
#endif

  // Fallthrough: let the FRE be shown when the user opens a browser UI for the
  // first time.
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void FirstRunService::StartSilentSync(base::OnceClosure callback) {
  // We should not be able to re-enter here as the FRE should be marked
  // already finished.
  DCHECK(!silent_sync_enabler_);

  auto reset_enabler_callback = base::BindOnce(
      &FirstRunService::ClearSilentSyncEnabler, weak_ptr_factory_.GetWeakPtr());
  silent_sync_enabler_ = std::make_unique<SilentSyncEnabler>(profile_);
  silent_sync_enabler_->StartAttempt(
      callback ? std::move(reset_enabler_callback).Then(std::move(callback))
               : std::move(reset_enabler_callback));
}

void FirstRunService::ClearSilentSyncEnabler() {
  silent_sync_enabler_.reset();
}
#endif

void FirstRunService::OpenFirstRunIfNeeded(EntryPoint entry_point,
                                           ResumeTaskCallback callback) {
  TryMarkFirstRunAlreadyFinished(base::BindOnce(
      &FirstRunService::OpenFirstRunInternal, weak_ptr_factory_.GetWeakPtr(),
      entry_point, std::move(callback)));
}

void FirstRunService::OpenFirstRunInternal(EntryPoint entry_point,
                                           ResumeTaskCallback callback) {
  if (!ShouldOpenFirstRun()) {
    // Opening the First Run is not needed, it might have been marked finished
    // silently for example.
    std::move(callback).Run(/*proceed=*/true);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::UmaHistogramEnumeration(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", entry_point);
#endif

  // Note: we call `Show()` even if the FRE might be already open and rely on
  // the ProfilePicker to decide what it wants to do with `callback`.
  ProfilePicker::Show(ProfilePicker::Params::ForFirstRun(
      profile_->GetPath(),
      base::BindOnce(&OnFirstRunHasExited, std::move(callback))));
}

// FirstRunServiceFactory ------------------------------------------------------

FirstRunServiceFactory::FirstRunServiceFactory()
    : ProfileKeyedServiceFactory(
          "FirstRunServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  // Used for checking Sync consent level.
  DependsOn(IdentityManagerFactory::GetInstance());
}

FirstRunServiceFactory::~FirstRunServiceFactory() = default;

// static
FirstRunServiceFactory* FirstRunServiceFactory::GetInstance() {
  static base::NoDestructor<FirstRunServiceFactory> factory;
  return factory.get();
}

// static
FirstRunService* FirstRunServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FirstRunService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* FirstRunServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // `ProfileSelections` exclude some profiles already, but they do not check
  // for some more specific conditions where we don't want to instantiate the
  // service.
  if (!IsFirstRunEligibleProfile(profile))
    return nullptr;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!base::FeatureList::IsEnabled(kForYouFre))
    return nullptr;
#endif

  auto* instance = new FirstRunService(profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Check if we should turn Sync on from the background and skip the FRE.
  // TODO(dgn): maybe post task? For example see
  // //chrome/browser/permissions/permission_auditing_service_factory.cc
  if (instance->ShouldOpenFirstRun()) {
    // If we don't manage to set it, we will just have to defer silent or visual
    // handling of the FRE to when the user attempts to open a browser UI. So
    // we don't need to do anything when the attempt finishes.
    instance->TryMarkFirstRunAlreadyFinished(base::OnceClosure());
  }
#endif

  return instance;
}

bool FirstRunServiceFactory::ServiceIsCreatedWithBrowserContext() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We want the service to be created early, even if the browser is created in
  // the background, so we can check whether we need to enable Sync silently.
  return true;
#else
  return false;
#endif
}

// Helpers ---------------------------------------------------------------------

bool ShouldOpenFirstRun(Profile* profile) {
  auto* instance = FirstRunServiceFactory::GetForBrowserContext(profile);
  return instance && instance->ShouldOpenFirstRun();
}
