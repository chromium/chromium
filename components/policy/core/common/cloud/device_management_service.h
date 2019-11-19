// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class DMAuth;

// Used in the Enterprise.DMServerRequestSuccess histogram, shows how many
// retries we had to do to execute the DeviceManagementRequestJob.
enum class DMServerRequestSuccess {
  // No retries happened, the request succeeded for the first try.
  kRequestNoRetry = 0,

  // 1..kMaxRetries: number of retries. kMaxRetries is the maximum number of
  // retries allowed, defined in the .cc file.

  // The request failed (too many retries or non-retryable error).
  kRequestFailed = 10,
  // The server responded with an error.
  kRequestError = 11,

  kMaxValue = kRequestError,

};

// The device management service is responsible for everything related to
// communication with the device management server. It creates the backends
// objects that the device management policy provider and friends use to issue
// requests.
class POLICY_EXPORT DeviceManagementService {
 public:
  // HTTP Error Codes of the DM Server with their concrete meanings in the
  // context of the DM Server communication.
  static constexpr int kSuccess = 200;
  static constexpr int kInvalidArgument = 400;
  static constexpr int kInvalidAuthCookieOrDMToken = 401;
  static constexpr int kMissingLicenses = 402;
  static constexpr int kDeviceManagementNotAllowed = 403;
  static constexpr int kInvalidURL =
      404;  // This error is not coming from the GFE.
  static constexpr int kInvalidSerialNumber = 405;
  static constexpr int kDomainMismatch = 406;
  static constexpr int kDeviceIdConflict = 409;
  static constexpr int kDeviceNotFound = 410;
  static constexpr int kPendingApproval = 412;
  static constexpr int kRequestTooLarge = 413;
  static constexpr int kConsumerAccountWithPackagedLicense = 417;
  static constexpr int kInternalServerError = 500;
  static constexpr int kServiceUnavailable = 503;
  static constexpr int kPolicyNotFound = 902;
  static constexpr int kDeprovisioned = 903;
  static constexpr int kArcDisabled = 904;

  // Number of times to retry on ERR_NETWORK_CHANGED errors.
  static const int kMaxRetries = 3;

  // Obtains the parameters used to contact the server.
  // This allows creating the DeviceManagementService early and getting these
  // parameters later. Passing the parameters directly in the ctor isn't
  // possible because some aren't ready during startup. http://crbug.com/302798
  class POLICY_EXPORT Configuration {
   public:
    virtual ~Configuration() {}

    // Server at which to contact the service (DMServer).
    virtual std::string GetDMServerUrl() = 0;

    // Agent reported in the "agent" query parameter.
    virtual std::string GetAgentParameter() = 0;

    // The platform reported in the "platform" query parameter.
    virtual std::string GetPlatformParameter() = 0;

    // Server at which to contact the real time reporting service.
    virtual std::string GetReportingServerUrl() = 0;
  };

  // A DeviceManagementService job manages network requests to the device
  // management and real-time reporting services. Jobs are created by calling
  // CreateJob() and specifying a JobConfiguration. Jobs can be canceled by
  // deleting the returned Job object.
  //
  // If network requests fail, the Job will retry them.
  //
  // JobConfiguration is the interface used by callers to specify parameters
  // of network requests.  This object is not immutable and may be changed after
  // a call to OnBeforeRetry().  DeviceManagementService calls the GetXXX
  // methods again to create a new network request for each retry.
  //
  // JobControl is the interface used internally by DeviceManagementService to
  // control a job.

  class POLICY_EXPORT Job {
   public:
    virtual ~Job() {}
  };

