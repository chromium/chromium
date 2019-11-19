// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class SigningService;
class DMAuth;
class DMServerJobConfiguration;

// Implements the core logic required to talk to the device management service.
// Also keeps track of the current state of the association with the service,
// such as whether there is a valid registration (DMToken is present in that
// case) and whether and what errors occurred in the latest request.
//
// Note that CloudPolicyClient doesn't do any validation of policy responses
// such as signature and time stamp checks. These happen once the policy gets
// installed in the cloud policy cache.
class POLICY_EXPORT CloudPolicyClient {
 public:
  // Maps a (policy type, settings entity ID) pair to its corresponding
  // PolicyFetchResponse.
  using ResponseMap =
      std::map<std::pair<std::string, std::string>,
               std::unique_ptr<enterprise_management::PolicyFetchResponse>>;

  // Maps a license type to number of available licenses.
  using LicenseMap = std::map<LicenseType, int>;

  // A callback which receives boolean status of an operation.  If the operation
  // succeeded, |status| is true.
  using StatusCallback = base::Callback<void(bool status)>;

  // A callback for available licenses request. If the operation succeeded,
  // |status| is DM_STATUS_SUCCESS, and |map| contains available licenses.
  using LicenseRequestCallback = base::Callback<void(
      DeviceManagementStatus status,
      const LicenseMap& map)>;

  // A callback which receives fetched remote commands.
  using RemoteCommandCallback = base::OnceCallback<void(
      DeviceManagementStatus,
      const std::vector<enterprise_management::RemoteCommand>&,
      const std::vector<enterprise_management::SignedData>&)>;

  // A callback for fetching device robot OAuth2 authorization tokens.
  // Only occurs during enrollment, after the device is registered.
  using RobotAuthCodeCallback =
      base::OnceCallback<void(DeviceManagementStatus, const std::string&)>;

  // A callback which fetches device dm_token based on user affiliation.
  // Should be called once per registration.
  using DeviceDMTokenCallback = base::RepeatingCallback<std::string(
      const std::vector<std::string>& user_affiliation_ids)>;

  // Observer interface for state and policy changes.
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();

    // Called when a policy fetch completes successfully. If a policy fetch
    // triggers an error, OnClientError() will fire.
    virtual void OnPolicyFetched(CloudPolicyClient* client) = 0;

    // Called upon registration state changes. This callback is invoked for
    // successful completion of registration and unregistration requests.
    virtual void OnRegistrationStateChanged(CloudPolicyClient* client) = 0;

