// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

namespace {
bool IsFirstRunEligibleProfile(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // The parent guest and the profiles in a ChromeOS Guest session get through
  // the OTR check above.
  if (profile->IsGuestSession()) {
    return false;
  }

  return true;
}

bool IsFirstRunEligibleProcess() {
  if (!first_run::IsChromeFirstRun()) {
    return false;
  }

  // TODO(crbug.com/40232971): `IsChromeFirstRun()` should be a sufficient check
  // for Dice platforms. We currently keep this because some tests add
  // `--force-first-run` while keeping `--no-first-run`. We should updated the
  // affected tests to handle correctly the FRE opening instead of a tab.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoFirstRun);
}

enum class PolicyEffect {
  // The First Run experience can proceed unaffected.
  kNone,

  // The First Run experience should not run.
  kDisabled,
};

PolicyEffect ComputeDevicePolicyEffect(Profile& profile) {
  const PrefService* const local_state = g_browser_process->local_state();
  if (!local_state->GetBoolean(prefs::kPromotionsEnabled)) {
    // Corresponding policy: PromotionsEnabled=false
    return PolicyEffect::kDisabled;
  }

  if (!SyncServiceFactory::IsSyncAllowed(&profile)) {
    // Corresponding policy: SyncDisabled=true
    return PolicyEffect::kDisabled;
  }

  if (signin_util::IsForceSigninEnabled()) {
    // Corresponding policy: BrowserSignin=2
    // Debugging note: On Linux this policy is not supported and does not get
    // translated to the prefs (see crbug.com/956998), but we still respond to
    // `prefs::kForceBrowserSignin` being set (e.g. if manually edited).
    return PolicyEffect::kDisabled;
  }

  if (!profile.GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      !profile.GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup)) {
    // Corresponding policy: BrowserSignin=0
    return PolicyEffect::kDisabled;
  }

  return PolicyEffect::kNone;
}

void SetFirstRunFinished(FirstRunService::FinishedReason reason) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kFirstRunFinished, true);
  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.FinishReason", reason);
}

// Returns whether `prefs::kFirstRunFinished` is true. This implies that the FRE
// should not be opened again and would set if the user saw the FRE and is done
// with it, or if for some other reason (e.g. policy or some other browser
// state) we determine that we should not show it.
bool IsFirstRunMarkedFinishedInPrefs() {
  // Can be null in unit tests.
  const PrefService* const local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kFirstRunFinished);
}
}  // namespace

// FirstRunService -------------------------------------------------------------

// static
void FirstRunService::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFirstRunFinished, false);
}

FirstRunService::FirstRunService(Profile& profile,
                                 signin::IdentityManager& identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {}
FirstRunService::~FirstRunService() = default;

bool FirstRunService::ShouldOpenFirstRun() const {
  return ::ShouldOpenFirstRun(&profile_.get());
}

void FirstRunService::TryMarkFirstRunAlreadyFinished(
    base::OnceClosure callback) {
  DCHECK(ShouldOpenFirstRun());  // Caller should check.

  // The method has multiple exit points, this ensures `callback` gets called.
  base::ScopedClosureRunner scoped_closure_runner(std::move(callback));

  // If the FRE is already open, it is obviously not finished and we also don't
  // want to preemptively mark it completed. Skip all the below, the profile
  // picker can handle being called while already shown.
  if (ProfilePicker::IsFirstRunOpen()) {
    return;
  }

  auto policy_effect = ComputeDevicePolicyEffect(*profile_);
  // This check should be done prior to the profile already set up check below,
  // to include the case where the feature `kForceSigninFlowInProfilePicker` is
  // enabled which would cause the profile to be signed in already at this
  // point.
  if (policy_effect != PolicyEffect::kNone &&
      signin_util::IsForceSigninEnabled()) {
    // When ForceSignin is enabled and the flows are going through the profile
    // picker, the final profile setup should not yet be reached. The
    // rest of the flow is still happening within the Profile Picker, either
    // the management acceptance screen for Managed accounts, or the Sync
    // Confirmation screen for Consumer accounts.
    FinishFirstRun(FinishedReason::kForceSignin);
    return;
  }

  bool has_set_up_profile =
      // The Dice FRE focuses on identity and offering the user to sign in. If
      // the profile already has an account (e.g. the sentinel file was deleted
      // or `--force-first-run` was passed), this ensures we still skip it and
      // avoid having to handle too strange states later.
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  if (has_set_up_profile) {
    FinishFirstRun(FinishedReason::kProfileAlreadySetUp);
    return;
  }

  if (policy_effect != PolicyEffect::kNone) {
    FinishFirstRun(FinishedReason::kSkippedByPolicies);
    return;
  }

  // Fallthrough: let the FRE be shown when the user opens a browser UI for the
  // first time.
}

// `resume_task_callback_` should be run to allow the caller to resume what
// they were trying to do before they stopped to show the FRE.
// If the FRE's `status` is not `ProfilePicker::FirstRunExitStatus::kCompleted`,
// that `resume_task_callback_` will be called with `proceed` set to false,
// otherwise it will be called with true.
void FirstRunService::OnFirstRunHasExited(
    ProfilePicker::FirstRunExitStatus status) {
  if (!resume_task_callback_) {
    return;
  }

  bool proceed = false;
  bool should_mark_fre_finished = false;
  switch (status) {
    case ProfilePicker::FirstRunExitStatus::kCompleted:
      proceed = true;
      should_mark_fre_finished = true;
      break;
    case ProfilePicker::FirstRunExitStatus::kAbortTask:
      proceed = false;
      should_mark_fre_finished = false;
      break;
    case ProfilePicker::FirstRunExitStatus::kQuitAtEnd:
      proceed = true;
      should_mark_fre_finished = true;
      break;
    case ProfilePicker::FirstRunExitStatus::kAbandonedFlow:
      proceed = false;
      should_mark_fre_finished = true;
      break;
  }

  if (should_mark_fre_finished) {
    // The user got to the last step, we can mark the FRE as finished, whether
    // we eventually proceed with the original intent or not.
    FinishFirstRun(FinishedReason::kFinishedFlow);
  }

  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.ExitStatus", status);
  std::move(resume_task_callback_).Run(proceed);
}

void FirstRunService::FinishFirstRun(FinishedReason reason) {
  SetFirstRunFinished(reason);

  // If the reason is `FinishedReason::kForceSignin` the profile is already
  // signed in and finalized. It should not finish the setup again.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      reason != FinishedReason::kForceSignin) {
    // Noting that we expect that the name should already be available, as
    // after sign-in, the extended info is fetched and used for the sync
    // opt-in screen.
    profile_name_resolver_ = std::make_unique<ProfileNameResolver>(
        &identity_manager_.get(), identity_manager_->GetPrimaryAccountInfo(
                                      signin::ConsentLevel::kSignin));
    profile_name_resolver_->RunWithProfileName(base::BindOnce(
        &FirstRunService::FinishProfileSetUp, weak_ptr_factory_.GetWeakPtr()));
  } else if (reason == FinishedReason::kSkippedByPolicies) {
    // TODO(crbug.com/40256886): Try to get a domain name if available.
    FinishProfileSetUp(
        profiles::GetDefaultNameForNewEnterpriseProfile(std::string()));
  }
}

