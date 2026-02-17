// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/sequence_checker_impl.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/account_state_fetcher.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_id.h"

namespace {
constexpr char kHistorySyncOptIntAccessPointHistogramPrefix[] =
    "Signin.HistorySyncOptIn.";
constexpr char kHistorySyncOptIntAccessPointActionPrefix[] =
    "Signin_HistorySync_";
constexpr char kOtherManagedProfileCreationHistogramName[] =
    "Signin.ManagedUserProfileCreationConflict";
constexpr char kSyncServiceStartupAwaitCompleteHistogramName[] =
    "Signin.HistorySyncOptin.SyncStartupAwaitTime.Complete";
constexpr char kSyncServiceStartupAwaitTimeoutHistogramName[] =
    "Signin.HistorySyncOptin.SyncStartupAwaitTime.Timeout";

base::TimeDelta GetElapsedTime(const base::OneShotTimer& timer) {
  CHECK(timer.IsRunning());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start_time =
      timer.desired_run_time() - timer.GetCurrentDelay();
  return now - start_time;
}

// LINT.IfChange(FlowEventToString)
std::string_view GetHistorySyncSkipReasonMetricName(
    HistorySyncOptinHelper::HistorySyncSkipReason skip_reason) {
  switch (skip_reason) {
    case HistorySyncOptinHelper::HistorySyncSkipReason::kAlreadyOptedIn:
      return "AlreadyOptedIn";
    case HistorySyncOptinHelper::HistorySyncSkipReason::kUserNotSignedIn:
    case HistorySyncOptinHelper::HistorySyncSkipReason::kSyncForbidden:
    case HistorySyncOptinHelper::HistorySyncSkipReason::kManagementRejected:
      return "Skipped";
    case HistorySyncOptinHelper::HistorySyncSkipReason::
        kManagementProfileCreationConflict:
      // In this situation the entire flow is aborted.
      return "Aborted";
    case HistorySyncOptinHelper::HistorySyncSkipReason::
        kResumeFlowInNewManagedProfile:
      // The flow is resumed on a new profile, no metrics need to be recorded.
      NOTREACHED();
  }
  NOTREACHED();
}

std::string_view UserChoiceToStringMetric(
    HistorySyncOptinHelper::ScreenChoiceResult user_choice) {
  switch (user_choice) {
    case HistorySyncOptinHelper::ScreenChoiceResult::kAccepted:
      return "Completed";
    case HistorySyncOptinHelper::ScreenChoiceResult::kDeclined:
      return "Declined";
    case HistorySyncOptinHelper::ScreenChoiceResult::kDismissed:
      return "Aborted";
    case HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped:
      // When the screen is skipped the metrics are recorded via a different
      // method.
      NOTREACHED();
  }
  NOTREACHED();
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/signin/histograms.xml:Signin.HistorySyncOptIn)

void RecordSyncServiceStartupCompletionMetrics(base::TimeDelta elapsed_time) {
  base::UmaHistogramTimes(kSyncServiceStartupAwaitCompleteHistogramName,
                          elapsed_time);
}

void RecordSyncServiceStartupTimeoutMetrics(base::TimeDelta elapsed_time) {
  base::UmaHistogramTimes(kSyncServiceStartupAwaitTimeoutHistogramName,
                          elapsed_time);
}

void RecordMetricsForHistorySyncUserChoice(
    HistorySyncOptinHelper::ScreenChoiceResult user_choice,
    Profile* profile,
    signin_metrics::AccessPoint access_point) {
  auto user_choice_str = UserChoiceToStringMetric(user_choice);
  auto histogram_name = base::StrCat(
      {kHistorySyncOptIntAccessPointHistogramPrefix, user_choice_str});
  auto action_name = base::StrCat(
      {kHistorySyncOptIntAccessPointActionPrefix, user_choice_str});

  base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  base::UmaHistogramEnumeration(histogram_name, access_point);

  // Record successfully enabling history sync when originating from the
  // AvatarPill promo.
  if (user_choice == HistorySyncOptinHelper::ScreenChoiceResult::kAccepted &&
      access_point == signin_metrics::AccessPoint::
                          kHistorySyncOptinExpansionPillOnStartup) {
    signin::RecordAvatarButtonPromoAcceptedAtPromoShownCount(
        signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
        IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs());
  }
}

void RecordMetricsForSkippedHistoryScreen(
    HistorySyncOptinHelper::HistorySyncSkipReason skip_reason,
    signin_metrics::AccessPoint access_point) {
  auto skip_reason_str = GetHistorySyncSkipReasonMetricName(skip_reason);
  auto histogram_name = base::StrCat(
      {kHistorySyncOptIntAccessPointHistogramPrefix, skip_reason_str});
  auto action_name = base::StrCat(
      {kHistorySyncOptIntAccessPointActionPrefix, skip_reason_str});

  base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  base::UmaHistogramEnumeration(histogram_name, access_point);
}

bool AccountMayHaveCloudPolicies(Profile* profile, const std::string& email) {
  return signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
             email) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}