  class POLICY_EXPORT JobConfiguration {
   public:
    // Describes the job type.  (Integer values are stated explicitly to
    // facilitate reading of logs.)
    // TYPE_INVALID is used only in tests so that they can EXPECT the correct
    // job type has been used.  Otherwise, tests would need to initially set
    // the type to somehing like TYPE_AUTO_ENROLLMENT, and then it would not
    // be possible to EXPECT the job type in auto enrollment tests.
    enum JobType {
      TYPE_INVALID = -1,
      TYPE_AUTO_ENROLLMENT = 0,
      TYPE_REGISTRATION = 1,
      TYPE_API_AUTH_CODE_FETCH = 2,
      TYPE_POLICY_FETCH = 3,
      TYPE_UNREGISTRATION = 4,
      TYPE_UPLOAD_CERTIFICATE = 5,
      TYPE_DEVICE_STATE_RETRIEVAL = 6,
      TYPE_UPLOAD_STATUS = 7,
      TYPE_REMOTE_COMMANDS = 8,
      TYPE_ATTRIBUTE_UPDATE_PERMISSION = 9,
      TYPE_ATTRIBUTE_UPDATE = 10,
      TYPE_GCM_ID_UPDATE = 11,
      TYPE_ANDROID_MANAGEMENT_CHECK = 12,
      TYPE_CERT_BASED_REGISTRATION = 13,
      TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER = 14,
      TYPE_ACTIVE_DIRECTORY_PLAY_ACTIVITY = 15,
      TYPE_REQUEST_LICENSE_TYPES = 16,
      TYPE_UPLOAD_APP_INSTALL_REPORT = 17,
      TYPE_TOKEN_ENROLLMENT = 18,
      TYPE_CHROME_DESKTOP_REPORT = 19,
      TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL = 20,
      TYPE_UPLOAD_POLICY_VALIDATION_REPORT = 21,
      TYPE_UPLOAD_REAL_TIME_REPORT = 22,
      TYPE_REQUEST_SAML_URL = 23,
      TYPE_CHROME_OS_USER_REPORT = 24,
    };

    // The set of HTTP query parmaters of the request.
    using ParameterMap = std::map<std::string, std::string>;

    // Convert the job type into a string.
    static std::string GetJobTypeAsString(JobType type);

    virtual ~JobConfiguration() {}

    virtual JobType GetType() = 0;

    virtual const ParameterMap& GetQueryParams() = 0;

    // Gets the factory to create URL fetchers for requests.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetUrlLoaderFactory() = 0;

    // Gets the payload to send in requests.
    virtual std::string GetPayload() = 0;

    // Returns the network annotation to assign to requests.
    virtual net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() = 0;

    // Returns a properly initialized resource for requests.
    virtual std::unique_ptr<network::ResourceRequest> GetResourceRequest(
        bool bypass_proxy,
        int last_error) = 0;

    // Returns the the UMA histogram to record stats about the network request.
    virtual std::string GetUmaName() = 0;

    // Prepare this job for a network request retry.
    virtual void OnBeforeRetry() = 0;

    // Called when a result is available for the request (possibly after
    // retries). If |net_error| is net::OK, |response_code| will be set to the
    // HTTP response code.
    virtual void OnURLLoadComplete(Job* job,
                                   int net_error,
                                   int response_code,
                                   const std::string& response_body) = 0;
  };

  class POLICY_EXPORT JobControl {
   public:
    enum RetryMethod {
      // No retry required for this request.
      NO_RETRY,
      // Should retry immediately (no delay).
      RETRY_IMMEDIATELY,
      // Should retry after a delay.
      RETRY_WITH_DELAY
    };

    virtual ~JobControl() {}

    // Returns the configuration that controls the parameters of network
    // requests managed by this job.  The Job owns the configuration.
    virtual JobConfiguration* GetConfiguration() = 0;

    // Gets a weakpointer to the Job that is used to delay retries of the job.
    virtual base::WeakPtr<JobControl> GetWeakPtr() = 0;

    // Creates the URL loader for this job.
    virtual std::unique_ptr<network::SimpleURLLoader> CreateFetcher() = 0;

    // Handle the response of this job.  If the function returns anything other
    // than NO_RETRY, the the job did not complete and must be retried.  In this
    // case, *|retry_delay| contains the retry delay in ms.
    virtual RetryMethod OnURLLoadComplete(const std::string& response_body,
                                          const std::string& mime_type,
                                          int net_error,
                                          int response_code,
                                          bool was_fetched_via_proxy,
                                          int* retry_delay) = 0;
  };

  explicit DeviceManagementService(
      std::unique_ptr<Configuration> configuration);
  virtual ~DeviceManagementService();

  // Creates a new device management request job.
  std::unique_ptr<Job> CreateJob(std::unique_ptr<JobConfiguration> config);

  // Schedules a task to run |Initialize| after |delay_milliseconds| had passed.
  void ScheduleInitialization(int64_t delay_milliseconds);

