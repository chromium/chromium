// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

class PrefService;

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_reporting {
class ReportScheduler;
}

namespace policy {
class ChromeBrowserCloudManagementRegistrar;
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
    virtual base::FilePath GetExternalPolicyPath() = 0;

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

    // Creates and returns a ReportScheduler for enterprise reporting. Delegates
    // must pass the platform-specific factory that should be used to
    // instantiate the delegates for the reporting objects.
    virtual std::unique_ptr<enterprise_reporting::ReportScheduler>
    CreateReportScheduler(CloudPolicyClient* client) = 0;

    // Returns a BestEffort Task Runner, bound to the UI thread like the rest of
    // this class, that is meant to be used to schedule asynchronous tasks
    // during startup.
    virtual scoped_refptr<base::SingleThreadTaskRunner>
    GetBestEffortTaskRunner() = 0;

    // Sets the SharedURLLoaderFactory that this object will use to make
    // requests to GAIA.
    virtual void SetGaiaURLLoaderFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when policy enrollment is finished.
    // |succeeded| is true if |dm_token| is returned from the server.
    virtual void OnPolicyRegisterFinished(bool succeeded) {}

    // Called when the browser has been unenrolled.
    virtual void OnBrowserUnenrolled(bool succeeded) {}

    // Called when the cloud reporting is launched.
    virtual void OnCloudReportingLaunched() {}
  };

  // Directory name under the user-data-dir where the policy data is stored.
  static const base::FilePath::CharType kPolicyDir[];

  explicit ChromeBrowserCloudManagementController(
      std::unique_ptr<ChromeBrowserCloudManagementController::Delegate>
          delegate);
  ~ChromeBrowserCloudManagementController() override;

  // The Chrome browser cloud management is only enabled on Chrome by default.
  // However, it can be enabled on Chromium by command line switch for test and
  // development purpose.
  bool IsEnabled();

  std::unique_ptr<MachineLevelUserCloudPolicyManager> CreatePolicyManager(
      ConfigurationPolicyProvider* platform_provider);

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  bool WaitUntilPolicyEnrollmentFinished();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the enterprise startup dialog is being diaplayed.
  bool IsEnterpriseStartupDialogShowing();

  void UnenrollBrowser();

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;

  // Early cleanup during browser shutdown process
  void ShutDown();

  // Sets the SharedURLLoaderFactory that this will be used to make requests to
  // GAIA.
  void SetGaiaURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  void NotifyPolicyRegisterFinished(bool succeeded);
  void NotifyBrowserUnenrolled(bool succeeded);
  void NotifyCloudReportingLaunched();

 private:
  bool GetEnrollmentTokenAndClientId(std::string* enrollment_token,
                                     std::string* client_id);
  void RegisterForCloudManagementWithEnrollmentTokenCallback(
      const std::string& dm_token,
      const std::string& client_id);

  void InvalidatePolicies();
  void InvalidateDMTokenCallback(bool success);

  void CreateReportScheduler();

  base::ObserverList<Observer, true>::Unchecked observers_;

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<ChromeBrowserCloudManagementRegistrar>
      cloud_management_registrar_;
  std::unique_ptr<MachineLevelUserCloudPolicyFetcher> policy_fetcher_;

  // Time at which the enrollment process was started.  Used to log UMA metric.
  base::Time enrollment_start_time_;

  std::unique_ptr<enterprise_reporting::ReportScheduler> report_scheduler_;

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;

  base::WeakPtrFactory<ChromeBrowserCloudManagementController> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserCloudManagementController);
};

}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