syncer::SyncService* GetSyncService(Profile* profile) {
  return SyncServiceFactory::IsSyncAllowed(profile)
             ? SyncServiceFactory::GetForProfile(profile)
             : nullptr;
}

bool IsSyncStartupInPendingState(syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }
  auto transport_state = sync_service->GetTransportState();
  switch (transport_state) {
    case syncer::SyncService::TransportState::DISABLED:
    case syncer::SyncService::TransportState::PAUSED:
      return false;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
      return true;
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      return false;
  }
  NOTREACHED();
}

// An implementation of `SyncServiceStartupStateObserver` directly
// observing the Sync Service and tracking if its transport state
// has reached a final state.
class SyncServiceStartupStateObserverImpl
    : public SyncServiceStartupStateObserver,
      public syncer::SyncServiceObserver {
 public:
  SyncServiceStartupStateObserverImpl(
      syncer::SyncService* sync_service,
      base::TimeDelta startup_delay,
      base::OnceClosure on_state_updated_callback);
  ~SyncServiceStartupStateObserverImpl() override;

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const CoreAccountInfo& account_info,
      base::TimeDelta startup_delay,
      base::OnceClosure callback);

  // SyncServiceStartupStateObserver implementation:
  void MockTimeoutReachedForTesting() override;  // IN-TEST
  void OnSyncStartupStateChangedForTesting(      // IN-TEST
      SyncStartupTracker::ServiceStartupState state) override;

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  void OnSyncServiceStartupTimeout();

  base::OnceClosure on_state_updated_callback_;
  base::OneShotTimer sync_service_startup_timeout_timer_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::WeakPtrFactory<SyncServiceStartupStateObserverImpl>
      weak_pointer_factory_{this};
};

SyncServiceStartupStateObserverImpl::SyncServiceStartupStateObserverImpl(
    syncer::SyncService* sync_service,
    base::TimeDelta startup_delay,
    base::OnceClosure on_state_updated_callback)
    : on_state_updated_callback_(std::move(on_state_updated_callback)) {
  CHECK(sync_service);
  CHECK(on_state_updated_callback_);
  // Start a timeout for sync service to update its state.
  sync_service_startup_timeout_timer_.Start(
      FROM_HERE, startup_delay,
      base::BindOnce(
          &SyncServiceStartupStateObserverImpl::OnSyncServiceStartupTimeout,
          weak_pointer_factory_.GetWeakPtr()));
  sync_service_observation_.Observe(sync_service);
}

SyncServiceStartupStateObserverImpl::~SyncServiceStartupStateObserverImpl() =
    default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateObserverImpl::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::TimeDelta startup_delay,
        base::OnceClosure callback) {
  if (AccountMayHaveCloudPolicies(profile, account_info.email) &&
      IsSyncStartupInPendingState(sync_service)) {
    // The service is still starting up, wait for it to become active or
    // disabled.
    return std::make_unique<SyncServiceStartupStateObserverImpl>(
        sync_service, startup_delay, std::move(callback));
  }
  return nullptr;
}

void SyncServiceStartupStateObserverImpl::MockTimeoutReachedForTesting() {
  sync_service_startup_timeout_timer_.FireNow();
}

