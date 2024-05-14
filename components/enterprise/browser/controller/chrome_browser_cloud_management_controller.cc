// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_helper.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace policy {

const base::FilePath::CharType
    ChromeBrowserCloudManagementController::kPolicyDir[] =
        FILE_PATH_LITERAL("Policy");

std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
ChromeBrowserCloudManagementController::Delegate::
    CreateDeviceTrustKeyManager() {
  return nullptr;
}

void ChromeBrowserCloudManagementController::Delegate::DeferInitialization(
    base::OnceClosure callback) {
  NOTREACHED_IN_MIGRATION();
}

ChromeBrowserCloudManagementController::ChromeBrowserCloudManagementController(
    std::unique_ptr<ChromeBrowserCloudManagementController::Delegate> delegate)
    : delegate_(std::move(delegate)),
      client_data_delegate_(delegate_->CreateClientDataDelegate()) {
  delegate_->SetDMTokenStorageDelegate();
}

ChromeBrowserCloudManagementController::
    ~ChromeBrowserCloudManagementController() {
  if (policy_fetcher_)
    policy_fetcher_->RemoveClientObserver(this);
  if (cloud_policy_client_)
    cloud_policy_client_->RemoveObserver(this);
}

// static
bool ChromeBrowserCloudManagementController::IsEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableChromeBrowserCloudManagement);
#endif
}

