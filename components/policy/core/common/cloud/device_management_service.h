// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/policy_export.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

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
  static constexpr int kInvalidPackagedDeviceForKiosk = 418;
  static constexpr int kTooManyRequests = 429;
  static constexpr int kInternalServerError = 500;
  static constexpr int kServiceUnavailable = 503;
  static constexpr int kPolicyNotFound = 902;
  static constexpr int kDeprovisioned = 903;
  static constexpr int kArcDisabled = 904;
  static constexpr int kInvalidDomainlessCustomer = 905;
  static constexpr int kTosHasNotBeenAccepted = 906;
  static constexpr int kIllegalAccountForPackagedEDULicense = 907;

  // Number of times to retry on ERR_NETWORK_CHANGED errors.
  static const int kMaxRetries = 3;

  // Obtains the parameters used to contact the server.
  // This allows creating the DeviceManagementService early and getting these
  // parameters later. Passing the parameters directly in the ctor isn't
  // possible because some aren't ready during startup. http://crbug.com/302798
  class POLICY_EXPORT Configuration {
   public:
    virtual ~Configuration() = default;

    // Server at which to contact the service (DMServer).
    virtual std::string GetDMServerUrl() const = 0;

    // Agent reported in the "agent" query parameter.
    virtual std::string GetAgentParameter() const = 0;

    // The platform reported in the "platform" query parameter.
    virtual std::string GetPlatformParameter() const = 0;

    // Server at which to contact the real time reporting service.
    virtual std::string GetRealtimeReportingServerUrl() const = 0;

    // Server endpoint for encrypted events.
    virtual std::string GetEncryptedReportingServerUrl() const = 0;
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

  class JobConfiguration;

  class POLICY_EXPORT Job {
   public:
    enum RetryMethod {
      // No retry required for this request.
      NO_RETRY,
      // Should retry immediately (no delay).
      RETRY_IMMEDIATELY,
      // Should retry after a delay.
      RETRY_WITH_DELAY
    };

    virtual ~Job() = default;
  };

  class JobImpl;

  // JobForTesting is a test API to access jobs.
  // See also |FakeDeviceManagementService|.
  class POLICY_EXPORT JobForTesting {
   public:
    JobForTesting();
    explicit JobForTesting(JobImpl* job_impl);
    JobForTesting(const JobForTesting&);
    JobForTesting(JobForTesting&&) noexcept;
    JobForTesting& operator=(const JobForTesting&);
    JobForTesting& operator=(JobForTesting&&) noexcept;
    ~JobForTesting();

    bool IsActive() const;
    void Deactivate();

    // TODO(rbock) make return type const.
    JobConfiguration* GetConfigurationForTesting() const;

    Job::RetryMethod SetResponseForTesting(
        // TODO(rbock) change type to net::Error
        int net_error,
        int response_code,
        const std::string& response_body,
        const std::string& mime_type,
        bool was_fetched_via_proxy);

   private:
    base::WeakPtr<JobImpl> job_impl_;
  };

  class POLICY_EXPORT JobConfiguration {
   public:
    // Describes the job type.  (Integer values are stated explicitly to
    // facilitate reading of logs.)
    // TYPE_INVALID is used only in tests so that they can EXPECT the correct
    // job type has been used.  Otherwise, tests would need to initially set
    // the type to something like TYPE_AUTO_ENROLLMENT, and then it would not
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
      /* TYPE_REQUEST_LICENSE_TYPES = 16, */
      /*Deprecated, CloudPolicyClient no longer uses it.
        TYPE_UPLOAD_APP_INSTALL_REPORT = 17,*/
      TYPE_BROWSER_REGISTRATION = 18,
      TYPE_CHROME_DESKTOP_REPORT = 19,
      TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL = 20,
      TYPE_UPLOAD_POLICY_VALIDATION_REPORT = 21,
      TYPE_UPLOAD_REAL_TIME_REPORT = 22,
      TYPE_REQUEST_SAML_URL = 23,
      TYPE_CHROME_OS_USER_REPORT = 24,
      TYPE_CERT_PROVISIONING_REQUEST = 25,
      TYPE_PSM_HAS_DEVICE_STATE_REQUEST = 26,
      TYPE_UPLOAD_ENCRYPTED_REPORT = 27,
      TYPE_CHECK_USER_ACCOUNT = 28,
      TYPE_UPLOAD_EUICC_INFO = 29,
      TYPE_BROWSER_UPLOAD_PUBLIC_KEY = 30,
      TYPE_CHROME_PROFILE_REPORT = 31,
      TYPE_OIDC_REGISTRATION = 32,
      TYPE_TOKEN_BASED_DEVICE_REGISTRATION = 33,
      TYPE_UPLOAD_FM_REGISTRATION_TOKEN = 34,
      TYPE_POLICY_AGENT_REGISTRATION = 35,
    };

    // The set of HTTP query parameters of the request.
    using ParameterMap = std::map<std::string, std::string>;

    // Convert the job type into a string.
    static std::string GetJobTypeAsString(JobType type);

    virtual ~JobConfiguration() = default;

    virtual JobType GetType() = 0;

    virtual const DMAuth& GetAuth() const = 0;

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

    // Returns whether UMA histograms should be recorded. If this is false
    // then GetUmaName() is invalid.
    virtual bool ShouldRecordUma() const = 0;
    // Returns the the UMA histogram to record stats about the network request.
    virtual std::string GetUmaName() = 0;

    // Returns the RetryMethod the configuration considers appropriate given the
    // response from the server. The response_code is the http response, and the
    // response_body is the response returned by the server (it may be empty
    // depending on the response_code).
    // Note this method will not be called on a net_error, because the
    // assumption is that this configuration is deciding to retry based on a
    // server response, and there is no server response in that case.
    virtual Job::RetryMethod ShouldRetry(int response_code,
                                         const std::string& response_body) = 0;

    // Prepare this job for a network request retry.
    virtual void OnBeforeRetry(int response_code,
                               const std::string& response_body) = 0;

    // Called when a result is available for the request (possibly after
    // retries). If |net_error| is net::OK, |response_code| will be set to the
    // HTTP response code.
    virtual void OnURLLoadComplete(Job* job,
                                   int net_error,
                                   int response_code,
                                   const std::string& response_body) = 0;

    virtual std::optional<base::TimeDelta> GetTimeoutDuration() = 0;
  };

  explicit DeviceManagementService(
      std::unique_ptr<Configuration> configuration);
  DeviceManagementService(const DeviceManagementService&) = delete;
  DeviceManagementService& operator=(const DeviceManagementService&) = delete;
  virtual ~DeviceManagementService();

  // Creates and queues/starts a new Job.
  virtual std::unique_ptr<Job> CreateJob(
      std::unique_ptr<JobConfiguration> config);

  // Schedules a task to run |Initialize| after |delay_milliseconds| had passed.
  void ScheduleInitialization(int64_t delay_milliseconds);

  // Makes the service stop all requests.
  void Shutdown();

  const Configuration* configuration() const { return configuration_.get(); }

  // Sets the retry delay to a shorter time to prevent browser tests from
  // timing out.
  static void SetRetryDelayForTesting(long retryDelayMs);

 protected:
  // Creates a new Job without starting it.
  // Used by `FakeDeviceManagementService` to avoid queueing/starting of
  // jobs in tests.
  std::pair<std::unique_ptr<Job>, JobForTesting> CreateJobForTesting(
      std::unique_ptr<JobConfiguration> config);

  const scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForTesting() {
    return task_runner_;
  }

 private:
  using JobQueue = std::vector<base::WeakPtr<JobImpl>>;

  // Starts processing any queued jobs.
  void Initialize();

  // If called before |Initialize| this queues job.
  // Otherwise it starts the job.
  void AddJob(JobImpl* job);

  base::WeakPtr<DeviceManagementService> GetWeakPtr();

  // Moves jobs from the queued state to the pending state and starts them.
  // This should only be called when DeviceManagementService is already
  // initialized.
  void StartQueuedJobs();

  // A Configuration implementation that is used to obtain various parameters
  // used to talk to the device management server.
  std::unique_ptr<Configuration> configuration_;

  // Jobs that are added, but not started yet.
  JobQueue queued_jobs_;

  // If this service is initialized, incoming requests get started instantly.
  // If it is not initialized, incoming requests are queued.
  bool initialized_;

  // TaskRunner used to schedule retry attempts.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to run delayed tasks (e.g. |Initialize()|).
  base::WeakPtrFactory<DeviceManagementService> weak_ptr_factory_{this};
};