void SyncServiceStartupStateObserverImpl::
    OnSyncStartupStateChangedForTesting(  // IN-TEST
        SyncStartupTracker::ServiceStartupState state) {
  // Tests using this implementation should set the transport state of the sync
  // service directly.
  NOTREACHED();
}

void SyncServiceStartupStateObserverImpl::OnStateChanged(
    syncer::SyncService* sync) {
  if (!IsSyncStartupInPendingState(sync)) {
    // The sync service has finished starting up, so we can stop observing.
    if (sync_startup_complete_metrics_callback_) {
      std::move(sync_startup_complete_metrics_callback_)
          .Run(GetElapsedTime(sync_service_startup_timeout_timer_));
    }
    sync_service_startup_timeout_timer_.Stop();
    sync_service_observation_.Reset();
    std::move(on_state_updated_callback_).Run();
  }
}

void SyncServiceStartupStateObserverImpl::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_startup_timeout_timer_.Stop();
  sync_service_observation_.Reset();
}

void SyncServiceStartupStateObserverImpl::OnSyncServiceStartupTimeout() {
  if (timeout_metrics_callback_) {
    std::move(timeout_metrics_callback_)
        .Run(sync_service_startup_timeout_timer_.GetCurrentDelay());
  }
  sync_service_startup_timeout_timer_.Stop();
  sync_service_observation_.Reset();
  CHECK(!on_state_updated_callback_.is_null());
  std::move(on_state_updated_callback_).Run();
}

// An implementation of `SyncServiceStartupStateObserver` based on the
// soon to be deprecated `SyncStartupTracker`.
// TODO(crbug.com/40067025): Delete this implementation once the
// TurnSyncOnHelper is deprecated. `HistorySyncOptinHelper` should use the
// `SyncServiceStartupStateObserver` (currently gated by a feature flag).
class SyncServiceStartupStateLegacyObserverImpl
    : public SyncServiceStartupStateObserver {
 public:
  SyncServiceStartupStateLegacyObserverImpl(
      syncer::SyncService* sync_service,
      base::OnceClosure on_state_updated_callback);
  ~SyncServiceStartupStateLegacyObserverImpl() override;

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const CoreAccountInfo& account_info,
      base::OnceClosure callback);
  // SyncServiceStartupStateObserver implementation:
  void MockTimeoutReachedForTesting() override;  // IN-TEST
  void OnSyncStartupStateChangedForTesting(
      SyncStartupTracker::ServiceStartupState state) override;  // IN-TEST

 private:
  void OnSyncStartupStateChanged(SyncStartupTracker::ServiceStartupState state);

  base::OnceClosure on_state_updated_callback_;
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;
  base::WeakPtrFactory<SyncServiceStartupStateLegacyObserverImpl>
      weak_pointer_factory_{this};
};

SyncServiceStartupStateLegacyObserverImpl::
    SyncServiceStartupStateLegacyObserverImpl(
        syncer::SyncService* sync_service,
        base::OnceClosure on_state_updated_callback)
    : on_state_updated_callback_(std::move(on_state_updated_callback)) {
  CHECK(sync_service);
  CHECK(on_state_updated_callback_);

  sync_startup_tracker_ = std::make_unique<SyncStartupTracker>(
      sync_service,
      base::BindOnce(
          &SyncServiceStartupStateLegacyObserverImpl::OnSyncStartupStateChanged,
          weak_pointer_factory_.GetWeakPtr()));
  return;
}

SyncServiceStartupStateLegacyObserverImpl::
    ~SyncServiceStartupStateLegacyObserverImpl() = default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateLegacyObserverImpl::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::OnceClosure callback) {
  if (AccountMayHaveCloudPolicies(profile, account_info.email) &&
      SyncStartupTracker::GetServiceStartupState(sync_service) ==
          SyncStartupTracker::ServiceStartupState::kPending) {
    return std::make_unique<SyncServiceStartupStateLegacyObserverImpl>(
        sync_service, std::move(callback));
  }
  return nullptr;
}