std::unique_ptr<MachineLevelUserCloudPolicyManager>
ChromeBrowserCloudManagementController::CreatePolicyManager(
    ConfigurationPolicyProvider* platform_provider) {
  if (!IsEnabled()) {
    return nullptr;
  }

  std::string enrollment_token =
      BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  DMToken dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (dm_token.is_empty())
    VLOG_POLICY(1, CBCM_ENROLLMENT) << "DM token = empty";
  else if (dm_token.is_invalid())
    VLOG_POLICY(1, CBCM_ENROLLMENT) << "DM token = invalid";
  else if (dm_token.is_valid())
    VLOG_POLICY(1, CBCM_ENROLLMENT) << "DM token = from persistence";

  VLOG_POLICY(1, CBCM_ENROLLMENT) << "Enrollment token = " << enrollment_token;
  VLOG_POLICY(1, CBCM_ENROLLMENT) << "Client ID = " << client_id;

  // Don't create the policy manager if the DM token is explicitly invalid or if
  // both tokens are empty.
  if (dm_token.is_invalid() ||
      (enrollment_token.empty() && dm_token.is_empty())) {
    return nullptr;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(delegate_->GetUserDataDirKey(), &user_data_dir))
    return nullptr;

  DVLOG_POLICY(1, CBCM_ENROLLMENT)
      << "Creating machine level user cloud policy manager";

  base::FilePath policy_dir =
      user_data_dir.Append(ChromeBrowserCloudManagementController::kPolicyDir);

  base::FilePath external_policy_path = delegate_->GetExternalPolicyDir();

  std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
      MachineLevelUserCloudPolicyStore::Create(
          dm_token, client_id, external_policy_path, policy_dir,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               // Block shutdown to make sure the policy cache update is always
               // finished.
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  return std::make_unique<MachineLevelUserCloudPolicyManager>(
      std::move(policy_store), nullptr, policy_dir,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      delegate_->CreateNetworkConnectionTrackerGetter());
}

void ChromeBrowserCloudManagementController::DeferrableCreatePolicyManager(
    ConfigurationPolicyProvider* platform_provider,
    base::OnceCallback<
        void(std::unique_ptr<MachineLevelUserCloudPolicyManager>)> callback) {
  if (delegate_->ReadyToCreatePolicyManager()) {
    DeferrableCreatePolicyManagerImpl(platform_provider, std::move(callback));
  } else {
    // Postpone policy manager creation to happen during controller
    // initialization, so it's guaranteed that all dependencies have been
    // resolved (e.g. on Android it depends on PolicyService being created
    // and initialized).
    create_cloud_policy_manager_callback_ = base::BindOnce(
        &ChromeBrowserCloudManagementController::
            DeferrableCreatePolicyManagerImpl,
        weak_factory_.GetWeakPtr(), platform_provider, std::move(callback));
  }
}

void ChromeBrowserCloudManagementController::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!IsEnabled()) {
    VLOG_POLICY(1, CBCM_ENROLLMENT)
        << "Cloud management controller initialization aborted as CBCM is not "
           "enabled. Please use the `--enable-chrome-browser-cloud-management` "
           "command line flag to enable it if you are not using the official "
           "Google Chrome build.";
    return;
  }
  VLOG_POLICY(1, CBCM_ENROLLMENT)
      << "Starting CBCM Controller Initialization";

  delegate_->InitializeOAuthTokenFactory(url_loader_factory, local_state);

  if (create_cloud_policy_manager_callback_) {
    // The conditions that allow controller initialization should also unblock
    // policy manager creation (e.g. on Android both depend on PolicyService
    // being initialized).
    DCHECK(delegate_->ReadyToCreatePolicyManager());

    std::move(create_cloud_policy_manager_callback_).Run();
  }

  // Post the task of CreateReportScheduler to run on best effort after launch
  // is completed.
  delegate_->GetBestEffortTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeBrowserCloudManagementController::CreateReportScheduler,
          weak_factory_.GetWeakPtr()));

  MachineLevelUserCloudPolicyManager* policy_manager =
      delegate_->GetMachineLevelUserCloudPolicyManager();
  DeviceManagementService* device_management_service =
      delegate_->GetDeviceManagementService();

  if (!policy_manager) {
    LOG_POLICY(INFO, CBCM_ENROLLMENT)
        << "No machine level policy manager exists.";
    return;
  }

  // If there exists an enrollment token, then there are three states:
  //   1/ There also exists a valid DM token.  This machine is already
  //      registered, so the next step is to fetch policies.
  //   2/ There is no DM token.  In this case the machine is not already
  //      registered and needs to request a DM token.
  //   3/ The also exists an invalid DM token.  Do not fetch policies or try to
  //      request a DM token in that case.
  std::string enrollment_token;
  std::string client_id;
  DMToken dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();

  if (dm_token.is_invalid()) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Enrollment Token exists but DM Token is Invalid";
    return;
  }

  if (dm_token.is_valid()) {
    VLOG_POLICY(1, CBCM_ENROLLMENT) << "Valid DM Token retrieved.";
    policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
        policy_manager, local_state, device_management_service,
        url_loader_factory);
    policy_fetcher_->AddClientObserver(this);
    return;
  }

  if (!GetEnrollmentTokenAndClientId(&enrollment_token, &client_id))
    return;

  DCHECK(!enrollment_token.empty());
  DCHECK(!client_id.empty());

  cloud_management_registrar_ =
      std::make_unique<ChromeBrowserCloudManagementRegistrar>(
          device_management_service, url_loader_factory);
  policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
      policy_manager, local_state, device_management_service,
      url_loader_factory);
  policy_fetcher_->AddClientObserver(this);

  if (dm_token.is_empty()) {
    delegate_->StartWatchingRegistration(this);

    enrollment_start_time_ = base::Time::Now();

    // Not registered already, so do it now.
    LOG_POLICY(INFO, CBCM_ENROLLMENT)
        << "Starting cloud management registration with an enrollment token.";
    cloud_management_registrar_->RegisterForCloudManagementWithEnrollmentToken(
        enrollment_token, client_id, *client_data_delegate_,
        base::BindOnce(
            &ChromeBrowserCloudManagementController::
                RegisterForCloudManagementWithEnrollmentTokenCallback,
            weak_factory_.GetWeakPtr()));
    // On Windows, if Chrome is installed on the user level, we can't store the
    // DM token in the registry at the end of enrollment. Hence Chrome needs to
    // re-enroll every launch.
    // Based on the UMA metrics
    // Enterprise.MachineLevelUserCloudPolicyEnrollment.InstallLevel_Win,
    // the number of user-level enrollment is very low
    // compare to the total CBCM users. In additional to that, devices are now
    // mostly enrolled with Google Update on Windows. Based on that, we won't do
    // anything special for user-level install enrollment.
  }
}

void ChromeBrowserCloudManagementController::MaybeInit(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (delegate_->ReadyToInit()) {
    Init(local_state, url_loader_factory);
  } else {
    LOG_POLICY(INFO, CBCM_ENROLLMENT)
        << "Deferring CBCM Controller Initialization until it is unblocked.";
    delegate_->DeferInitialization(base::BindOnce(
        &ChromeBrowserCloudManagementController::Init,
        weak_factory_.GetWeakPtr(), local_state, url_loader_factory));
  }
}

bool ChromeBrowserCloudManagementController::
    WaitUntilPolicyEnrollmentFinished() {
  return delegate_->WaitUntilPolicyEnrollmentFinished();
}