// Base class used to implement job configurations.
class POLICY_EXPORT JobConfigurationBase
    : public DeviceManagementService::JobConfiguration {
 public:
  JobConfigurationBase(const JobConfigurationBase&) = delete;
  JobConfigurationBase& operator=(const JobConfigurationBase&) = delete;

  // DeviceManagementService::JobConfiguration:
  JobType GetType() override;
  const ParameterMap& GetQueryParams() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() override;
  std::unique_ptr<network::ResourceRequest> GetResourceRequest(
      bool bypass_proxy,
      int last_error) override;
  bool ShouldRecordUma() const override;
  DeviceManagementService::Job::RetryMethod ShouldRetry(
      int response_code,
      const std::string& response_body) override;
  std::optional<base::TimeDelta> GetTimeoutDuration() override;

 protected:
  JobConfigurationBase(JobType type,
                       DMAuth auth_data,
                       std::optional<std::string> oauth_token,
                       scoped_refptr<network::SharedURLLoaderFactory> factory);
  ~JobConfigurationBase() override;

  // Adds the query parameter to the network request's URL.  If the parameter
  // already exists its value is replaced.
  void AddParameter(const std::string& name, const std::string& value);

  const DMAuth& GetAuth() const override;

  // Derived classes should return the base URL for the request.
  virtual GURL GetURL(int last_error) const = 0;

  // Timeout for job request
  std::optional<base::TimeDelta> timeout_;

 private:
  JobType type_;
  scoped_refptr<network::SharedURLLoaderFactory> factory_;

  // Auth data that will be passed as 'Authorization' header. Both |auth_data_|
  // and |oauth_token_| can be specified for one request.
  DMAuth auth_data_;

  // OAuth token that will be passed as a query parameter. Both |auth_data_|
  // and |oauth_token_| can be specified for one request.
  std::optional<std::string> oauth_token_;

  // Query parameters for the network request.
  ParameterMap query_params_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