void SyncServiceStartupStateLegacyObserverImpl::OnSyncStartupStateChanged(
    SyncStartupTracker::ServiceStartupState state) {
  switch (state) {
    case SyncStartupTracker::ServiceStartupState::kPending:
      NOTREACHED();
    case SyncStartupTracker::ServiceStartupState::kTimeout:
      [[fallthrough]];
    case SyncStartupTracker::ServiceStartupState::kError:
    case SyncStartupTracker::ServiceStartupState::kComplete:
      std::move(on_state_updated_callback_).Run();
  }
}

void SyncServiceStartupStateLegacyObserverImpl::MockTimeoutReachedForTesting() {
  // Tests using this implementation can make use of
  // `testing::ScopedSyncStartupTimeoutOverride`.
  NOTREACHED();
}

void SyncServiceStartupStateLegacyObserverImpl::
    OnSyncStartupStateChangedForTesting(
        SyncStartupTracker::ServiceStartupState state) {
  OnSyncStartupStateChanged(state);
}
}  // namespace

BASE_FEATURE(kEnableAwaitSyncServiceStartupOnHistorySync,
             base::FEATURE_ENABLED_BY_DEFAULT);

const int kAwaitSyncServiceStartupInProfilePickerTimeoutDefaultValue = 10;
const base::FeatureParam<int>
    kAwaitSyncServiceStartupInProfilePickerTimeoutSeconds{
        &kEnableAwaitSyncServiceStartupOnHistorySync,
        /*name=*/"AwaitSyncServiceStartupInProfilePickerTimeoutSeconds",
        kAwaitSyncServiceStartupInProfilePickerTimeoutDefaultValue};

const int kAwaitSyncServiceStartupInBrowserTimeoutDefaultValue = 3;
const base::FeatureParam<int> kAwaitSyncServiceStartupInBrowserTimeoutSeconds{
    &kEnableAwaitSyncServiceStartupOnHistorySync,
    /*name=*/"AwaitSyncServiceStartupInBrowserTimeoutSeconds",
    kAwaitSyncServiceStartupInBrowserTimeoutDefaultValue};

SyncServiceStartupStateObserver::SyncServiceStartupStateObserver() = default;
SyncServiceStartupStateObserver::~SyncServiceStartupStateObserver() = default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateObserver::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::TimeDelta startup_delay,
        base::OnceClosure callback) {
  if (base::FeatureList::IsEnabled(
          kEnableAwaitSyncServiceStartupOnHistorySync) &&
      base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return SyncServiceStartupStateObserverImpl::
        MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
            sync_service, profile, account_info, startup_delay,
            std::move(callback));
  }
  return SyncServiceStartupStateLegacyObserverImpl::
      MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
          sync_service, profile, account_info, std::move(callback));
}

void SyncServiceStartupStateObserver::SetSyncStartupCompleteMetricsCallback(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  CHECK(callback);
  sync_startup_complete_metrics_callback_ = std::move(callback);
}

void SyncServiceStartupStateObserver::SetTimeoutMetricsCallback(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  CHECK(callback);
  timeout_metrics_callback_ = std::move(callback);
}

HistorySyncOptinPolicyHelper::HistorySyncOptinPolicyHelper(
    Profile* profile,
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> on_register_for_policies_callback,
    base::OnceClosure on_policies_fetched_callback)
    : profile_(profile),
      account_info_(account_info),
      on_register_for_policies_callback_(
          std::move(on_register_for_policies_callback)),
      on_policies_fetched_callback_(std::move(on_policies_fetched_callback)) {
  CHECK(!on_register_for_policies_callback_.is_null());
  CHECK(!on_policies_fetched_callback_.is_null());
}

HistorySyncOptinPolicyHelper::~HistorySyncOptinPolicyHelper() = default;

void HistorySyncOptinPolicyHelper::RegisterForPolicies() {
  CHECK(!policy_fetch_tracker_);
  policy_fetch_tracker_ = TurnSyncOnHelperPolicyFetchTracker::CreateInstance(
      profile_, account_info_);
  policy_fetch_tracker_->RegisterForPolicy(
      std::move(on_register_for_policies_callback_),
      /*is_registration_for_management_consistency_check=*/false);
}