void ChromeBrowserCloudManagementController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeBrowserCloudManagementController::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ChromeBrowserCloudManagementController::
    IsEnterpriseStartupDialogShowing() {
  return delegate_->IsEnterpriseStartupDialogShowing();
}

void ChromeBrowserCloudManagementController::UnenrollBrowser(
    bool delete_dm_token) {
  if (delete_dm_token) {
    DVLOG_POLICY(1, CBCM_ENROLLMENT)
        << "Browser unenrollment: Attempting DMToken deletion";
    BrowserDMTokenStorage::Get()->ClearDMToken(base::BindOnce(
        &ChromeBrowserCloudManagementController::UnenrollCallback,
        weak_factory_.GetWeakPtr(), "DMTokenDeletion"));
    return;
  }

  // Invalidate DM token in storage.
  DVLOG_POLICY(1, CBCM_ENROLLMENT)
      << "Browser unenrollment: Attempting DMToken invalidation";
  BrowserDMTokenStorage::Get()->InvalidateDMToken(
      base::BindOnce(&ChromeBrowserCloudManagementController::UnenrollCallback,
                     weak_factory_.GetWeakPtr(), "UnenrollSuccess"));
}

void ChromeBrowserCloudManagementController::InvalidatePolicies() {
  // Reset policies.
  if (policy_fetcher_) {
    policy_fetcher_->RemoveClientObserver(this);
    policy_fetcher_->Disconnect();
  }

  // This causes the scheduler to stop refreshing itself since the DM token is
  // no longer valid.
  if (report_scheduler_)
    report_scheduler_->OnDMTokenUpdated();
}

void ChromeBrowserCloudManagementController::UnenrollCallback(
    const std::string& metric_name,
    bool success) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Enterprise.MachineLevelUserCloudPolicyEnrollment.", metric_name}),
      success);
  DVLOG_POLICY(1, CBCM_ENROLLMENT)
      << "Browser unenrollment: " << (success ? "succeeded" : "failed");

  if (success)
    InvalidatePolicies();

  NotifyBrowserUnenrolled(success);
}

void ChromeBrowserCloudManagementController::OnPolicyFetched(
    CloudPolicyClient* client) {
  // Ignored.
}

void ChromeBrowserCloudManagementController::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  // Ignored.
}

void ChromeBrowserCloudManagementController::OnClientError(
    CloudPolicyClient* client) {
  // DM_STATUS_SERVICE_DEVICE_NOT_FOUND being the last status implies the
  // browser has been unenrolled via DMToken invalidation, so it is not expected
  // to re-enroll automatically. DM_STATUS_SERVICE_DEVICE_NEEDS_RESET signals
  // that the browser has been unenrolled via DMToken deletion, and that it will
  // automatically re-enroll if a valid enrollment token has been set.
  if (client->last_dm_status() == DM_STATUS_SERVICE_DEVICE_NOT_FOUND ||
      client->last_dm_status() == DM_STATUS_SERVICE_DEVICE_NEEDS_RESET) {
    UnenrollBrowser(/*delete_dm_token=*/client->last_dm_status() ==
                    DM_STATUS_SERVICE_DEVICE_NEEDS_RESET);
  }
}

void ChromeBrowserCloudManagementController::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  delegate_->OnServiceAccountSet(client, account_email);
}

void ChromeBrowserCloudManagementController::ShutDown() {
  NotifyShutdown();
  delegate_->ShutDown();
  if (report_scheduler_)
    report_scheduler_.reset();
}

enterprise_connectors::DeviceTrustKeyManager*
ChromeBrowserCloudManagementController::GetDeviceTrustKeyManager() {
  if (!device_trust_key_manager_) {
    device_trust_key_manager_ = delegate_->CreateDeviceTrustKeyManager();
  }
  return device_trust_key_manager_.get();
}

void ChromeBrowserCloudManagementController::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  delegate_->SetGaiaURLLoaderFactory(url_loader_factory);
}

void ChromeBrowserCloudManagementController::NotifyPolicyRegisterFinished(
    bool succeeded) {
  for (auto& observer : observers_) {
    observer.OnPolicyRegisterFinished(succeeded);
  }
}

void ChromeBrowserCloudManagementController::NotifyBrowserUnenrolled(
    bool succeeded) {
  for (auto& observer : observers_)
    observer.OnBrowserUnenrolled(succeeded);
}

