// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class ClientDataDelegate;
class DMServerJobConfiguration;
class RegistrationJobConfiguration;
class SigningService;
struct DMServerJobResult;

inline constexpr char kPolicyFetchingTimeHistogramName[] =
    "Enterprise.CloudManagement.PolicyFetchingTime";

POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyFetchWithSha256);

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
  using ResponseMap = std::map<std::pair<std::string, std::string>,
                               enterprise_management::PolicyFetchResponse>;

  // A callback which receives boolean status of an operation. If the
  // operation succeeded, |status| is true.
  // TODO(b/256553070) Use `ResultCallback` instead of `StatusCallback`
  // everywhere.
  using StatusCallback = base::OnceCallback<void(bool status)>;

  // A callback which receives fetched remote commands.
  using RemoteCommandCallback = base::OnceCallback<void(
      DeviceManagementStatus,
      const std::vector<enterprise_management::SignedData>&)>;

  // A callback for fetching device robot OAuth2 authorization tokens.
  // Only occurs during enrollment, after the device is registered.
  using RobotAuthCodeCallback =
      base::OnceCallback<void(DeviceManagementStatus, const std::string&)>;

  // A callback which fetches device dm_token based on user affiliation.
  // Should be called once per registration.
  using DeviceDMTokenCallback = base::RepeatingCallback<std::string(
      const std::vector<std::string>& user_affiliation_ids)>;

  using ClientCertProvisioningRequestCallback = base::OnceCallback<void(
      DeviceManagementStatus,
      const enterprise_management::ClientCertificateProvisioningResponse&
          response)>;

  using MacAddress = std::array<uint8_t, 6>;

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

    // Called when the Service Account Identity is set on a policy data object
    // after a policy fetch. |service_account_email()| will return the new
    // account's email.
    virtual void OnServiceAccountSet(CloudPolicyClient* client,
                                     const std::string& account_email) {}
  };

  using NotRegistered = absl::monostate;

  class POLICY_EXPORT Result {
   public:
    explicit Result(DeviceManagementStatus);
    explicit Result(DeviceManagementStatus, int);
    explicit Result(NotRegistered);

    bool IsSuccess() const;
    bool IsClientNotRegisteredError() const;
    bool IsDMServerError() const;

    DeviceManagementStatus GetDMServerError() const;
    int GetNetError() const;

    bool operator==(const Result& other) const {
      return this->result_ == other.result_ && net_error_ == other.net_error_;
    }

   private:
    absl::variant<NotRegistered, DeviceManagementStatus> result_;
    int net_error_ = 0;
  };

  // A callback which receives the operations result.
  using ResultCallback = base::OnceCallback<void(Result)>;

  struct POLICY_EXPORT RegistrationParameters {
   public:
    RegistrationParameters(
        enterprise_management::DeviceRegisterRequest::Type registration_type,
        enterprise_management::DeviceRegisterRequest::Flavor flavor);
    ~RegistrationParameters();

    enterprise_management::DeviceRegisterRequest::Type registration_type;
    enterprise_management::DeviceRegisterRequest::Flavor flavor;

    std::optional<enterprise_management::LicenseType_LicenseTypeEnum>
        license_type;

    // Lifetime of registration. Used for easier clean up of ephemeral session
    // registrations.
    enterprise_management::DeviceRegisterRequest::Lifetime lifetime =
        enterprise_management::DeviceRegisterRequest::LIFETIME_INDEFINITE;

    // Device requisition.
    std::string requisition;

    // Server-backed state keys (used for forced enrollment check).
    std::string current_state_key;

    // The following field is relevant only to Chrome OS.
    // PSM protocol execution result. Its value will exist if the device
    // undergoes enrollment and a PSM server-backed state determination was
    // performed before (on Chrome OS, as encoded in the
    // `prefs::kEnrollmentPsmResult` pref).
    std::optional<
        enterprise_management::DeviceRegisterRequest::PsmExecutionResult>
        psm_execution_result;

    // The following field is relevant only to Chrome OS.
    // PSM protocol determination timestamp. Its value will exist if the device
    // undergoes enrollment and PSM got executed successfully (on ChromeOS, as
    // encoded in `prefs::kEnrollmentPsmDeterminationTime` pref).
    std::optional<int64_t> psm_determination_timestamp;

    // The following field is relevant only to Chrome OS Demo Mode.
    // Information about demo-specific device attributes and retail context.
    // This value will only exist if the enrollment requisition is
    // kDemoRequisition ("cros-demo-mode").
    std::optional<enterprise_management::DemoModeDimensions>
        demo_mode_dimensions;

    // The following field is relevant only to Browsers undergoing profile
    // registration via the generic OIDC, and contains OIDC specific state
    // details.
    std::string oidc_state;
  };

  // If non-empty, |machine_id|, |machine_model|, |brand_code|,
  // |attested_device_id|, |ethernet_mac_address|, |dock_mac_address| and
  // |manufacture_date| are passed to the server verbatim. As these reveal
  // machine identity, they must only be used where this is appropriate (i.e.
  // device policy, but not user policy). |service| is weak pointer and it's
  // the caller's responsibility to keep it valid for the lifetime of
  // CloudPolicyClient. |device_dm_token_callback| is used to retrieve device
  // DMToken for affiliated users. Could be null if it's not possible to use
  // device DMToken for user policy fetches.
  CloudPolicyClient(
      std::string_view machine_id,
      std::string_view machine_model,
      std::string_view brand_code,
      std::string_view attested_device_id,
      std::optional<MacAddress> ethernet_mac_address,
      std::optional<MacAddress> dock_mac_address,
      std::string_view manufacture_date,
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceDMTokenCallback device_dm_token_callback);
  // Create CloudPolicyClient for Profile with its `profile_id`.
  CloudPolicyClient(
      const std::string& profile_id,
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceDMTokenCallback device_dm_token_callback);
  // A simpler constructor for those that do not need any of the identification
  // strings of the full constructor.
  CloudPolicyClient(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceDMTokenCallback device_dm_token_callback);
  CloudPolicyClient(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudPolicyClient(const CloudPolicyClient&) = delete;
  CloudPolicyClient& operator=(const CloudPolicyClient&) = delete;

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
  // error notification. The |signing_service| is used to sign the request and
  // is expected to be available until caller receives
  // |OnRegistrationStateChanged| or |OnClientError|.
  // TODO(crbug.com/40192631): Remove SigningService from CloudPolicyClient and
  // make callees sign their data themselves.
  virtual void RegisterWithCertificate(
      const RegistrationParameters& parameters,
      const std::string& client_id,
      const std::string& pem_certificate_chain,
      const std::string& sub_organization,
      std::unique_ptr<SigningService> signing_service);

  // Attempts to enroll a browser with the device management service using an
  // enrollment token. Results in a registration change or error notification.
  // To emphasize, this method is used to register browser (e.g. for
  // machine-level policies).
  // Device registration with enrollment token should be performed using
  // RegisterWithEnrollmentToken method, and this request will timeout after 30
  // seconds if the enrollment is not mandatory.
  virtual void RegisterBrowserWithEnrollmentToken(
      const std::string& token,
      const std::string& client_id,
      const ClientDataDelegate& client_data_delegate,
      bool is_mandatory);

  // Attempts to enroll with the device management service using an enrollment
  // token. Results in a registration change or error notification.
  //
  // This method is used to register a ChromeOS device (currently only used for
  // ChromeOS Flex Auto Enrollment). Browser registration should be performed
  // using RegisterWithToken.
  virtual void RegisterDeviceWithEnrollmentToken(
      const RegistrationParameters& parameters,
      const std::string& client_id,
      DMAuth enrollment_token_auth);

  // Attempts to enroll a policy agent, (i.e. Omaha, Keystone, or the Chrome
  // Enterprise Companion App) with the device management service using an
  // enrollment token. Results in a registration change or error notification.
  // To emphasize, this method is used to register browser (e.g. for
  // machine-level policies).
  virtual void RegisterPolicyAgentWithEnrollmentToken(
      const std::string& token,
      const std::string& client_id,
      const ClientDataDelegate& client_data_delegate);

  // Attempts to register the profile with the device management service using a
  // OIDC response from a third party IdP's authentication. Results in a
  // registration change or error notification.
  virtual void RegisterWithOidcResponse(
      const RegistrationParameters& parameters,
      const std::string& oauth_token,
      const std::string& oidc_id_token,
      const std::string& client_id,
      const base::TimeDelta& timeout_duration,
      ResultCallback callback);

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
  // The |reason| parameter will be used to tag the request to DMServer. This
  // will allow for more targeted monitoring and alerting.
  virtual void FetchPolicy(PolicyFetchReason reason);

  // Upload a policy validation report to the server. Like FetchPolicy, this
  // method requires that the client is in a registered state. This method
  // should only be called if the policy was rejected (e.g. validation or
  // serialization error).
  virtual void UploadPolicyValidationReport(
      CloudPolicyValidatorBase::Status status,
      const std::vector<ValueValidationIssue>& value_validation_issues,
      const ValidationAction action,
      const std::string& policy_type,
      const std::string& policy_token);

  // Requests OAuth2 auth codes for the device robot account. The client being
  // registered is a prerequisite to this operation and this call will CHECK if
  // the client is not in registered state. |oauth_scopes| is the scopes for
  // which the robot auth codes will be valid, and |device_type| should match
  // the type of the robot account associated with this request.
  // The |callback| will be called when the operation completes.
  virtual void FetchRobotAuthCodes(
      DMAuth auth,
      enterprise_management::DeviceServiceApiAccessRequest::DeviceType
          device_type,
      const std::set<std::string>& oauth_scopes,
      RobotAuthCodeCallback callback);

  // Upload a machine certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  virtual void UploadEnterpriseMachineCertificate(
      const std::string& certificate_data,
      ResultCallback callback);

  // Upload an enrollment certificate to the server.  Like FetchPolicy, this
  // method requires that the client is in a registered state.
  // |certificate_data| must hold the X.509 certificate data to be sent to the
  // server.  The |callback| will be called when the operation completes.
  virtual void UploadEnterpriseEnrollmentCertificate(
      const std::string& certificate_data,
      ResultCallback callback);

  // Upload an enrollment identifier to the server. Like FetchPolicy, this
  // method requires that the client is in a registered state.
  // |enrollment_id| must hold an enrollment identifier. The |callback| will be
  // called when the operation completes.
  virtual void UploadEnterpriseEnrollmentId(const std::string& enrollment_id,
                                            ResultCallback callback);

  // Uploads status to the server. The client must be in a registered state.
  // Only non-null statuses will be included in the upload status request. The
  // |callback| will be called when the operation completes.
  virtual void UploadDeviceStatus(
      const enterprise_management::DeviceStatusReportRequest* device_status,
      const enterprise_management::SessionStatusReportRequest* session_status,
      const enterprise_management::ChildStatusReportRequest* child_status,
      ResultCallback callback);

  // Uploads Chrome Desktop report to the server. As above, the client must be
  // in a registered state. |chrome_desktop_report| will be included in the
  // upload request. The |callback| will be called when the operation completes.
  virtual void UploadChromeDesktopReport(
      std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
          chrome_desktop_report,
      ResultCallback callback);

  // Uploads Chrome OS User report to the server. The user's DM token must be
  // set. |chrome_os_user_report| will be included in the upload request. The
  // |callback| will be called when the operation completes.
  virtual void UploadChromeOsUserReport(
      std::unique_ptr<enterprise_management::ChromeOsUserReportRequest>
          chrome_os_user_report,
      ResultCallback callback);

  // Uploads Chrome profile report to the server. The user's DM token must be
  // set. |chrome_profile_report| will be included in the upload request. The
  // |callback| will be called when the operation completes.
  virtual void UploadChromeProfileReport(
      std::unique_ptr<enterprise_management::ChromeProfileReportRequest>
          chrome_profile_report,
      ResultCallback callback);

  // Uploads a report containing enterprise connectors real-time security
  // events to the server. As above, the client must be in a registered state.
  // If |include_device_info| is true, information specific to the device such
  // as the device name, user, id and OS will be included in the report. The
  // |callback| will be called when the operation completes.
  virtual void UploadSecurityEventReport(bool include_device_info,
                                         base::Value::Dict report,
                                         ResultCallback callback);

  // Uploads a report on the status of app push-installs. The client must be in
  // a registered state. The |callback| will be called when the operation
  // completes.
  // Only one outstanding app push-install report upload is allowed.
  // In case the new push-installs report upload is started, the previous one
  // will be canceled.
  virtual void UploadAppInstallReport(base::Value::Dict report,
                                      ResultCallback callback);

  // Cancels the pending app push-install status report upload, if exists.
  virtual void CancelAppInstallReportUpload();

  // Attempts to fetch remote commands, with `last_command_id` being the ID of
  // the last command that finished execution, `command_results` being
  // results for previous commands which have not been reported yet,
  // `signature_type` being a security signature type that the server will use
  // to sign the remote commands and `request_type` being the type of the fetch
  // request. The |callback| will be called when the operation completes.
  // Note that sending |last_command_id| will acknowledge this command and any
  // previous commands. A nullptr indicates that no commands have finished
  // execution.
  virtual void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<enterprise_management::RemoteCommandResult>&
          command_results,
      enterprise_management::PolicyFetchRequest::SignatureType signature_type,
      const std::string& request_type,
      RemoteCommandCallback callback);

  // Sends a device attribute update permission request to the server, uses
  // |auth| to identify user who requests a permission to name a device, calls
  // a |callback| from the enrollment screen to indicate whether the device
  // naming prompt should be shown.
  void GetDeviceAttributeUpdatePermission(DMAuth auth, StatusCallback callback);

  // Sends a device naming information (Asset Id and Location) to the
  // device management server, uses |auth| to identify user who names a device,
  // the |callback| will be called when the operation completes.
  void UpdateDeviceAttributes(DMAuth auth,
                              const std::string& asset_id,
                              const std::string& location,
                              StatusCallback callback);

  // Sends a GCM id update request to the DM server. The server will
  // associate the DM token in authorization header with |gcm_id|, and
  // |callback| will be called when the operation completes.
  virtual void UpdateGcmId(const std::string& gcm_id, StatusCallback callback);

  // Sends a request with EUICCs on device to the DM server.
  virtual void UploadEuiccInfo(
      std::unique_ptr<enterprise_management::UploadEuiccInfoRequest> request,
      StatusCallback callback);

  // Fills `request.device_dm_token` if available.
  virtual void ClientCertProvisioningRequest(
      enterprise_management::ClientCertificateProvisioningRequest request,
      ClientCertProvisioningRequestCallback callback);

  // Sends a request to store FM registration token used for invalidations.
  virtual void UploadFmRegistrationToken(
      enterprise_management::FmRegistrationTokenUploadRequest request,
      ResultCallback callback);

  // Used the update the current service account email associated with this
  // policy client and notify observers.
  void UpdateServiceAccount(const std::string& account_email);

  // Adds an observer to be called back upon policy and state changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  const std::string& machine_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return machine_id_;
  }
  const std::string& machine_model() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return machine_model_;
  }
  const std::string& brand_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return brand_code_;
  }
  const std::string& attested_device_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return attested_device_id_;
  }
  const std::string& ethernet_mac_address() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ethernet_mac_address_;
  }
  const std::string& dock_mac_address() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return dock_mac_address_;
  }
  const std::string& manufacture_date() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return manufacture_date_;
  }
  const std::string& oidc_user_display_name() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return oidc_user_display_name_;
  }
  const std::string& oidc_user_email() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return oidc_user_email_;
  }
  // TODO(326063101): Replace boolean with an enum, same as
  // policy::ThirdPartyIdentityType
  bool is_dasherless() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_dasherless_;
  }
  const std::vector<std::string>& user_affiliation_ids() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return user_affiliation_ids_;
  }

  void set_last_policy_timestamp(const base::Time& timestamp) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    last_policy_timestamp_ = timestamp;
  }

  const base::Time& last_policy_timestamp() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_policy_timestamp_;
  }

  void set_public_key_version(int public_key_version) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    public_key_version_ = public_key_version;
    public_key_version_valid_ = true;
  }

  void clear_public_key_version() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  bool is_registered() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !dm_token_.empty();
  }

  // Whether the client requires reregistration with the device management
  // service.
  bool requires_reregistration() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !reregistration_dm_token_.empty();
  }

  DeviceManagementService* service() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return service_;
  }
  const std::string& dm_token() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return dm_token_;
  }
  const std::string& client_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return client_id_;
  }
  const base::Value::Dict* configuration_seed() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return configuration_seed_.get();
  }

  // The device mode as received in the registration request.
  DeviceMode device_mode() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return device_mode_;
  }

  // The type of third party identity as received in the registration request.
  ThirdPartyIdentityType third_party_identity_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return third_party_identity_type_;
  }

  // The policy responses as obtained by the last request to the cloud. These
  // policies haven't gone through verification, so their contents cannot be
  // trusted. Use CloudPolicyStore::policy() and CloudPolicyStore::policy_map()
  // instead for making policy decisions.
  const ResponseMap& last_policy_fetch_responses() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_policy_fetch_responses_;
  }

  // Returns the policy response for the (|policy_type|, |settings_entity_id|)
  // pair if found in |responses()|. Otherwise returns nullptr.
  const enterprise_management::PolicyFetchResponse* GetPolicyFor(
      const std::string& policy_type,
      const std::string& settings_entity_id) const;

  DeviceManagementStatus last_dm_status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_dm_status_;
  }

  // Returns the invalidation version that was used for the last FetchPolicy.
  // Observers can call this method from their OnPolicyFetched method to
  // determine which at which invalidation version the policy was fetched.
  int64_t fetched_invalidation_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      ResultCallback callback);

  // This is called when a RegisterWithCertiifcate request has been signed.
  void OnRegisterWithCertificateRequestSigned(
      std::unique_ptr<SigningService> signing_service,
      bool success,
      enterprise_management::SignedData signed_data);

  // Callback for registration requests.
  void OnRegisterCompleted(DMServerJobResult result);

  // Callback for token-based device registration requests.
  void OnTokenBasedRegisterDeviceCompleted(DMServerJobResult result);

  // Callback for policy fetch requests. `start_time` is the timestamp of the
  // request creation, used for recording fetching time as a histogram.
  void OnPolicyFetchCompleted(base::Time start_time, DMServerJobResult result);

  // Callback for robot account api authorization requests.
  void OnFetchRobotAuthCodesCompleted(RobotAuthCodeCallback callback,
                                      DMServerJobResult result);

  // Callback for certificate upload requests.
  void OnCertificateUploadCompleted(ResultCallback callback,
                                    DMServerJobResult result);

  // Callback for several types of status/report upload requests.
  void OnReportUploadCompleted(ResultCallback callback,
                               DMServerJobResult result);

  // Callback for realtime report upload requests.
  void OnRealtimeReportUploadCompleted(
      ResultCallback callback,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      std::optional<base::Value::Dict> response);

  // Callback for remote command fetch requests.
  void OnRemoteCommandsFetched(RemoteCommandCallback callback,
                               DMServerJobResult result);

  // Callback for device attribute update permission requests.
  void OnDeviceAttributeUpdatePermissionCompleted(StatusCallback callback,
                                                  DMServerJobResult result);

  // Callback for device attribute update requests.
  void OnDeviceAttributeUpdated(StatusCallback callback,
                                DMServerJobResult result);

  // Callback for gcm id update requests.
  void OnGcmIdUpdated(StatusCallback callback, DMServerJobResult result);

  // Callback for EUICC info upload requests.
  void OnEuiccInfoUploaded(StatusCallback callback, DMServerJobResult result);

  // Callback for certificate provisioning requests.
  void OnClientCertProvisioningRequestResponse(
      ClientCertProvisioningRequestCallback callback,
      DMServerJobResult result);

  // Callback for `UploadFmRegistrationToken` request.
  void OnUploadFmRegistrationTokenResponse(ResultCallback callback,
                                           DMServerJobResult result);

  // Helper to remove a job from request_jobs_.
  void RemoveJob(const DeviceManagementService::Job* job);

  // Observer notification helpers.
  void NotifyPolicyFetched();
  void NotifyRegistrationStateChanged();
  void NotifyClientError();
  void NotifyServiceAccountSet(const std::string& account_email);

  // Assert non-concurrent usage in debug builds.
  SEQUENCE_CHECKER(sequence_checker_);

  // Data necessary for constructing policy requests.
  const std::string machine_id_;
  const std::string machine_model_;
  const std::string brand_code_;
  const std::string attested_device_id_;
  const std::string ethernet_mac_address_;
  const std::string dock_mac_address_;
  const std::string manufacture_date_;

  // Specific fields for oidc registration responses.
  std::string oidc_user_display_name_;
  std::string oidc_user_email_;
  bool is_dasherless_ = false;

  PolicyTypeSet types_to_fetch_;
  std::vector<std::string> state_keys_to_upload_;

  // OAuth token that if set is used as an additional form of authentication
  // (next to |dm_token_|) in policy fetch requests.
  std::string oauth_token_;

  std::string dm_token_;
  std::unique_ptr<base::Value::Dict> configuration_seed_;
  DeviceMode device_mode_ = DEVICE_MODE_NOT_SET;
  ThirdPartyIdentityType third_party_identity_type_ = NO_THIRD_PARTY_MANAGEMENT;
  std::string client_id_;
  std::optional<std::string> profile_id_;
  base::Time last_policy_timestamp_;
  int public_key_version_ = -1;
  bool public_key_version_valid_ = false;
  // Device DMToken for affiliated user policy requests.
  // Retrieved from |device_dm_token_callback_| on registration.
  std::string device_dm_token_;

  // A list of user affiliation ids, provided during setup or after
  // registration.
  std::vector<std::string> user_affiliation_ids_;

  // Information for the latest policy invalidation received.
  int64_t invalidation_version_ = 0;
  std::string invalidation_payload_;

  // The invalidation version used for the most recent fetch operation.
  int64_t fetched_invalidation_version_ = 0;

  // Used for issuing requests to the cloud.
  raw_ptr<DeviceManagementService> service_ = nullptr;

  // Only one outstanding policy fetch or device/user registration request is
  // allowed. Using a separate job to track those requests. If multiple
  // requests have been started, only the last one will be kept.
  std::unique_ptr<DeviceManagementService::Job> unique_request_job_;

  // All of the outstanding non-policy-fetch request jobs.
  std::vector<std::unique_ptr<DeviceManagementService::Job>> request_jobs_;

  // Only one outstanding app push-install report upload is allowed, and it must
  // be accessible so that it can be canceled.
  raw_ptr<DeviceManagementService::Job> app_install_report_request_job_ =
      nullptr;

  // Only one outstanding extension install report upload is allowed, and it
  // must be accessible so that it can be canceled.
  raw_ptr<DeviceManagementService::Job> extension_install_report_request_job_ =
      nullptr;

  // The policy responses returned by the last policy fetch operation. See
  // `ResponseMap` for information on the format.
  ResponseMap last_policy_fetch_responses_;
  DeviceManagementStatus last_dm_status_ = DM_STATUS_SUCCESS;

  DeviceDMTokenCallback device_dm_token_callback_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

 private:
  // Creates a new real-time reporting job and appends it to |request_jobs_|.
  // The job will send its report to the |server_url| endpoint.  If
  // |include_device_info| is true, information specific to the device such as
  // the device name, user, id and OS will be included in the report. If
  // |add_connector_url_params| is true then URL paramaters specific to
  // enterprise connectors are added to the request uploading the report.
  // |callback| is invoked once the report is uploaded.
  DeviceManagementService::Job* CreateNewRealtimeReportingJob(
      base::Value::Dict report,
      const std::string& server_url,
      bool include_device_info,
      ResultCallback callback);

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
      CloudPolicyClient::ResultCallback callback);

  // Creates a job config to upload a report.
  std::unique_ptr<DMServerJobConfiguration> CreateReportUploadJobConfiguration(
      DeviceManagementService::JobConfiguration::JobType type,
      CloudPolicyClient::ResultCallback callback);

  // Executes a job to upload a certificate. Onwership of the job is
  // retained by this method.
  void ExecuteCertUploadJob(std::unique_ptr<DMServerJobConfiguration> config);

  // Sets `unique_request_job_` with a new job created with `config`.
  void CreateUniqueRequestJob(
      std::unique_ptr<RegistrationJobConfiguration> config);

  // Shared logic for reading fields out of DeviceRegisterResponse and
  // notifying observers of the response status.
  void ProcessDeviceRegisterResponse(
      const enterprise_management::DeviceRegisterResponse& response,
      DeviceManagementStatus dm_status);

  // Records the fetch status for each supported type to fetch used by the
  // client.
  void RecordFetchStatus(DeviceManagementStatus status);

  enterprise_management::PolicyFetchRequest::SignatureType
  GetPolicyFetchRequestSignatureType();

  // Fills a request and creates a job for browser or policy agent enrollment,
  // which differ only by request type.
  virtual void RegisterBrowserOrPolicyAgentWithEnrollmentToken(
      const std::string& token,
      const std::string& client_id,
      const ClientDataDelegate& client_data_delegate,
      bool is_mandatory,
      DeviceManagementService::JobConfiguration::JobType type);

#if BUILDFLAG(IS_WIN)
  // Callback to get browser device identifier.
  void SetBrowserDeviceIdentifier(
      enterprise_management::PolicyFetchRequest* request,
      std::unique_ptr<DMServerJobConfiguration> config,
      std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
          identifier);
#endif

  // Used to store a copy of the previously used `dm_token_`. This is used
  // during re-registration, which gets triggered by a failed policy fetch
  // with errors `DM_STATUS_SERVICE_DEVICE_NOT_FOUND` and
  // `DM_STATUS_SERVICE_DEVICE_NEEDS_RESET`.
  std::string reregistration_dm_token_;

  // Used to create tasks which run delayed on the UI thread.
  base::WeakPtrFactory<CloudPolicyClient> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
