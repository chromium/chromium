// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/chrome_browser_cloud_management_metrics.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/policy_service.h"

class PrefService;

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_reporting {
class ReportScheduler;
}  // namespace enterprise_reporting

namespace policy {
class ChromeBrowserCloudManagementRegistrar;
class ClientDataDelegate;
class ConfigurationPolicyProvider;
class MachineLevelUserCloudPolicyManager;
class MachineLevelUserCloudPolicyFetcher;

// A class that setups and manages all CBCM related features.
// Notes on threading and lifetime: This object lives on the UI thread and is
// owned by |g_browser_process|.
class ChromeBrowserCloudManagementController
    : public CloudPolicyClient::Observer {
 public:
  // Chrome browser cloud management enrollment result.
  enum class RegisterResult {
    kNoEnrollmentNeeded,  // The device won't be enrolled without an enrollment
                          // token.
    kEnrollmentSuccessBeforeDialogDisplayed,  // The enrollment process is
                                              // finished before dialog
                                              // displayed.
    kEnrollmentSuccess,  // The device has been enrolled successfully
    kQuitDueToFailure,   // The enrollment has failed or aborted, user choose to
                         // quit Chrome.
    kRestartDueToFailure,  // The enrollment has failed, user choose to restart
    kEnrollmentFailedSilently,  // The enrollment has failed, admin choose to
                                // ignore the error message.
    kEnrollmentFailedSilentlyBeforeDialogDisplayed,  // The enrollment has
                                                     // failed before dialog
                                                     // displayed, admin choose
                                                     // to ignore the error
                                                     // message.
  };

  class Delegate {
   public:
    using NetworkConnectionTrackerGetter =
        base::RepeatingCallback<network::NetworkConnectionTracker*()>;

    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Sets the platform-specific DM token storage delegate;
    virtual void SetDMTokenStorageDelegate() = 0;

    // Returns the platform-specific DIR_USER_DATA value to pass to the
    // PathService.
    virtual int GetUserDataDirKey() = 0;

    // Returns the platform-specific file path, if any, of the browser policy
    // cache file that is fetched by external binaries. For example, on Windows,
    // the external policy cache is fetched by Google Update.
    virtual base::FilePath GetExternalPolicyDir() = 0;

    // Returns a RepeatingCallback to get the platform-specific
    // NetworkConnectionTracker.
    virtual NetworkConnectionTrackerGetter
    CreateNetworkConnectionTrackerGetter() = 0;

    // Initializes the OAuth2 token service factory for policy invalidations.
    virtual void InitializeOAuthTokenFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        PrefService* local_state) = 0;

    // Creates the RegisterWatcher. Only needed on desktop for now.
    virtual void StartWatchingRegistration(
        ChromeBrowserCloudManagementController* controller) = 0;

    // Returns true if enrollment was successful.
    virtual bool WaitUntilPolicyEnrollmentFinished() = 0;

    // Returns true if the startup dialog is showing.
    virtual bool IsEnterpriseStartupDialogShowing() = 0;

    // Starts the policy invalidation service.
    virtual void OnServiceAccountSet(CloudPolicyClient* client,
                                     const std::string& account_email) = 0;

    // Performs clean-up during shutdown.
    virtual void ShutDown() = 0;

    // Gets the platform-specific machine-level policy manager.
    virtual MachineLevelUserCloudPolicyManager*
    GetMachineLevelUserCloudPolicyManager() = 0;

    // Gets the platform-specific device management service.
    virtual DeviceManagementService* GetDeviceManagementService() = 0;

    // Gets the platform-specific URL loader factory.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetSharedURLLoaderFactory() = 0;

    // Returns a BestEffort Task Runner, bound to the UI thread like the rest of
    // this class, that is meant to be used to schedule asynchronous tasks
    // during startup.
    virtual scoped_refptr<base::SingleThreadTaskRunner>
    GetBestEffortTaskRunner() = 0;

    // Gets the platform-specific reporting delegate factory.
    virtual std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
    GetReportingDelegateFactory() = 0;

    // Creates a platform-specific DeviceTrustKeyManager instance.
    virtual std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
    CreateDeviceTrustKeyManager();

    // Sets the SharedURLLoaderFactory that this object will use to make
    // requests to GAIA.
    virtual void SetGaiaURLLoaderFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

    // Returns true if the cloud policy manager can be created rightaway of if
    // it should be deferred for some reason depending on the platform (e.g. on
    // Android it should wait for PolicyService initialization).
    virtual bool ReadyToCreatePolicyManager() = 0;

    // Returns true if controller initialization can proceed, and false it it
    // needs to be deferred. On platforms where controller initialization isn't
    // blocked, this method should return true.
    virtual bool ReadyToInit() = 0;

    // Returns the platform-specific client data delegate.
    virtual std::unique_ptr<ClientDataDelegate> CreateClientDataDelegate() = 0;

    // Postpones controller initialization until |ReadyToInit()| is true.
    // Implemented in the delegate because the reason why initialization needs
    // to be deferred may vary across platforms.
    virtual void DeferInitialization(base::OnceClosure init_callback);
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when policy enrollment is finished.
    // |succeeded| is true if |dm_token| is returned from the server.
    virtual void OnPolicyRegisterFinished(bool succeeded) {}

    // Called when the browser has been unenrolled.
    virtual void OnBrowserUnenrolled(bool succeeded) {}

    // Called when the cloud reporting is launched.
    virtual void OnCloudReportingLaunched(
        enterprise_reporting::ReportScheduler* report_scheduler) {}

    // Called when enrollment result is recorded.
    virtual void OnEnrollmentResultRecorded() {}

    // Called when shutting down.
    virtual void OnShutdown() {}
  };