void ChromeBrowserCloudManagementController::NotifyCloudReportingLaunched() {
  for (auto& observer : observers_) {
    observer.OnCloudReportingLaunched(report_scheduler_.get());
  }
}

void ChromeBrowserCloudManagementController::NotifyShutdown() {
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
}

bool ChromeBrowserCloudManagementController::GetEnrollmentTokenAndClientId(
    std::string* enrollment_token,
    std::string* client_id) {
  *client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id->empty())
    return false;

  *enrollment_token = BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  return !enrollment_token->empty();
}

void ChromeBrowserCloudManagementController::
    RegisterForCloudManagementWithEnrollmentTokenCallback(
        const std::string& dm_token,
        const std::string& client_id) {
  base::TimeDelta enrollment_time = base::Time::Now() - enrollment_start_time_;

  if (dm_token.empty()) {
    VLOG_POLICY(1, CBCM_ENROLLMENT)
        << "No DM token returned from browser registration.";
    RecordEnrollmentResult(
        ChromeBrowserCloudManagementEnrollmentResult::kFailedToFetch);
    MachineLevelUserCloudPolicyManager* policy_manager =
        delegate_->GetMachineLevelUserCloudPolicyManager();

    if (policy_manager)
      policy_manager->store()->InitWithoutToken();
    NotifyPolicyRegisterFinished(false);
    return;
  }

  VLOG_POLICY(1, CBCM_ENROLLMENT) << "DM token retrieved from server.";

  base::UmaHistogramTimes(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.RequestSuccessTime",
      enrollment_time);

  // TODO(alito): Log failures to store the DM token. Should we try again later?
  BrowserDMTokenStorage::Get()->StoreDMToken(
      dm_token,
      base::BindOnce(
          [](base::WeakPtr<ChromeBrowserCloudManagementController> controller,
             bool success) {
            if (!success) {
              DVLOG_POLICY(1, CBCM_ENROLLMENT)
                  << "Failed to store the DM token";
              controller->RecordEnrollmentResult(
                  ChromeBrowserCloudManagementEnrollmentResult::kFailedToStore);
            } else {
              DVLOG_POLICY(1, CBCM_ENROLLMENT)
                  << "Successfully stored the DM token";
              controller->RecordEnrollmentResult(
                  ChromeBrowserCloudManagementEnrollmentResult::kSuccess);
            }
          },
          weak_factory_.GetWeakPtr()));

  // Start fetching policies.
  VLOG_POLICY(1, POLICY_FETCHING) << "Fetch policy after enrollment.";
  policy_fetcher_->SetupRegistrationAndFetchPolicy(
      BrowserDMTokenStorage::Get()->RetrieveDMToken(), client_id);
  if (report_scheduler_) {
    report_scheduler_->OnDMTokenUpdated();
  }

  NotifyPolicyRegisterFinished(true);
}

void ChromeBrowserCloudManagementController::CreateReportScheduler() {
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      delegate_->GetDeviceManagementService(),
      delegate_->GetSharedURLLoaderFactory(),
      CloudPolicyClient::DeviceDMTokenCallback());
  cloud_policy_client_->AddObserver(this);
  auto reporting_delegate_factory = delegate_->GetReportingDelegateFactory();

  enterprise_reporting::ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = reporting_delegate_factory->GetReportSchedulerDelegate();
  params.report_generator =
      std::make_unique<enterprise_reporting::ReportGenerator>(
          reporting_delegate_factory.get());
  params.real_time_report_controller =
      std::make_unique<enterprise_reporting::RealTimeReportController>(
          reporting_delegate_factory.get());

  report_scheduler_ = std::make_unique<enterprise_reporting::ReportScheduler>(
      std::move(params));

  NotifyCloudReportingLaunched();
}

void ChromeBrowserCloudManagementController::DeferrableCreatePolicyManagerImpl(
    ConfigurationPolicyProvider* platform_provider,
    base::OnceCallback<
        void(std::unique_ptr<MachineLevelUserCloudPolicyManager>)> callback) {
  std::unique_ptr<MachineLevelUserCloudPolicyManager> policy_manager =
      CreatePolicyManager(platform_provider);
  std::move(callback).Run(std::move(policy_manager));
}

void ChromeBrowserCloudManagementController::RecordEnrollmentResult(
    ChromeBrowserCloudManagementEnrollmentResult result) {
  base::UmaHistogramEnumeration(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result", result);
  for (auto& observer : observers_) {
    observer.OnEnrollmentResultRecorded();
  }
}

}  // namespace policy