void HistorySyncOptinPolicyHelper::FetchPolicies() {
  CHECK(policy_fetch_tracker_);
  CHECK(!on_policies_fetched_callback_.is_null());
  bool fetch_started = policy_fetch_tracker_->FetchPolicy(
      std::move(on_policies_fetched_callback_));
  CHECK(fetch_started);
}

// static
std::unique_ptr<HistorySyncOptinHelper> HistorySyncOptinHelper::Create(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    LaunchContext launch_context,
    signin_metrics::AccessPoint access_point) {
  switch (launch_context) {
    case LaunchContext::kInBrowser:
      return std::make_unique<HistorySyncOptinHelperInBrowser>(
          identity_manager, profile, account_info, delegate, access_point);
    case LaunchContext::kInProfilePicker:
      return std::make_unique<HistorySyncOptinHelperInProfilePicker>(
          identity_manager, profile, account_info, delegate, access_point);
  }
}

HistorySyncOptinHelper::HistorySyncOptinHelper(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    signin_metrics::AccessPoint access_point)
    : profile_(profile),
      account_info_(account_info),
      delegate_(delegate),
      account_state_fetcher_(std::make_unique<AccountStateFetcher>(
          identity_manager,
          account_info,
          /*get_account_state_callback=*/
          base::BindRepeating(&HistorySyncOptinHelper::AccountIsManaged,
                              base::Unretained(this)),
          /*on_account_info_fetched_callback=*/
          base::BindOnce(
              &HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow,
              base::Unretained(this)))),
      access_point_(access_point) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));
  CHECK(delegate);
}

HistorySyncOptinHelper::~HistorySyncOptinHelper() = default;

void HistorySyncOptinHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HistorySyncOptinHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HistorySyncOptinHelper::NotifyFlowFinishedWithHistorySyncScreenAttempted(
    ScreenChoiceResult user_choice) {
  CHECK(!is_history_sync_step_complete_);
  is_history_sync_step_complete_ = true;

  if (user_choice != ScreenChoiceResult::kScreenSkipped) {
    RecordMetricsForHistorySyncUserChoice(user_choice, profile_,
                                          access_point());
  }

  for (Observer& observer : observers_) {
    observer.OnHistorySyncOptinHelperFlowFinished();
  }
}

void HistorySyncOptinHelper::NotifyFlowFinishedWithHistorySyncScreenSkipped(
    HistorySyncSkipReason skip_reason) {
  CHECK(!is_history_sync_step_complete_);
  is_history_sync_step_complete_ = true;
  if (skip_reason != HistorySyncSkipReason::kResumeFlowInNewManagedProfile) {
    RecordMetricsForSkippedHistoryScreen(skip_reason, access_point());
  }
  // Observer notification must be the last step, as the observer
  // may delete this helper (see HistorySyncOptinService).
  for (Observer& observer : observers_) {
    observer.OnHistorySyncOptinHelperFlowFinished();
  }
}

void HistorySyncOptinHelper::StartHistorySyncOptinFlow() {
  account_state_fetcher_->FetchAccountInfo();
}

void HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow(
    signin::Tribool maybe_managed_account) {
  if (maybe_managed_account == signin::Tribool::kTrue) {
    maybe_managed_account_ = maybe_managed_account;

    if (management_status_state_ !=
        ManagementStatusState::kManagementDisclaimerComplete) {
      // Updates the `management_status_state_`.
      DetermineManagementStatusAndShowManagementScreens();
      switch (management_status_state_) {
        case ManagementStatusState::kManagementDisclaimerInProgress:
        case ManagementStatusState::kManagementDisclaimerComplete:
          // kManagementDisclaimerInProgress: The management flow is
          // responsible for progressing an ongoing flow
          // kManagementDisclaimerComplete: We expect to follow up call to
          // ResumeShowHistorySyncOptinScreenFlow with the state marked
          // complete.
          return;
        case ManagementStatusState::kFlowAborted:
          NotifyFlowFinishedWithHistorySyncScreenSkipped(
              HistorySyncSkipReason::kManagementProfileCreationConflict);
          return;
        case ManagementStatusState::kManagementDisclaimerNotStarted:
          // `DetermineManagementStatusAndShowManagementScreens` has updated the
          // managed disclaimer's state.
          NOTREACHED();
      }
    }
  }

  AwaitSyncStartupAndShowHistorySyncScreen();
}

