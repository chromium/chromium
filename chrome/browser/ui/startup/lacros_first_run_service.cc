// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/lacros_first_run_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace {

// Overrides signals indicating that Sync is required, for testing purposes.
absl::optional<bool> g_sync_required_for_testing;

// Helper to run `callback` once refresh tokens from the
// `signin::IdentityManager` are loaded.
class RefreshTokensLoadObserver : public signin::IdentityManager::Observer {
 public:
  RefreshTokensLoadObserver(signin::IdentityManager* identity_manager,
                            base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
    DCHECK(!identity_manager->AreRefreshTokensLoaded());
    scoped_observation_.Observe(identity_manager);
  }

  void OnRefreshTokensLoaded() override {
    // We're using a `OnceCallback`, no need to continue observing.
    scoped_observation_.Reset();

    if (callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

// Silently goes through the `TurnSyncOnHelper` flow, enabling Sync when given
// the opportunity.
class SilentSyncEnablerDelegate : public TurnSyncOnHelper::Delegate {
 public:
  ~SilentSyncEnablerDelegate() override = default;

  // TurnSyncOnHelper::Delegate:
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override {
    // We proceed here, and we are waiting until `ShowSyncConfirmation()` for
    // the Sync engine to be started to know if we can proceed or not.
    // TODO(https://crbug.com/1324569): Introduce a `DEFER` status or a new
    // `ShouldShowEnterpriseAccountConfirmation()` delegate method to handle
    // management consent not being handled at this step.
    std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  }

  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {
    // The purpose of this delegate is specifically to enable Sync silently. If
    // we get all the way here, we assume that we can proceed with it.
    std::move(callback).Run(LoginUIService::SyncConfirmationUIClosedResult::
                                SYNC_WITH_DEFAULT_SETTINGS);

    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies);
  }

  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {
    // `SYNC_WITH_DEFAULT_SETTINGS` for the sync disable confirmation means
    // "stay signed in". See https://crbug.com/1141341.
    std::move(callback).Run(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies);
  }

  void ShowLoginError(const SigninUIError& error) override { NOTREACHED(); }

  void ShowMergeSyncDataConfirmation(const std::string&,
                                     const std::string&,
                                     signin::SigninChoiceCallback) override {
    NOTREACHED();
  }

  void ShowSyncSettings() override { NOTREACHED(); }

  void SwitchToProfile(Profile*) override { NOTREACHED(); }
};

bool IsFirstRunEligibleProfile(Profile* profile) {
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
  if (g_sync_required_for_testing.has_value())
    return g_sync_required_for_testing.value();

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
  std::move(original_intent_callback).Run(proceed);

  if (proceed && post_first_run_callback)
    std::move(post_first_run_callback).Run();
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

  // Skip for users without Gaia account (e.g. Active Directory, Kiosk, Guest…)
  if (!profiles::SessionHasGaiaAccount())
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

  // If the FRE is already open, no need to do any of the below. We need to
  // check this to avoid conflicts, but also because the FRE being open can make
  // some checks below inaccurate. For example, it turns Sync on while
  // configuring it, but might end up turning it off at the end of the flow.
  if (ProfilePicker::IsLacrosFirstRunOpen())
    return;

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing);
    SetFirstRunFinished();
    return;
  }

  if (!IsSyncRequired(profile_)) {
    // Let the FRE be shown when the user opens a browser UI for the first time.
    return;
  }

  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (identity_manager->HasAccountWithRefreshToken(account_id)) {
    TryEnableSyncSilentlyWithToken(account_id, scoped_closure_runner.Release());
  } else {
    if (token_load_observer_) {
      // An attempt to mark the FRE finish is already ongoing. We choose to
      // abort the current one and let the previous one continue.
      LOG(WARNING)
          << "Aborting slient sync opt-in attempt, another one is ongoing.";
      return;
    }

    // The observer will get cleared by `TryEnableSyncSilentlyWithToken()`.
    token_load_observer_ = std::make_unique<RefreshTokensLoadObserver>(
        identity_manager,
        base::BindOnce(
            &LacrosFirstRunService::TryEnableSyncSilentlyWithToken,
            // Unretained is safe because the observer will get destroyed before
            // this object and won't be able to trigger the callback.
            base::Unretained(this), account_id,
            scoped_closure_runner.Release()));
  }

  // At this point, Sync is either enabled, or can't be enabled at all for some
  // reason, so we should not reopen the FRE.
  SetFirstRunFinished();
}

void LacrosFirstRunService::TryEnableSyncSilentlyWithToken(
    const CoreAccountId& account_id,
    base::OnceClosure callback) {
  token_load_observer_.reset();

  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      !identity_manager->HasAccountWithRefreshToken(account_id)) {
    // Still no token, just give up.
    if (callback)
      std::move(callback).Run();
    return;
  }

  // TurnSyncOnHelper deletes itself once done.
  new TurnSyncOnHelper(
      profile_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kForcedSigninPrimaryAccount, account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<SilentSyncEnablerDelegate>(), std::move(callback));
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

  ProfilePicker::Show(ProfilePicker::Params::ForLacrosPrimaryProfileFirstRun(
      base::BindOnce(&OnFirstRunHasExited, std::move(callback))));
}

// LacrosFirstRunServiceFactory ------------------------------------------------

LacrosFirstRunServiceFactory::LacrosFirstRunServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LacrosFirstRunServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
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

namespace testing {

ScopedSyncRequiredInFirstRun::ScopedSyncRequiredInFirstRun(bool required) {
  if (g_sync_required_for_testing.has_value()) {
    overriden_value_ = g_sync_required_for_testing.value();
  }
  g_sync_required_for_testing = required;
}

ScopedSyncRequiredInFirstRun::~ScopedSyncRequiredInFirstRun() {
  if (overriden_value_.has_value()) {
    g_sync_required_for_testing = overriden_value_.value();
  } else {
    g_sync_required_for_testing.reset();
  }
}

}  // namespace testing