  // Makes the service stop all requests.
  void Shutdown();

  Configuration* configuration() { return configuration_.get(); }

  // Called by SimpleURLLoader.
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  // Called by OnURLLoaderComplete, exposed publicly to ease unit testing.
  void OnURLLoaderCompleteInternal(network::SimpleURLLoader* url_loader,
                                   const std::string& response_body,
                                   const std::string& mime_type,
                                   int net_error,
                                   int response_code,
                                   bool was_fetched_via_proxy);

  // Returns the SimpleURLLoader for testing. Expects that there's only one.
  network::SimpleURLLoader* GetSimpleURLLoaderForTesting();

  // Sets the retry delay to a shorter time to prevent browser tests from
  // timing out.
  static void SetRetryDelayForTesting(long retryDelayMs);

 protected:
  // Starts processing any queued jobs.
  void Initialize();

  // Starts a job.  Virtual for overriding in tests.
  virtual void StartJob(JobControl* job);
  void StartJobAfterDelay(base::WeakPtr<JobControl> job);

  // Adds a job. Caller must make sure the job pointer stays valid until the job
  // completes or gets canceled via RemoveJob().
  void AddJob(JobControl* job);

  // Removes a job. The job will be removed and won't receive a completion
  // callback.
  void RemoveJob(JobControl* job);

  base::WeakPtr<DeviceManagementService> GetWeakPtr();

  const scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // Moves jobs from the queued state to the pending state and starts them.
  // This should only be called when DeviceManagementService is already
  // initialized.
  void StartQueuedJobs();

  // Used in tests to queue jobs to be executed later via StartQueuedJobs().
  // This should only be called when DeviceManagementService is already
  // initialized.
  void RequeueJobForTesting(JobControl* job);

 private:
  typedef std::map<const network::SimpleURLLoader*, JobControl*> JobFetcherMap;
  typedef base::circular_deque<JobControl*> JobQueue;

  class JobImpl;

  // A Configuration implementation that is used to obtain various parameters
  // used to talk to the device management server.
  std::unique_ptr<Configuration> configuration_;

  // The jobs we currently have in flight.
  JobFetcherMap pending_jobs_;

  // Jobs that are registered, but not started yet.
  JobQueue queued_jobs_;

  // If this service is initialized, incoming requests get fired instantly.
  // If it is not initialized, incoming requests are queued.
  bool initialized_;

  // TaskRunner used to schedule retry attempts.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  // Used to create tasks which run delayed on the UI thread.
  base::WeakPtrFactory<DeviceManagementService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementService);
};

// Base class used to implement job configurations.
class POLICY_EXPORT JobConfigurationBase
    : public DeviceManagementService::JobConfiguration {
 protected:
  JobConfigurationBase(JobType type,
                       std::unique_ptr<DMAuth> auth_data,
                       base::Optional<std::string> oauth_token,
                       scoped_refptr<network::SharedURLLoaderFactory> factory);
  ~JobConfigurationBase() override;

 protected:
  // Adds the query parameter to the network request's URL.  If the parameter
  // already exists its value is replaced.
  void AddParameter(const std::string& name, const std::string& value);

  const DMAuth& GetAuth() { return *auth_data_.get(); }

  // DeviceManagementService::JobConfiguration.
  JobType GetType() override;
  const ParameterMap& GetQueryParams() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() override;
  std::unique_ptr<network::ResourceRequest> GetResourceRequest(
      bool bypass_proxy,
      int last_error) override;

  // Derived classes should return the base URL for the request.
  virtual GURL GetURL(int last_error) = 0;

 private:
  JobType type_;
  scoped_refptr<network::SharedURLLoaderFactory> factory_;

  // Auth data that will be passed as 'Authorization' header. Both |auth_data_|
  // and |oauth_token_| can be specified for one request.
  std::unique_ptr<DMAuth> auth_data_;

  // OAuth token that will be passed as a query parameter. Both |auth_data_|
  // and |oauth_token_| can be specified for one request.
  base::Optional<std::string> oauth_token_;

  // Query parameters for the network request.
  ParameterMap query_params_;

  DISALLOW_COPY_AND_ASSIGN(JobConfigurationBase);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