void HistorySyncOptinHelper::AwaitSyncStartupAndShowHistorySyncScreen() {
  // For managed users the polices are fetched when the user accepts
  // management, which is done as part of
  // `DetermineManagementStatusAndShowManagementScreens`. We are ready to get
  // the sync service's startup state.
  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (sync_service) {
    sync_startup_state_observer_ = SyncServiceStartupStateObserver::
        MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
            sync_service, profile_, account_info_, GetSyncStartupDelay(),
            base::BindOnce(&HistorySyncOptinHelper::ShowHistorySyncOptinScreen,
                           weak_ptr_factory_.GetWeakPtr()));
    if (sync_startup_state_observer_) {
      sync_startup_state_observer_->SetSyncStartupCompleteMetricsCallback(
          base::BindOnce(&RecordSyncServiceStartupCompletionMetrics));
      sync_startup_state_observer_->SetTimeoutMetricsCallback(
          base::BindOnce(&RecordSyncServiceStartupTimeoutMetrics));
      return;
    }
  }
  ShowHistorySyncOptinScreen();
}

void HistorySyncOptinHelper::ShowHistorySyncOptinScreen() {
  CHECK(!is_history_sync_screen_attempted_);
  is_history_sync_screen_attempted_ = true;
  signin_util::ShouldShowHistorySyncOptinResult result =
      signin_util::ShouldShowHistorySyncOptinScreen(*profile_.get());

  if (result == signin_util::ShouldShowHistorySyncOptinResult::kShow) {
    base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Started"));
    base::UmaHistogramEnumeration("Signin.HistorySyncOptIn.Started",
                                  access_point());

    delegate_->ShowHistorySyncOptinScreen(
        profile(), FlowCompletedCallback(base::BindOnce(
                       &HistorySyncOptinHelper::
                           NotifyFlowFinishedWithHistorySyncScreenAttempted,
                       weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  // Sanity checks that we are not in an
  // access point that should not offer the history sync screen.
  CHECK(signin_util::IsValidAccessPointForHistoryOptinScreen(access_point()));

  // Currently this class is used to for entry points meant to
  // display the history sync optin screen (i.e. enabling history
  // sync is optional).
  // If sync is disabled just skip the screen.
  HistorySyncSkipReason skip_reason;
  switch (result) {
    case signin_util::ShouldShowHistorySyncOptinResult::kShow:
      NOTREACHED();
    case signin_util::ShouldShowHistorySyncOptinResult::kSkipUserNotSignedIn:
      skip_reason = HistorySyncSkipReason::kUserNotSignedIn;
      break;
    case signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden:
      skip_reason = HistorySyncSkipReason::kSyncForbidden;
      break;
    case signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn:
      skip_reason = HistorySyncSkipReason::kAlreadyOptedIn;
      break;
  }
  FinishFlowWithoutHistorySyncOptin(skip_reason);
}

signin::Tribool HistorySyncOptinHelper::AccountIsManaged(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.IsManaged();
  }
  return signin::Tribool::kUnknown;
}

void HistorySyncOptinHelper::FinishFlowWithoutHistorySyncOptin(
    HistorySyncSkipReason skip_reason) {
  delegate_->FinishFlowWithoutHistorySyncOptin();
  NotifyFlowFinishedWithHistorySyncScreenSkipped(skip_reason);
}

// HistorySyncOptinHelperInBrowser
HistorySyncOptinHelperInBrowser::HistorySyncOptinHelperInBrowser(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    signin_metrics::AccessPoint access_point)
    : HistorySyncOptinHelper(identity_manager,
                             profile,
                             account_info,
                             delegate,
                             access_point) {}

HistorySyncOptinHelperInBrowser::~HistorySyncOptinHelperInBrowser() = default;

void HistorySyncOptinHelperInBrowser::
    DetermineManagementStatusAndShowManagementScreens() {
  if (management_status_state_ !=
      HistorySyncOptinHelper::ManagementStatusState::
          kManagementDisclaimerNotStarted) {
    return;
  }
  CHECK(management_status_state_ ==
        HistorySyncOptinHelper::ManagementStatusState::
            kManagementDisclaimerNotStarted);
  management_status_state_ = HistorySyncOptinHelper::ManagementStatusState::
      kManagementDisclaimerInProgress;

  profile_management_disclaimer_service_ =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(profile());
  CHECK(profile_management_disclaimer_service_);
  base::OnceCallback<void(Profile*, bool)>
      profile_management_accepted_callback =
          base::BindOnce(&HistorySyncOptinHelperInBrowser::OnManagementAccepted,
                         weak_ptr_factory_.GetWeakPtr());

  const CoreAccountId account_id_of_ongoing_management_flow =
      profile_management_disclaimer_service_
          ->GetAccountBeingConsideredForManagementIfAny();
  // Abort the flow is we are in the process of creating a managed profile
  // for another user.
  bool has_managed_profile_created_conflict =
      !account_id_of_ongoing_management_flow.empty() &&
      account_id_of_ongoing_management_flow != account_info().account_id;
  base::UmaHistogramBoolean(kOtherManagedProfileCreationHistogramName,
                            has_managed_profile_created_conflict);
  if (has_managed_profile_created_conflict) {
    management_status_state_ =
        HistorySyncOptinHelper::ManagementStatusState::kFlowAborted;
    return;
  }
  profile_management_disclaimer_service_->EnsureManagedProfileForAccount(
      account_info().account_id, access_point(),
      std::move(profile_management_accepted_callback));
}

base::TimeDelta HistorySyncOptinHelperInBrowser::GetSyncStartupDelay() {
  return base::Seconds(kAwaitSyncServiceStartupInBrowserTimeoutSeconds.Get());
}

void HistorySyncOptinHelperInBrowser::OnManagementAccepted(
    Profile* chosen_profile,
    bool) {
  // `chosen_profile` is null can mean:
  // 1) the user rejects management, or
  // 2) the flow was aborted, or
  // 3) user is not managed. This cases does not apply to the
  // HistorySyncOptinHelperInBrowser since we only reach this method
  // if the user is managed.
  CHECK_EQ(maybe_managed_account(), signin::Tribool::kTrue);
  if (!chosen_profile) {
    // Note, if we need the exact reason we need to modify the disclaimer
    // service to provide this. However both valid reasons are treated the same
    // so using the management rejection is sufficient.
    FinishFlowWithoutHistorySyncOptin(
        HistorySyncSkipReason::kManagementRejected);
    return;
  }
  management_status_state_ = HistorySyncOptinHelper::ManagementStatusState::
      kManagementDisclaimerComplete;

  if (profile() == chosen_profile) {
    ResumeShowHistorySyncOptinScreenFlowForManagedAccount(
        account_info().account_id);
    return;
  }

  // Resume the flow in the new profile.
  HistorySyncOptinService* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(chosen_profile);
  CHECK(history_sync_optin_service);
  history_sync_optin_service
      ->ResumeShowHistorySyncOptinScreenFlowForManagedUser(
          account_info().account_id,
          std::make_unique<HistorySyncOptinServiceDefaultDelegate>(),
          access_point());

  NotifyFlowFinishedWithHistorySyncScreenSkipped(
      HistorySyncSkipReason::kResumeFlowInNewManagedProfile);
}

void HistorySyncOptinHelperInBrowser::
    ResumeShowHistorySyncOptinScreenFlowForManagedAccount(
        const CoreAccountId& managed_account_id) {
  CHECK_EQ(account_info().account_id, managed_account_id);
  CHECK(enterprise_util::UserAcceptedAccountManagement(profile()));
  management_status_state_ = HistorySyncOptinHelper::ManagementStatusState::
      kManagementDisclaimerComplete;
  AwaitSyncStartupAndShowHistorySyncScreen();
}

// HistorySyncOptinHelperInProfilePicker
HistorySyncOptinHelperInProfilePicker::HistorySyncOptinHelperInProfilePicker(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    signin_metrics::AccessPoint access_point)
    : HistorySyncOptinHelper(identity_manager,
                             profile,
                             account_info,
                             delegate,
                             access_point) {}

HistorySyncOptinHelperInProfilePicker::
    ~HistorySyncOptinHelperInProfilePicker() = default;

void HistorySyncOptinHelperInProfilePicker::
    DetermineManagementStatusAndShowManagementScreens() {
  if (policy_helper_) {
    return;
  }
  CHECK(management_status_state_ ==
        HistorySyncOptinHelper::ManagementStatusState::
            kManagementDisclaimerNotStarted);
  management_status_state_ = HistorySyncOptinHelper::ManagementStatusState::
      kManagementDisclaimerInProgress;

  // Register for policies to determine if the user is managed.
  // Show the management screen for managed user, before proceeding with the
  // flow.
  auto extended_account_info =
      IdentityManagerFactory::GetForProfile(profile())->FindExtendedAccountInfo(
          account_info());
  policy_helper_ = std::make_unique<HistorySyncOptinPolicyHelper>(
      profile(), extended_account_info,
      /*on_register_for_policies_callback=*/
      base::BindOnce(&HistorySyncOptinHelperInProfilePicker::
                         MaybeShowAccountManagementScreen,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_policies_fetched_callback=*/
      base::BindOnce(&HistorySyncOptinHelperInProfilePicker::
                         ResumeShowHistorySyncOptinScreenFlow,
                     weak_ptr_factory_.GetWeakPtr(), signin::Tribool::kTrue));
  policy_helper_->RegisterForPolicies();
  return;
}

base::TimeDelta HistorySyncOptinHelperInProfilePicker::GetSyncStartupDelay() {
  return base::Seconds(
      kAwaitSyncServiceStartupInProfilePickerTimeoutSeconds.Get());
}

void HistorySyncOptinHelperInProfilePicker::MaybeShowAccountManagementScreen(
    bool is_managed_account) {
  if (!is_managed_account) {
    ResumeShowHistorySyncOptinScreenFlow(signin::Tribool::kFalse);
    return;
  }
  if (is_managed_account &&
      !enterprise_util::UserAcceptedAccountManagement(profile())) {
    // If the user has not yet have accepted management, we show the appropriate
    // screen.
    ShowAccountManagementScreen();
    return;
  }
  FetchPoliciesAndUpdateManagedDisclaimerState();
}

void HistorySyncOptinHelperInProfilePicker::ShowAccountManagementScreen() {
  CHECK(!enterprise_util::UserAcceptedAccountManagement(profile()));
  CHECK_EQ(maybe_managed_account(), signin::Tribool::kTrue);
  delegate()->ShowAccountManagementScreen(base::BindOnce(
      &HistorySyncOptinHelperInProfilePicker::OnAccountManagementScreenClosed,
      weak_ptr_factory_.GetWeakPtr()));
}

void HistorySyncOptinHelperInProfilePicker::OnAccountManagementScreenClosed(
    signin::SigninChoice result) {
  switch (result) {
    case signin::SIGNIN_CHOICE_CONTINUE:
    case signin::SIGNIN_CHOICE_SIZE:
      // These cases do not apply in the profile picker flow.
      NOTREACHED();
    case signin::SIGNIN_CHOICE_CANCEL:
      NotifyFlowFinishedWithHistorySyncScreenSkipped(
          HistorySyncSkipReason::kManagementRejected);
      return;
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      // Mark the user having accepted the management.
      enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
      FetchPoliciesAndUpdateManagedDisclaimerState();
      return;
  }
}

void HistorySyncOptinHelperInProfilePicker::
    FetchPoliciesAndUpdateManagedDisclaimerState() {
  management_status_state_ = HistorySyncOptinHelper::ManagementStatusState::
      kManagementDisclaimerComplete;
  CHECK(policy_helper_);
  policy_helper_->FetchPolicies();
}

void HistorySyncOptinHelperInProfilePicker::
    ResumeShowHistorySyncOptinScreenFlowForManagedAccount(
        const CoreAccountId& managed_account_id) {
  // This method is only used for the browser case.
  NOTREACHED();
}