    // Indicates there's been an error in a previously-issued request.
    virtual void OnClientError(CloudPolicyClient* client) = 0;
  };

  struct POLICY_EXPORT RegistrationParameters {
   public:
    RegistrationParameters(
        enterprise_management::DeviceRegisterRequest::Type registration_type,
        enterprise_management::DeviceRegisterRequest::Flavor flavor);
    ~RegistrationParameters();

    enterprise_management::DeviceRegisterRequest::Type registration_type;
    enterprise_management::DeviceRegisterRequest::Flavor flavor;

    // Lifetime of registration. Used for easier clean up of ephemeral session
    // registrations.
    enterprise_management::DeviceRegisterRequest::Lifetime lifetime =
        enterprise_management::DeviceRegisterRequest::LIFETIME_INDEFINITE;

    // Selected license type if user is allowed to select it.
    enterprise_management::LicenseType::LicenseTypeEnum license_type =
        enterprise_management::LicenseType::UNDEFINED;

    // Device requisition.
    std::string requisition;

    // Server-backed state keys (used for forced enrollment check).
    std::string current_state_key;
  };

  // If non-empty, |machine_id|, |machine_model|, |brand_code|,
  // |ethernet_mac_address|, |dock_mac_address| and |manufacture_date| are
  // passed to the server verbatim. As these reveal machine identity, they must
  // only be used where this is appropriate (i.e. device policy, but not user
  // policy). |service| and |signing_service| are weak pointers and it's the
  // caller's responsibility to keep them valid for the lifetime of
  // CloudPolicyClient. The |signing_service| is used to sign sensitive
  // requests. |device_dm_token_callback| is used to retrieve device DMToken for
  // affiliated users. Could be null if it's not possible to use
  // device DMToken for user policy fetches.
  CloudPolicyClient(
      const std::string& machine_id,
      const std::string& machine_model,
      const std::string& brand_code,
      const std::string& ethernet_mac_address,
      const std::string& dock_mac_address,
      const std::string& manufacture_date,
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SigningService* signing_service,
      DeviceDMTokenCallback device_dm_token_callback);
  virtual ~CloudPolicyClient();

  // Sets the DMToken, thereby establishing a registration with the server. A
  // policy fetch is not automatically issued but can be requested by calling
  // FetchPolicy().
  // |user_affiliation_ids| are used to get device DMToken if relevant.
  virtual void SetupRegistration(
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids);

  // Attempts to register with the device management service. Results in a
  // registration change or error notification.
  virtual void Register(const RegistrationParameters& parameters,
                        const std::string& client_id,
                        const std::string& oauth_token);

  // Attempts to register with the device management service using a
  // registration certificate. Results in a registration change or
  // error notification.
  virtual void RegisterWithCertificate(const RegistrationParameters& parameters,
                                       const std::string& client_id,
                                       std::unique_ptr<DMAuth> auth,
                                       const std::string& pem_certificate_chain,
                                       const std::string& sub_organization);

  // Attempts to enroll with the device management service using an enrollment
  // token. Results in a registration change or error notification.
  // This method is used to register browser (e.g. for machine-level policies).
  // Device registration with enrollment token should be performed using
  // RegisterWithCertificate method.
  virtual void RegisterWithToken(const std::string& token,
                                 const std::string& client_id);

  // Sets information about a policy invalidation. Subsequent fetch operations
  // will use the given info, and callers can use fetched_invalidation_version
  // to determine which version of policy was fetched.
  void SetInvalidationInfo(int64_t version, const std::string& payload);

  // Sets OAuth token to be used as an additional authentication in requests to
  // DMServer. It is used for child user. This class does not track validity of
  // the |oauth_token|. It should be provided with a fresh token when the
  // previous token expires. If OAuth token is set for the client, it will be
  // automatically included in the folllowing requests:
  //  * policy fetch
  //  * status report upload
  virtual void SetOAuthTokenAsAdditionalAuth(const std::string& oauth_token);

  // Requests a policy fetch. The client being registered is a prerequisite to
  // this operation and this call will CHECK if the client is not in registered
  // state. FetchPolicy() triggers a policy fetch from the cloud. A policy
  // change notification is reported to the observers and the new policy blob
  // can be retrieved once the policy fetch operation completes. In case of
  // multiple requests to fetch policy, new requests will cancel any pending
  // requests and the latest request will eventually trigger notifications.
  virtual void FetchPolicy();

  // Upload a policy validation report to the server. Like FetchPolicy, this
  // method requires that the client is in a registered state. This method
  // should only be called if the policy was rejected (e.g. validation or
  // serialization error).
  virtual void UploadPolicyValidationReport(
      CloudPolicyValidatorBase::Status status,
      const std::vector<ValueValidationIssue>& value_validation_issues,
      const std::string& policy_type,
      const std::string& policy_token);

  // Requests OAuth2 auth codes for the device robot account. The client being
  // registered is a prerequisite to this operation and this call will CHECK if
  // the client is not in registered state.
  // The |callback| will be called when the operation completes.
  virtual void FetchRobotAuthCodes(std::unique_ptr<DMAuth> auth,
                                   RobotAuthCodeCallback callback);

  // Sends an unregistration request to the server.
  virtual void Unregister();

  // Upload a machine certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  virtual void UploadEnterpriseMachineCertificate(
      const std::string& certificate_data,
      const StatusCallback& callback);

  // Upload an enrollment certificate to the server.  Like FetchPolicy, this
  // method requires that the client is in a registered state.
  // |certificate_data| must hold the X.509 certificate data to be sent to the
  // server.  The |callback| will be called when the operation completes.
  virtual void UploadEnterpriseEnrollmentCertificate(
      const std::string& certificate_data,
      const StatusCallback& callback);

  // Upload an enrollment identifier to the server. Like FetchPolicy, this
  // method requires that the client is in a registered state.
  // |enrollment_id| must hold an enrollment identifier. The |callback| will be
  // called when the operation completes.
  virtual void UploadEnterpriseEnrollmentId(const std::string& enrollment_id,
                                            const StatusCallback& callback);

  // Uploads status to the server. The client must be in a registered state.
  // Only non-null statuses will be included in the upload status request. The
  // |callback| will be called when the operation completes.
  virtual void UploadDeviceStatus(
      const enterprise_management::DeviceStatusReportRequest* device_status,
      const enterprise_management::SessionStatusReportRequest* session_status,
      const enterprise_management::ChildStatusReportRequest* child_status,
      const StatusCallback& callback);

  // Uploads Chrome Desktop report to the server. As above, the client must be
  // in a registered state. |chrome_desktop_report| will be included in the
  // upload request. The |callback| will be called when the operation completes.
  virtual void UploadChromeDesktopReport(
      std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
          chrome_desktop_report,
      const StatusCallback& callback);

  // Uploads Chrome OS User report to the server. The user dm token must be set
  // properly. |chrome_os_user_report| will be included in the upload request.
  // The |callback| will be called when the operation completes.
  virtual void UploadChromeOsUserReport(
      std::unique_ptr<enterprise_management::ChromeOsUserReportRequest>
          chrome_os_user_report,
      const StatusCallback& callback);

  // Uploads |report| using the real-time reporting API.  As above, the client
  // must be in a registered state.  The |callback| will be called when the
  // operation completes.
  virtual void UploadRealtimeReport(base::Value report,
                                    const StatusCallback& callback);

  // Uploads a report on the status of app push-installs. The client must be in
  // a registered state. The |callback| will be called when the operation
  // completes.
  virtual void UploadAppInstallReport(
      const enterprise_management::AppInstallReportRequest* app_install_report,
      const StatusCallback& callback);

  // Cancels the pending app push-install status report upload, if an.
  virtual void CancelAppInstallReportUpload();

  // Attempts to fetch remote commands, with |last_command_id| being the ID of
  // the last command that finished execution and |command_results| being
  // results for previous commands which have not been reported yet. The
  // |callback| will be called when the operation completes.
  // Note that sending |last_command_id| will acknowledge this command and any
  // previous commands. A nullptr indicates that no commands have finished
  // execution.
  virtual void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<enterprise_management::RemoteCommandResult>&
          command_results,
      RemoteCommandCallback callback);

  // Sends a device attribute update permission request to the server, uses
  // |auth| to identify user who requests a permission to name a device, calls
  // a |callback| from the enrollment screen to indicate whether the device
  // naming prompt should be shown.
  void GetDeviceAttributeUpdatePermission(std::unique_ptr<DMAuth> auth,
                                          const StatusCallback& callback);

  // Sends a device naming information (Asset Id and Location) to the
  // device management server, uses |auth| to identify user who names a device,
  // the |callback| will be called when the operation completes.
  void UpdateDeviceAttributes(std::unique_ptr<DMAuth> auth,
                              const std::string& asset_id,
                              const std::string& location,
                              const StatusCallback& callback);

  // Requests a list of licenses available for enrollment. Uses |oauth_token| to
  // identify user who issues the request, the |callback| will
  // be called when the operation completes.
  void RequestAvailableLicenses(const std::string& oauth_token,
                                const LicenseRequestCallback& callback);

  // Sends a GCM id update request to the DM server. The server will
  // associate the DM token in authorization header with |gcm_id|, and
  // |callback| will be called when the operation completes.
  virtual void UpdateGcmId(const std::string& gcm_id,
                           const StatusCallback& callback);

  // Adds an observer to be called back upon policy and state changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  const std::string& machine_id() const { return machine_id_; }
  const std::string& machine_model() const { return machine_model_; }
  const std::string& brand_code() const { return brand_code_; }
  const std::string& ethernet_mac_address() const {
    return ethernet_mac_address_;
  }
  const std::string& dock_mac_address() const { return dock_mac_address_; }
  const std::string& manufacture_date() const { return manufacture_date_; }

  void set_last_policy_timestamp(const base::Time& timestamp) {
    last_policy_timestamp_ = timestamp;
  }

  const base::Time& last_policy_timestamp() { return last_policy_timestamp_; }

  void set_public_key_version(int public_key_version) {
    public_key_version_ = public_key_version;
    public_key_version_valid_ = true;
  }

  void clear_public_key_version() {
    public_key_version_valid_ = false;
  }

  // FetchPolicy() calls will request this policy type.
  // If |settings_entity_id| is empty then it won't be set in the
  // PolicyFetchRequest.
  void AddPolicyTypeToFetch(const std::string& policy_type,
                            const std::string& settings_entity_id);

  // FetchPolicy() calls won't request the given policy type and optional
  // |settings_entity_id| anymore.
  void RemovePolicyTypeToFetch(const std::string& policy_type,
                               const std::string& settings_entity_id);

  // Configures a set of device state keys to transfer to the server in the next
  // policy fetch. If the fetch is successful, the keys will be cleared so they
  // are only uploaded once.
  void SetStateKeysToUpload(const std::vector<std::string>& keys);

  // Whether the client is registered with the device management service.
  bool is_registered() const { return !dm_token_.empty(); }

  // Whether the client requires reregistration with the device management
  // service.
  bool requires_reregistration() const {
    return !reregistration_dm_token_.empty();
  }

  DeviceManagementService* service() { return service_; }
  const std::string& dm_token() const { return dm_token_; }
  const std::string& client_id() const { return client_id_; }
  const base::DictionaryValue* configuration_seed() const {
    return configuration_seed_.get();
  }

  // The device mode as received in the registration request.
  DeviceMode device_mode() const { return device_mode_; }

  // The policy responses as obtained by the last request to the cloud. These
  // policies haven't gone through verification, so their contents cannot be
  // trusted. Use CloudPolicyStore::policy() and CloudPolicyStore::policy_map()
  // instead for making policy decisions.
  const ResponseMap& responses() const {
    return responses_;
  }

  // Returns the policy response for the (|policy_type|, |settings_entity_id|)
  // pair if found in |responses()|. Otherwise returns nullptr.
  const enterprise_management::PolicyFetchResponse* GetPolicyFor(
      const std::string& policy_type,
      const std::string& settings_entity_id) const;

  DeviceManagementStatus status() const {
    return status_;
  }

  // Returns the invalidation version that was used for the last FetchPolicy.
  // Observers can call this method from their OnPolicyFetched method to
  // determine which at which invalidation version the policy was fetched.
  int64_t fetched_invalidation_version() const {
    return fetched_invalidation_version_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Returns the number of active requests.
  int GetActiveRequestCountForTest() const;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

 protected:
  // A set of (policy type, settings entity ID) pairs to fetch.
  typedef std::set<std::pair<std::string, std::string>> PolicyTypeSet;

  // Upload a certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  void UploadCertificate(
      const std::string& certificate_data,
      enterprise_management::DeviceCertUploadRequest::CertificateType
          certificate_type,
      const StatusCallback& callback);

  // Callback for siganture of requests.
  void OnRegisterWithCertificateRequestSigned(
      std::unique_ptr<DMAuth> auth,
      bool success,
      enterprise_management::SignedData signed_data);

  // Callback for registration requests.
  void OnRegisterCompleted(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for policy fetch requests.
  void OnPolicyFetchCompleted(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for robot account api authorization requests.
  void OnFetchRobotAuthCodesCompleted(
      RobotAuthCodeCallback callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for unregistration requests.
  void OnUnregisterCompleted(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for certificate upload requests.
  void OnCertificateUploadCompleted(
      const StatusCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for several types of status/report upload requests.
  void OnReportUploadCompleted(
      const StatusCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for realtime report upload requests.
  void OnRealtimeReportUploadCompleted(const StatusCallback& callback,
                                       DeviceManagementService::Job* job,
                                       DeviceManagementStatus status,
                                       int net_error,
                                       const base::Value& response);

  // Callback for remote command fetch requests.
  void OnRemoteCommandsFetched(
      RemoteCommandCallback callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for device attribute update permission requests.
  void OnDeviceAttributeUpdatePermissionCompleted(
      const StatusCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for device attribute update requests.
  void OnDeviceAttributeUpdated(
      const StatusCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for available license types request.
  void OnAvailableLicensesRequested(
      const LicenseRequestCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for gcm id update requests.
  void OnGcmIdUpdated(
      const StatusCallback& callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Helper to remove a job from request_jobs_.
  void RemoveJob(DeviceManagementService::Job* job);

  // Observer notification helpers.
  void NotifyPolicyFetched();
  void NotifyRegistrationStateChanged();
  void NotifyClientError();

  // Data necessary for constructing policy requests.
  const std::string machine_id_;
  const std::string machine_model_;
  const std::string brand_code_;
  const std::string ethernet_mac_address_;
  const std::string dock_mac_address_;
  const std::string manufacture_date_;
  PolicyTypeSet types_to_fetch_;
  std::vector<std::string> state_keys_to_upload_;

  // OAuth token that if set is used as an additional form of authentication
  // (next to |dm_token_|) in policy fetch requests.
  std::string oauth_token_;

  std::string dm_token_;
  std::unique_ptr<base::DictionaryValue> configuration_seed_;
  DeviceMode device_mode_ = DEVICE_MODE_NOT_SET;
  std::string client_id_;
  base::Time last_policy_timestamp_;
  int public_key_version_ = -1;
  bool public_key_version_valid_ = false;
  // Device DMToken for affiliated user policy requests.
  // Retrieved from |device_dm_token_callback_| on registration.
  std::string device_dm_token_;

  // Information for the latest policy invalidation received.
  int64_t invalidation_version_ = 0;
  std::string invalidation_payload_;

  // The invalidation version used for the most recent fetch operation.
  int64_t fetched_invalidation_version_ = 0;

  // Used for issuing requests to the cloud.
  DeviceManagementService* service_ = nullptr;

  // Used for signing requests.
  SigningService* signing_service_ = nullptr;

  // Only one outstanding policy fetch is allowed, so this is tracked in
  // its own member variable.
  std::unique_ptr<DeviceManagementService::Job> policy_fetch_request_job_;

  // All of the outstanding non-policy-fetch request jobs. These jobs are
  // silently cancelled if Unregister() is called.
  std::vector<std::unique_ptr<DeviceManagementService::Job>> request_jobs_;

  // Only one outstanding app push-install report upload is allowed, and it must
  // be accessible so that it can be canceled.
  DeviceManagementService::Job* app_install_report_request_job_ = nullptr;

  // The policy responses returned by the last policy fetch operation.
  ResponseMap responses_;
  DeviceManagementStatus status_ = DM_STATUS_SUCCESS;

  DeviceDMTokenCallback device_dm_token_callback_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

 private:
  void SetClientId(const std::string& client_id);
  // Fills in the common fields of a DeviceRegisterRequest for |Register| and
  // |RegisterWithCertificate|.
  void CreateDeviceRegisterRequest(
      const RegistrationParameters& params,
      const std::string& client_id,
      enterprise_management::DeviceRegisterRequest* request);

  // Prepare the certificate upload request field for uploading a certificate.
  void PrepareCertUploadRequest(
      DMServerJobConfiguration* config,
      const std::string& certificate_data,
      enterprise_management::DeviceCertUploadRequest::CertificateType
          certificate_type);

  // Creates a job config to upload a certificate.
  std::unique_ptr<DMServerJobConfiguration> CreateCertUploadJobConfiguration(
      const CloudPolicyClient::StatusCallback& callback);

  // Executes a job to upload a certificate. Onwership of the job is
  // retained by this method.
  void ExecuteCertUploadJob(std::unique_ptr<DMServerJobConfiguration> config);

  // Used to store a copy of the previously used |dm_token_|. This is used
  // during re-registration, which gets triggered by a failed policy fetch with
  // error |DM_STATUS_SERVICE_DEVICE_NOT_FOUND|.
  std::string reregistration_dm_token_;

  // Used to create tasks which run delayed on the UI thread.
  base::WeakPtrFactory<CloudPolicyClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyClient);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