  // Directory name under the user-data-dir where the policy data is stored.
  static const base::FilePath::CharType kPolicyDir[];

  explicit ChromeBrowserCloudManagementController(
      std::unique_ptr<ChromeBrowserCloudManagementController::Delegate>
          delegate);

  ChromeBrowserCloudManagementController(
      const ChromeBrowserCloudManagementController&) = delete;
  ChromeBrowserCloudManagementController& operator=(
      const ChromeBrowserCloudManagementController&) = delete;

  ~ChromeBrowserCloudManagementController() override;

  // The Chrome browser cloud management is only enabled on Chrome by default.
  // However, it can be enabled on Chromium by command line switch for test and
  // development purpose.
  static bool IsEnabled();

  // Returns a MachineLevelUserCloudPolicyManager instance if cloud management
  // is enabled, or nullptr otherwise.
  // TODO(http://crbug.com/1221173): Consider deprecating this method (still
  // used on iOS) in favor of DeferrableCreatePolicyManager.
  std::unique_ptr<MachineLevelUserCloudPolicyManager> CreatePolicyManager(
      ConfigurationPolicyProvider* platform_provider);

  // Invokes |callback| with a MachineLevelUserCloudPolicyManager instance if
  // cloud management is enabled, or with nullptr otherwise. Callback invocation
  // may be deferred if it can't be determined rightaway if cloud management
  // is enabled (e.g. on Android).
  void DeferrableCreatePolicyManager(
      ConfigurationPolicyProvider* platform_provider,
      base::OnceCallback<
          void(std::unique_ptr<MachineLevelUserCloudPolicyManager>)> callback);

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  void MaybeInit(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  bool WaitUntilPolicyEnrollmentFinished();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the enterprise startup dialog is being diaplayed.
  bool IsEnterpriseStartupDialogShowing();

  // Unenrolls the browser from cloud management by either invalidating or
  // deleting the stored DMToken.
  void UnenrollBrowser(bool delete_dm_token);

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;

  // Early cleanup during browser shutdown process
  void ShutDown();

  // Returns the device trust key manager. Returns nullptr if the Device Trust
  // feature flag isn't enabled.
  enterprise_connectors::DeviceTrustKeyManager* GetDeviceTrustKeyManager();

  // Sets the SharedURLLoaderFactory that this will be used to make requests to
  // GAIA.
  void SetGaiaURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  enterprise_reporting::ReportScheduler* report_scheduler() {
    return report_scheduler_.get();
  }

 protected:
  void NotifyPolicyRegisterFinished(bool succeeded);
  void NotifyBrowserUnenrolled(bool succeeded);
  void NotifyCloudReportingLaunched();
  void NotifyShutdown();

 private:
  bool GetEnrollmentTokenAndClientId(std::string* enrollment_token,
                                     std::string* client_id);
  void RegisterForCloudManagementWithEnrollmentTokenCallback(
      const std::string& dm_token,
      const std::string& client_id);

  void InvalidatePolicies();
  void UnenrollCallback(const std::string& metric_name, bool success);

  void CreateReportScheduler();

  // Implementation of |DeferrableCreatePolicyManager| that can be invoked right
  // away or bound to a callback to be executed later.
  void DeferrableCreatePolicyManagerImpl(
      ConfigurationPolicyProvider* platform_provider,
      base::OnceCallback<
          void(std::unique_ptr<MachineLevelUserCloudPolicyManager>)> callback);

  // Logs enrollment result to histogram
  // `Enterprise.MachineLevelUserCloudPolicyEnrollment.Result`.
  void RecordEnrollmentResult(
      ChromeBrowserCloudManagementEnrollmentResult result);

  base::ObserverList<Observer, true>::Unchecked observers_;

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<ChromeBrowserCloudManagementRegistrar>
      cloud_management_registrar_;
  std::unique_ptr<MachineLevelUserCloudPolicyFetcher> policy_fetcher_;

  // Time at which the enrollment process was started.  Used to log UMA metric.
  base::Time enrollment_start_time_;

  std::unique_ptr<enterprise_reporting::ReportScheduler> report_scheduler_;

  std::unique_ptr<CloudPolicyClient> cloud_policy_client_;

  std::unique_ptr<ClientDataDelegate> client_data_delegate_;

  // Holds a callback to the function that will consume the
  // MachineLevelUserCloudPolicyManager object once it's created.
  // This allows creation to be deferred on platforms in which the enrollment
  // token may not be immediately available (e.g. Android).
  base::OnceClosure create_cloud_policy_manager_callback_;

  std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
      device_trust_key_manager_;

  base::WeakPtrFactory<ChromeBrowserCloudManagementController> weak_factory_{
      this};
};

}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