void FirstRunService::FinishProfileSetUp(std::u16string profile_name) {
  DCHECK(IsFirstRunMarkedFinishedInPrefs());

  profile_name_resolver_.reset();
  DCHECK(!profile_name.empty());
  FinalizeNewProfileSetup(&profile_.get(), profile_name,
                          /*is_default_name=*/false);
}

void FirstRunService::OpenFirstRunIfNeeded(ResumeTaskCallback callback) {
  OnFirstRunHasExited(ProfilePicker::FirstRunExitStatus::kAbortTask);
  resume_task_callback_ = std::move(callback);
  TryMarkFirstRunAlreadyFinished(base::BindOnce(
      &FirstRunService::OpenFirstRunInternal, weak_ptr_factory_.GetWeakPtr()));
}

void FirstRunService::OpenFirstRunInternal() {
  if (IsFirstRunMarkedFinishedInPrefs()) {
    // Opening the First Run is not needed. For example it might have been
    // marked finished silently, or is suppressed by policy.
    //
    // Note that this assumes that the prefs state is the the only part of
    // `ShouldOpenFirstRun()` that can change during the service's lifetime.
    std::move(resume_task_callback_).Run(/*proceed=*/true);
    return;
  }

  // Note: we call `Show()` even if the FRE might be already open and rely on
  // the ProfilePicker to decide what it wants to do with `callback`.
  ProfilePicker::Show(ProfilePicker::Params::ForFirstRun(
      profile_->GetPath(), base::BindOnce(&FirstRunService::OnFirstRunHasExited,
                                          weak_ptr_factory_.GetWeakPtr())));
}

void FirstRunService::FinishFirstRunWithoutResumeTask() {
  if (!resume_task_callback_) {
    return;
  }

  DCHECK(ProfilePicker::IsFirstRunOpen());
  OnFirstRunHasExited(ProfilePicker::FirstRunExitStatus::kAbandonedFlow);
  ProfilePicker::Hide();
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

// static
FirstRunService* FirstRunServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<FirstRunService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

std::unique_ptr<KeyedService>
FirstRunServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!ShouldOpenFirstRun(profile)) {
    return nullptr;
  }

  std::unique_ptr<FirstRunService> instance = std::make_unique<FirstRunService>(
      *profile, *IdentityManagerFactory::GetForProfile(profile));
  base::UmaHistogramBoolean("ProfilePicker.FirstRun.ServiceCreated", true);

  return instance;
}

// Helpers ---------------------------------------------------------------------

bool ShouldOpenFirstRun(Profile* profile) {
  return IsFirstRunEligibleProcess() && IsFirstRunEligibleProfile(profile) &&
         !IsFirstRunMarkedFinishedInPrefs();
}
