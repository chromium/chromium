// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

using testing::_;
using testing::Contains;
using testing::DoAll;
using testing::ElementsAre;
using testing::Invoke;
using testing::Key;
using testing::Mock;
using testing::Not;
using testing::Pair;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

// Matcher for std::optional. Can be combined with Not().
MATCHER(HasValue, "Has value") {
  return arg.has_value();
}

namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {

constexpr char kClientID[] = "fake-client-id";
constexpr char kMachineID[] = "fake-machine-id";
constexpr char kMachineModel[] = "fake-machine-model";
constexpr char kBrandCode[] = "fake-brand-code";
constexpr char kAttestedDeviceId[] = "fake-attested-device-id";
constexpr CloudPolicyClient::MacAddress kEthernetMacAddress = {0, 1, 2,
                                                               3, 4, 5};
constexpr char kEthernetMacAddressStr[] = "000102030405";
constexpr CloudPolicyClient::MacAddress kDockMacAddress = {170, 187, 204,
                                                           221, 238, 255};
constexpr char kDockMacAddressStr[] = "AABBCCDDEEFF";
constexpr char kManufactureDate[] = "fake-manufacture-date";
constexpr char kOAuthToken[] = "fake-oauth-token";
constexpr char kDMToken[] = "fake-dm-token";
constexpr char kDeviceDMToken[] = "fake-device-dm-token";
constexpr char kMachineCertificate[] = "fake-machine-certificate";
constexpr char kEnrollmentCertificate[] = "fake-enrollment-certificate";
constexpr char kEnrollmentId[] = "fake-enrollment-id";
constexpr char kOsName[] = "fake-os-name";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr char kIdToken[] = "id_token";
constexpr char kOidcState[] = "fake-oidc-state";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
constexpr char kBrowserEnrollmentToken[] = "browser_enrollment_token";
#endif

constexpr char kFlexEnrollmentToken[] = "flex_enrollment_token";

constexpr char kRequisition[] = "fake-requisition";
constexpr char kStateKey[] = "fake-state-key";
constexpr char kPayload[] = "input_payload";
constexpr char kResultPayload[] = "output_payload";
constexpr char kAssetId[] = "fake-asset-id";
constexpr char kLocation[] = "fake-location";
constexpr char kGcmID[] = "fake-gcm-id";
constexpr char kPolicyToken[] = "fake-policy-token";
constexpr char kPolicyName[] = "fake-policy-name";
constexpr char kValueValidationMessage[] = "fake-value-validation-message";
constexpr char kRobotAuthCode[] = "fake-robot-auth-code";
constexpr char kApiAuthScope[] = "fake-api-auth-scope";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr base::TimeDelta kDefaultOidcRegistrationTimeout = base::Seconds(30);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

constexpr int64_t kAgeOfCommand = 123123123;
constexpr int64_t kLastCommandId = 123456789;
constexpr int64_t kTimestamp = 987654321;

constexpr em::PolicyFetchRequest::SignatureType
    kRemoteCommandsFetchSignatureType = em::PolicyFetchRequest::SHA256_RSA;

constexpr PolicyFetchReason kReason = PolicyFetchReason::kTest;
constexpr auto kProtoReason = enterprise_management::DevicePolicyRequest::TEST;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

// A mock class to allow us to set expectations on result callbacks.
struct MockResultCallbackObserver {
  MOCK_METHOD(void, OnCallbackComplete, (CloudPolicyClient::Result));
};

// A mock class to allow us to set expectations on status callbacks.
struct MockStatusCallbackObserver {
  MOCK_METHOD(void, OnCallbackComplete, (bool));
};

// A mock class to allow us to set expectations on remote command fetch
// callbacks.
struct MockRemoteCommandsObserver {
  MOCK_METHOD(void,
              OnRemoteCommandsFetched,
              (DeviceManagementStatus, const std::vector<em::SignedData>&));
};

struct MockDeviceDMTokenCallbackObserver {
  MOCK_METHOD(std::string,
              OnDeviceDMTokenRequested,
              (const std::vector<std::string>&));
};

struct MockRobotAuthCodeCallbackObserver {
  MOCK_METHOD(void,
              OnRobotAuthCodeFetched,
              (DeviceManagementStatus, const std::string&));
};

struct MockResponseCallbackObserver {
  MOCK_METHOD(void, OnResponseReceived, (std::optional<base::Value::Dict>));
};

class FakeClientDataDelegate : public ClientDataDelegate {
 public:
  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override {
    request->set_os_platform(GetOSPlatform());
    request->set_os_version(GetOSVersion());

    std::move(callback).Run();
  }
};

std::string CreatePolicyData(const std::string& policy_value) {
  em::PolicyData policy_data;
  policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_data.set_policy_value(policy_value);
  return policy_data.SerializeAsString();
}

em::DeviceManagementRequest GetPolicyRequest() {
  em::DeviceManagementRequest policy_request;

  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_fetch_request->set_signature_type(em::PolicyFetchRequest::SHA256_RSA);
  policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);
  policy_fetch_request->set_device_dm_token(kDeviceDMToken);
  policy_request.mutable_policy_request()->set_reason(kProtoReason);

  return policy_request;
}

em::DeviceManagementResponse GetPolicyResponse() {
  em::DeviceManagementResponse policy_response;
  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("fake-policy-data"));
  return policy_response;
}

em::DeviceManagementRequest GetRegistrationRequest() {
  em::DeviceManagementRequest request;

  em::DeviceRegisterRequest* register_request =
      request.mutable_register_request();
  register_request->set_type(em::DeviceRegisterRequest::USER);
  register_request->set_machine_id(kMachineID);
  register_request->set_machine_model(kMachineModel);
  register_request->set_brand_code(kBrandCode);
  register_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  register_request->set_ethernet_mac_address(kEthernetMacAddressStr);
  register_request->set_dock_mac_address(kDockMacAddressStr);
  register_request->set_manufacture_date(kManufactureDate);
  register_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  register_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);

  return request;
}

em::DeviceManagementResponse GetRegistrationResponse() {
  em::DeviceManagementResponse registration_response;
  registration_response.mutable_register_response()
      ->set_device_management_token(kDMToken);
  return registration_response;
}

em::DeviceManagementResponse GetTokenBasedRegistrationResponse() {
  em::DeviceManagementResponse registration_response;
  registration_response.mutable_token_based_device_register_response()
      ->mutable_device_register_response()
      ->set_device_management_token(kDMToken);
  return registration_response;
}

em::DeviceManagementRequest GetReregistrationRequest() {
  em::DeviceManagementRequest request;

  em::DeviceRegisterRequest* reregister_request =
      request.mutable_register_request();
  reregister_request->set_type(em::DeviceRegisterRequest::USER);
  reregister_request->set_machine_id(kMachineID);
  reregister_request->set_machine_model(kMachineModel);
  reregister_request->set_brand_code(kBrandCode);
  reregister_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  reregister_request->set_ethernet_mac_address(kEthernetMacAddressStr);
  reregister_request->set_dock_mac_address(kDockMacAddressStr);
  reregister_request->set_manufacture_date(kManufactureDate);
  reregister_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  reregister_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  reregister_request->set_reregister(true);
  reregister_request->set_reregistration_dm_token(kDMToken);

  return request;
}

em::DeviceManagementRequest GetTokenBasedDeviceRegistrationRequest() {
  em::DeviceManagementRequest request;
  em::DeviceRegisterRequest* register_request =
      request.mutable_token_based_device_register_request()
          ->mutable_device_register_request();
  register_request->set_type(em::DeviceRegisterRequest::DEVICE);
  register_request->set_machine_id(kMachineID);
  register_request->set_machine_model(kMachineModel);
  register_request->set_brand_code(kBrandCode);
  register_request->set_ethernet_mac_address(kEthernetMacAddressStr);
  register_request->set_dock_mac_address(kDockMacAddressStr);
  register_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  register_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);
  return request;
}

// Constructs the DeviceManagementRequest with
// CertificateBasedDeviceRegistrationData.
// Also, if |psm_execution_result| or |psm_determination_timestamp| has a value,
// then populate its corresponding PSM field in DeviceRegisterRequest.
em::DeviceManagementRequest GetCertBasedRegistrationRequest(
    FakeSigningService* fake_signing_service,
    std::optional<PsmExecutionResult> psm_execution_result,
    std::optional<int64_t> psm_determination_timestamp,
    const std::optional<em::DemoModeDimensions>& demo_mode_dimensions) {
  em::CertificateBasedDeviceRegistrationData data;
  data.set_certificate_type(em::CertificateBasedDeviceRegistrationData::
                                ENTERPRISE_ENROLLMENT_CERTIFICATE);
  data.set_device_certificate(kEnrollmentCertificate);

  em::DeviceRegisterRequest* register_request =
      data.mutable_device_register_request();
  register_request->set_type(em::DeviceRegisterRequest::DEVICE);
  register_request->set_machine_id(kMachineID);
  register_request->set_machine_model(kMachineModel);
  register_request->set_brand_code(kBrandCode);
  register_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  register_request->set_ethernet_mac_address(kEthernetMacAddressStr);
  register_request->set_dock_mac_address(kDockMacAddressStr);
  register_request->set_manufacture_date(kManufactureDate);
  register_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  register_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  if (psm_determination_timestamp.has_value()) {
    register_request->set_psm_determination_timestamp_ms(
        psm_determination_timestamp.value());
  }
  if (psm_execution_result.has_value()) {
    register_request->set_psm_execution_result(psm_execution_result.value());
  }
  if (demo_mode_dimensions.has_value()) {
    *register_request->mutable_demo_mode_dimensions() =
        demo_mode_dimensions.value();
  }

  em::DeviceManagementRequest request;

  em::CertificateBasedDeviceRegisterRequest* cert_based_register_request =
      request.mutable_certificate_based_register_request();
  fake_signing_service->SignDataSynchronously(
      data.SerializeAsString(),
      cert_based_register_request->mutable_signed_request());

  return request;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
em::DeviceManagementRequest GetEnrollmentRequest() {
  em::DeviceManagementRequest request;

  em::RegisterBrowserRequest* enrollment_request =
      request.mutable_register_browser_request();
  enrollment_request->set_os_platform(GetOSPlatform());
  enrollment_request->set_os_version(GetOSVersion());
  return request;
}
#endif

em::DeviceManagementRequest GetUploadMachineCertificateRequest() {
  em::DeviceManagementRequest upload_machine_certificate_request;
  upload_machine_certificate_request.mutable_cert_upload_request()
      ->set_device_certificate(kMachineCertificate);
  upload_machine_certificate_request.mutable_cert_upload_request()
      ->set_certificate_type(
          em::DeviceCertUploadRequest::ENTERPRISE_MACHINE_CERTIFICATE);
  return upload_machine_certificate_request;
}

em::DeviceManagementRequest GetUploadEnrollmentCertificateRequest() {
  em::DeviceManagementRequest upload_enrollment_certificate_request;
  upload_enrollment_certificate_request.mutable_cert_upload_request()
      ->set_device_certificate(kEnrollmentCertificate);
  upload_enrollment_certificate_request.mutable_cert_upload_request()
      ->set_certificate_type(
          em::DeviceCertUploadRequest::ENTERPRISE_ENROLLMENT_CERTIFICATE);
  return upload_enrollment_certificate_request;
}

em::DeviceManagementResponse GetUploadCertificateResponse() {
  em::DeviceManagementResponse upload_certificate_response;
  upload_certificate_response.mutable_cert_upload_response();
  return upload_certificate_response;
}

em::DeviceManagementRequest GetUploadStatusRequest() {
  em::DeviceManagementRequest upload_status_request;
  upload_status_request.mutable_device_status_report_request();
  upload_status_request.mutable_session_status_report_request();
  upload_status_request.mutable_child_status_report_request();
  return upload_status_request;
}

em::DeviceManagementRequest GetRemoteCommandRequest(
    em::PolicyFetchRequest::SignatureType signature_type) {
  em::DeviceManagementRequest remote_command_request;

  remote_command_request.mutable_remote_command_request()
      ->set_last_command_unique_id(kLastCommandId);
  em::RemoteCommandResult* command_result =
      remote_command_request.mutable_remote_command_request()
          ->add_command_results();
  command_result->set_command_id(kLastCommandId);
  command_result->set_result(em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  command_result->set_payload(kResultPayload);
  command_result->set_timestamp(kTimestamp);
  remote_command_request.mutable_remote_command_request()->set_signature_type(
      signature_type);
  remote_command_request.mutable_remote_command_request()
      ->set_send_secure_commands(true);
  remote_command_request.mutable_remote_command_request()->set_type(
      dm_protocol::kChromeUserRemoteCommandType);

  return remote_command_request;
}

em::DeviceManagementRequest GetRobotAuthCodeFetchRequest() {
  em::DeviceManagementRequest robot_auth_code_fetch_request;

  em::DeviceServiceApiAccessRequest* api_request =
      robot_auth_code_fetch_request.mutable_service_api_access_request();
  api_request->set_oauth2_client_id(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  api_request->add_auth_scopes(kApiAuthScope);
  api_request->set_device_type(em::DeviceServiceApiAccessRequest::CHROME_OS);

  return robot_auth_code_fetch_request;
}

em::DeviceManagementResponse GetRobotAuthCodeFetchResponse() {
  em::DeviceManagementResponse robot_auth_code_fetch_response;

  em::DeviceServiceApiAccessResponse* api_response =
      robot_auth_code_fetch_response.mutable_service_api_access_response();
  api_response->set_auth_code(kRobotAuthCode);

  return robot_auth_code_fetch_response;
}

em::DeviceManagementResponse GetFmRegistrationTokenUploadResponse() {
  em::DeviceManagementResponse fm_registration_token_upload_response;
  fm_registration_token_upload_response
      .mutable_fm_registration_token_upload_response();
  return fm_registration_token_upload_response;
}

em::DeviceManagementResponse GetEmptyResponse() {
  return em::DeviceManagementResponse();
}

em::DemoModeDimensions GetDemoModeDimensions() {
  em::DemoModeDimensions demo_mode_dimensions;
  demo_mode_dimensions.set_country("GB");
  demo_mode_dimensions.set_retailer_name("retailer");
  demo_mode_dimensions.set_store_number("1234");
  demo_mode_dimensions.add_customization_facets(
      em::DemoModeDimensions::FEATURE_AWARE_DEVICE);
  return demo_mode_dimensions;
}

}  // namespace

class CloudPolicyClientTest : public testing::Test {
 protected:
  explicit CloudPolicyClientTest(
      std::unique_ptr<base::test::TaskEnvironment> task_env)
      : task_environment_(std::move(task_env)),
        job_type_(DeviceManagementService::JobConfiguration::TYPE_INVALID),
        client_id_(kClientID),
        policy_type_(dm_protocol::kChromeUserPolicyType) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  "fake_serial_number");
#endif
  }

  CloudPolicyClientTest()
      : CloudPolicyClientTest(
            std::make_unique<base::test::SingleThreadTaskEnvironment>()) {}

  void SetUp() override { CreateClient(); }

  void TearDown() override { client_->RemoveObserver(&observer_); }

  void RegisterClient(const std::string& device_dm_token) {
    EXPECT_CALL(observer_, OnRegistrationStateChanged);
    EXPECT_CALL(device_dmtoken_callback_observer_,
                OnDeviceDMTokenRequested(
                    /*user_affiliation_ids=*/std::vector<std::string>()))
        .WillOnce(Return(device_dm_token));
    client_->SetupRegistration(kDMToken, client_id_,
                               std::vector<std::string>());
  }

  void RegisterClient() { RegisterClient(kDeviceDMToken); }

  void CreateClient() { CreateClient(kAttestedDeviceId, kManufactureDate); }

  // Flex devices don't have VPD so will not have attested device ID or
  // manufacture date.
  void CreateFlexClient() { CreateClient("", ""); }

  base::Value::Dict MakeDefaultRealtimeReport() {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent");
    context.SetByDottedPath("profile.profileName", "Profile 1");
    context.SetByDottedPath("profile.profilePath", "C:\\User Data\\Profile 1");

    base::Value::Dict event;
    event.Set("time", "2019-05-22T13:01:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List event_list;
    event_list.Append(std::move(event));
    return policy::RealtimeReportingJobConfiguration::BuildReport(
        std::move(event_list), std::move(context));
  }

  void ExpectAndCaptureJob(const em::DeviceManagementResponse& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                        service_.CaptureAuthData(&auth_data_),
                        service_.CaptureQueryParams(&query_params_),
                        service_.CaptureTimeout(&timeout_),
                        service_.CaptureRequest(&job_request_),
                        service_.SendJobOKAsync(response)));
  }

  void ExpectAndCaptureJSONJob(const std::string& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                        service_.CaptureAuthData(&auth_data_),
                        service_.CaptureQueryParams(&query_params_),
                        service_.CaptureTimeout(&timeout_),
                        service_.CapturePayload(&job_payload_),
                        service_.SendJobOKAsync(response)));
  }

  void ExpectAndCaptureJobReplyFailure(int net_error, int response_code) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(
            DoAll(service_.CaptureJobType(&job_type_),
                  service_.CaptureAuthData(&auth_data_),
                  service_.CaptureQueryParams(&query_params_),
                  service_.CaptureTimeout(&timeout_),
                  service_.CaptureRequest(&job_request_),
                  service_.SendJobResponseAsync(net_error, response_code)));
  }

  void CheckPolicyResponse(
      const em::DeviceManagementResponse& policy_response) {
    ASSERT_TRUE(client_->GetPolicyFor(policy_type_, std::string()));
    EXPECT_THAT(*client_->GetPolicyFor(policy_type_, std::string()),
                MatchProto(policy_response.policy_response().responses(0)));
  }

  void VerifyQueryParameter() {
#if !BUILDFLAG(IS_IOS)
    EXPECT_THAT(query_params_,
                Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
#endif
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  DeviceManagementService::JobConfiguration::JobType job_type_;
  DeviceManagementService::JobConfiguration::ParameterMap query_params_;
  DMAuth auth_data_;
  base::TimeDelta timeout_;
  em::DeviceManagementRequest job_request_;
  std::string job_payload_;
  std::string client_id_;
  std::string policy_type_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService service_{&job_creation_handler_};
  StrictMock<MockCloudPolicyClientObserver> observer_;
  StrictMock<MockResultCallbackObserver> result_callback_observer_;
  StrictMock<MockStatusCallbackObserver> status_callback_observer_;
  StrictMock<MockDeviceDMTokenCallbackObserver>
      device_dmtoken_callback_observer_;
  StrictMock<MockRobotAuthCodeCallbackObserver>
      robot_auth_code_callback_observer_;
  StrictMock<MockResponseCallbackObserver> response_callback_observer_;
  std::unique_ptr<CloudPolicyClient> client_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
 private:
  void CreateClient(std::string_view attested_device_id,
                    std::string_view manufacture_date) {
    service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    if (client_) {
      client_->RemoveObserver(&observer_);
    }

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    client_ = std::make_unique<CloudPolicyClient>(
        kMachineID, kMachineModel, kBrandCode, attested_device_id,
        kEthernetMacAddress, kDockMacAddress, manufacture_date, &service_,
        shared_url_loader_factory_,
        base::BindRepeating(
            &MockDeviceDMTokenCallbackObserver::OnDeviceDMTokenRequested,
            base::Unretained(&device_dmtoken_callback_observer_)));
    client_->AddPolicyTypeToFetch(policy_type_, std::string());
    client_->AddObserver(&observer_);
  }
};

// CloudPolicyClient tests that need multiple threads.
class CloudPolicyClientMultipleThreadsTest : public CloudPolicyClientTest {
 public:
  CloudPolicyClientMultipleThreadsTest()
      : CloudPolicyClientTest(std::make_unique<base::test::TaskEnvironment>()) {
  }
};

TEST_F(CloudPolicyClientTest, Init) {
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(0, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetch) {
  base::HistogramTester histogram_tester;

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  client_->SetupRegistration(kDMToken, client_id_, std::vector<std::string>());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus.UserPolicy",
      DM_STATUS_SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus", DM_STATUS_SUCCESS, 1);
}

class CloudPolicyClientWithFetchReasonTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<
          std::tuple<PolicyFetchReason,
                     enterprise_management::DevicePolicyRequest_Reason>> {
 public:
  PolicyFetchReason GetReason() const { return get<0>(GetParam()); }
  auto GetProtoReason() const { return get<1>(GetParam()); }
};

TEST_P(CloudPolicyClientWithFetchReasonTest, FetchReason) {
  base::HistogramTester histogram_tester;
  RegisterClient();
  ExpectAndCaptureJob(GetPolicyResponse());

  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(GetReason());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(job_request_.policy_request().reason(), GetProtoReason());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPolicyFetchingTimeHistogramName),
              ElementsAre(_));
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyClientWithReasonParams,
    CloudPolicyClientWithFetchReasonTest,
    ::testing::Values(
        std::tuple(PolicyFetchReason::kUnspecified,
                   enterprise_management::DevicePolicyRequest::UNSPECIFIED),
        std::tuple(PolicyFetchReason::kBrowserStart,
                   enterprise_management::DevicePolicyRequest::BROWSER_START),
        std::tuple(PolicyFetchReason::kCrdHostPolicyWatcher,
                   enterprise_management::DevicePolicyRequest::
                       CRD_HOST_POLICY_WATCHER),
        std::tuple(
            PolicyFetchReason::kDeviceEnrollment,
            enterprise_management::DevicePolicyRequest::DEVICE_ENROLLMENT),
        std::tuple(PolicyFetchReason::kInvalidation,
                   enterprise_management::DevicePolicyRequest::INVALIDATION),
        std::tuple(PolicyFetchReason::kLacros,
                   enterprise_management::DevicePolicyRequest::LACROS),
        std::tuple(
            PolicyFetchReason::kRegistrationChanged,
            enterprise_management::DevicePolicyRequest::REGISTRATION_CHANGED),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServiceActivationPending,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_SERVICE_ACTIVATION_PENDING),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServicePolicyNotFound,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_SERVICE_POLICY_NOT_FOUND),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServiceTooManyRequests,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_SERVICE_TOO_MANY_REQUESTS),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestFailed,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_REQUEST_FAILED),
        std::tuple(PolicyFetchReason::kRetryAfterStatusTemporaryUnavailable,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_TEMPORARY_UNAVAILABLE),
        std::tuple(PolicyFetchReason::kRetryAfterStatusCannotSignRequest,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_CANNOT_SIGN_REQUEST),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestInvalid,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_REQUEST_INVALID),
        std::tuple(PolicyFetchReason::kRetryAfterStatusHttpStatusError,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_HTTP_STATUS_ERROR),
        std::tuple(PolicyFetchReason::kRetryAfterStatusResponseDecodingError,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_RESPONSE_DECODING_ERROR),
        std::tuple(
            PolicyFetchReason::kRetryAfterStatusServiceManagementNotSupported,
            enterprise_management::DevicePolicyRequest::
                RETRY_AFTER_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestTooLarge,
                   enterprise_management::DevicePolicyRequest::
                       RETRY_AFTER_STATUS_REQUEST_TOO_LARGE),
        std::tuple(PolicyFetchReason::kScheduled,
                   enterprise_management::DevicePolicyRequest::SCHEDULED),
        std::tuple(PolicyFetchReason::kSignin,
                   enterprise_management::DevicePolicyRequest::SIGNIN),
        std::tuple(PolicyFetchReason::kTest,
                   enterprise_management::DevicePolicyRequest::TEST),
        std::tuple(PolicyFetchReason::kUserRequest,
                   enterprise_management::DevicePolicyRequest::USER_REQUEST)));

class CloudPolicyClientFetchPolicyCriticalTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<std::tuple<PolicyFetchReason, bool>> {
 public:
  PolicyFetchReason GetReason() const { return get<0>(GetParam()); }
  bool IsCriticalParamExpected() const { return get<1>(GetParam()); }

  base::expected<bool, std::string> HasCriticalQueryParam() const {
    const auto* critical_param =
        base::FindOrNull(query_params_, dm_protocol::kParamCritical);
    if (!critical_param) {
      return false;
    }
    if (*critical_param != "true") {
      return base::unexpected("invalid critical parameter: " + *critical_param);
    }
    return true;
  }
};

TEST_P(CloudPolicyClientFetchPolicyCriticalTest, FetchReasonIsCritical) {
  RegisterClient();
  ExpectAndCaptureJob(GetPolicyResponse());

  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(GetReason());
  base::RunLoop().RunUntilIdle();

  const auto has_critical_param = HasCriticalQueryParam();
  ASSERT_TRUE(has_critical_param.has_value()) << has_critical_param.error();
  EXPECT_EQ(has_critical_param, IsCriticalParamExpected());
}

// As of today, only policy fetches during device enrollment are considered
// critical (that was the initial purpose of the parameter). We might consider
// more fetch reasons critical in the future, but it would be odd to make all
// policy fetches critical.
INSTANTIATE_TEST_SUITE_P(
    CloudPolicyClientCriticalityParams,
    CloudPolicyClientFetchPolicyCriticalTest,
    ::testing::Values(
        std::tuple(PolicyFetchReason::kUnspecified, false),
        std::tuple(PolicyFetchReason::kBrowserStart, false),
        std::tuple(PolicyFetchReason::kCrdHostPolicyWatcher, false),
        std::tuple(PolicyFetchReason::kDeviceEnrollment, true),
        std::tuple(PolicyFetchReason::kInvalidation, false),
        std::tuple(PolicyFetchReason::kLacros, false),
        std::tuple(PolicyFetchReason::kRegistrationChanged, false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServiceActivationPending,
                   false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServicePolicyNotFound,
                   false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusServiceTooManyRequests,
                   false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestFailed, false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusTemporaryUnavailable,
                   false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusCannotSignRequest,
                   false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestInvalid, false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusHttpStatusError, false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusResponseDecodingError,
                   false),
        std::tuple(
            PolicyFetchReason::kRetryAfterStatusServiceManagementNotSupported,
            false),
        std::tuple(PolicyFetchReason::kRetryAfterStatusRequestTooLarge, false),
        std::tuple(PolicyFetchReason::kScheduled, false),
        std::tuple(PolicyFetchReason::kSignin, false),
        std::tuple(PolicyFetchReason::kTest, false),
        std::tuple(PolicyFetchReason::kUserRequest, false)));

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetchWithOAuthToken) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  client_->SetupRegistration(kDMToken, client_id_, std::vector<std::string>());
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
TEST_F(CloudPolicyClientTest, BrowserRegistrationWithTokenAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  FakeClientDataDelegate client_data_delegate;
  client_->RegisterBrowserWithEnrollmentToken(kBrowserEnrollmentToken,
                                              "device_id", client_data_delegate,
                                              /*is_mandatory=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_BROWSER_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetEnrollmentRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  EXPECT_EQ(base::Seconds(0), timeout_);

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, BrowserRegistrationWithTokenTestTimeout) {
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  FakeClientDataDelegate client_data_delegate;
  client_->RegisterBrowserWithEnrollmentToken(kBrowserEnrollmentToken,
                                              "device_id", client_data_delegate,
                                              /*is_mandatory=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::Seconds(30), timeout_);
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(CloudPolicyClientTest, RegistrationWithOidcAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->RegisterWithOidcResponse(
      register_user, kOAuthToken, kIdToken, std::string() /* no client_id*/,
      kDefaultOidcRegistrationTimeout,
      base::BindOnce([](CloudPolicyClient::Result result) {
        EXPECT_TRUE(result.IsSuccess());
        EXPECT_EQ(result.GetNetError(), net::OK);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_OIDC_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::FromOidcResponse(kIdToken));
  EXPECT_THAT(query_params_,
              Not(Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken))));
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(kDefaultOidcRegistrationTimeout, timeout_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, RegistrationWithOidcAndPolicyFetchWithOidcState) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();
  em::DeviceManagementRequest registration_request = GetRegistrationRequest();
  registration_request.mutable_register_request()
      ->set_oidc_profile_enrollment_state(kOidcState);
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));

  CloudPolicyClient::RegistrationParameters register_parameters(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  register_parameters.oidc_state = kOidcState;

  client_->RegisterWithOidcResponse(
      register_parameters, kOAuthToken, kIdToken,
      std::string() /* no client_id*/, kDefaultOidcRegistrationTimeout,
      base::BindOnce([](CloudPolicyClient::Result result) {
        EXPECT_TRUE(result.IsSuccess());
        EXPECT_EQ(result.GetNetError(), net::OK);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_OIDC_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            registration_request.SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::FromOidcResponse(kIdToken));
  EXPECT_THAT(query_params_,
              Not(Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken))));
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(kDefaultOidcRegistrationTimeout, timeout_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, OidcRegistrationFailure) {
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->RegisterWithOidcResponse(
      register_user, kOAuthToken, kIdToken, std::string() /* no client_id*/,
      kDefaultOidcRegistrationTimeout,
      base::BindOnce([](CloudPolicyClient::Result result) {
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_EQ(result.GetNetError(), net::ERR_FAILED);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_OIDC_REGISTRATION,
            job_type);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetch) {
  base::HistogramTester histogram_tester;

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(base::Seconds(0), timeout_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus.UserPolicy",
      DM_STATUS_SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus", DM_STATUS_SUCCESS, 1);
}

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetchWithOAuthToken) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, RegistrationWithCertificateAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  ExpectAndCaptureJob(GetRegistrationResponse());
  auto fake_signing_service = std::make_unique<FakeSigningService>();
  fake_signing_service->set_success(true);
  const std::string expected_job_request_string =
      GetCertBasedRegistrationRequest(
          fake_signing_service.get(),
          /*psm_execution_result=*/std::nullopt,
          /*psm_determination_timestamp=*/std::nullopt,
          /*demo_mode_dimensions=*/std::nullopt)
          .SerializePartialAsString();
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, std::move(fake_signing_service));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_job_request_string);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest,
       FlexDeviceRegistrationWithEnrollmentTokenAndPolicyFetch) {
  CreateFlexClient();

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  ExpectAndCaptureJob(GetTokenBasedRegistrationResponse());
  CloudPolicyClient::RegistrationParameters params(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);

  client_->RegisterDeviceWithEnrollmentToken(
      params, /*client_id=*/std::string(),
      DMAuth::FromEnrollmentToken(kFlexEnrollmentToken));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_TOKEN_BASED_DEVICE_REGISTRATION,
            job_type_);
  EXPECT_TRUE(job_request_.has_token_based_device_register_request());
  EXPECT_EQ(
      job_request_.SerializePartialAsString(),
      GetTokenBasedDeviceRegistrationRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

// TODO(b/329271128): Add tests or modify this one to test specific errors
// returned for token-based (Flex) enrollment.
TEST_F(CloudPolicyClientTest,
       FlexDeviceRegistrationWithEnrollmentTokenFailure) {
  CreateFlexClient();
  ExpectAndCaptureJobReplyFailure(net::ERR_FAILED,
                                  DeviceManagementService::kInvalidArgument);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters params(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);

  client_->RegisterDeviceWithEnrollmentToken(
      params, /*client_id=*/std::string(),
      DMAuth::FromEnrollmentToken(kFlexEnrollmentToken));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_TOKEN_BASED_DEVICE_REGISTRATION,
            job_type_);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, DemoModeRegistration) {
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  ExpectAndCaptureJob(GetRegistrationResponse());
  auto fake_signing_service = std::make_unique<FakeSigningService>();
  fake_signing_service->set_success(true);
  const std::string expected_job_request_string =
      GetCertBasedRegistrationRequest(
          fake_signing_service.get(),
          /*psm_execution_result=*/std::nullopt,
          /*psm_determination_timestamp=*/std::nullopt, GetDemoModeDimensions())
          .SerializePartialAsString();
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  CloudPolicyClient::RegistrationParameters demo_enrollment_parameters(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  demo_enrollment_parameters.demo_mode_dimensions = GetDemoModeDimensions();
  client_->RegisterWithCertificate(
      demo_enrollment_parameters, std::string() /* client_id */,
      kEnrollmentCertificate, std::string() /* sub_organization */,
      std::move(fake_signing_service));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_job_request_string);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RegistrationWithCertificateFailToSignRequest) {
  auto fake_signing_service = std::make_unique<FakeSigningService>();
  fake_signing_service->set_success(false);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, std::move(fake_signing_service));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_CANNOT_SIGN_REQUEST, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RegistrationParametersPassedThrough) {
  em::DeviceManagementRequest registration_request = GetRegistrationRequest();
  registration_request.mutable_register_request()->set_reregister(true);
  registration_request.mutable_register_request()->set_requisition(
      kRequisition);
  registration_request.mutable_register_request()->set_server_backed_state_key(
      kStateKey);
  registration_request.mutable_register_request()->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL);
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));

  CloudPolicyClient::RegistrationParameters register_parameters(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL);
  register_parameters.requisition = kRequisition;
  register_parameters.current_state_key = kStateKey;

  client_->Register(register_parameters, kClientID, kOAuthToken);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            registration_request.SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(kClientID, client_id_);
}

TEST_F(CloudPolicyClientTest, RegistrationNoDMTokenInResponse) {
  em::DeviceManagementResponse registration_response =
      GetRegistrationResponse();
  registration_response.mutable_register_response()
      ->clear_device_management_token();
  ExpectAndCaptureJob(registration_response);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RegistrationFailure) {
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RetryRegistration) {
  // Force the register to fail with an error that causes a retry.
  enterprise_management::DeviceManagementRequest request;
  DeviceManagementService::JobConfiguration::JobType job_type;
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.CaptureRequest(&request), SaveArg<0>(&job)));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_EQ(GetRegistrationRequest().SerializePartialAsString(),
            request.SerializePartialAsString());
  EXPECT_FALSE(request.register_request().reregister());
  EXPECT_FALSE(client_->is_registered());
  Mock::VerifyAndClearExpectations(&service_);

  // Retry with network errors |DeviceManagementService::kMaxRetries| times.
  for (int i = 0; i < DeviceManagementService::kMaxRetries; ++i) {
    service_.SendJobResponseNow(&job, net::ERR_NETWORK_CHANGED, 0);
    ASSERT_TRUE(job.IsActive());
    request.ParseFromString(job.GetConfigurationForTesting()->GetPayload());
    EXPECT_TRUE(request.register_request().reregister());
  }

  // Expect failure with yet another retry.
  EXPECT_CALL(observer_, OnClientError);
  service_.SendJobResponseNow(&job, net::ERR_NETWORK_CHANGED, 0);
  EXPECT_FALSE(job.IsActive());
  EXPECT_FALSE(client_->is_registered());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CloudPolicyClientTest, PolicyUpdate) {
  RegisterClient();

  const em::DeviceManagementRequest policy_request = GetPolicyRequest();

  {
    const em::DeviceManagementResponse policy_response = GetPolicyResponse();

    ExpectAndCaptureJob(policy_response);
    EXPECT_CALL(observer_, OnPolicyFetched);
    client_->FetchPolicy(kReason);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              job_type_);
    EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
    EXPECT_EQ(job_request_.SerializePartialAsString(),
              policy_request.SerializePartialAsString());
    CheckPolicyResponse(policy_response);
  }

  {
    em::DeviceManagementResponse policy_response;
    policy_response.mutable_policy_response()->add_responses()->set_policy_data(
        CreatePolicyData("updated-fake-policy-data"));

    ExpectAndCaptureJob(policy_response);
    EXPECT_CALL(observer_, OnPolicyFetched);
    client_->FetchPolicy(kReason);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              job_type_);
    EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
    EXPECT_EQ(job_request_.SerializePartialAsString(),
              policy_request.SerializePartialAsString());
    EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
    CheckPolicyResponse(policy_response);
  }
}

TEST_F(CloudPolicyClientTest, PolicyFetchSHA256) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(policy::kPolicyFetchWithSha256);
  RegisterClient();
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();
  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);

  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  policy_request.mutable_policy_request()
      ->mutable_requests(0)
      ->set_signature_type(em::PolicyFetchRequest::SHA256_RSA);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyFetchDisabledSHA256) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(policy::kPolicyFetchWithSha256);
  RegisterClient();
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();
  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);

  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  policy_request.mutable_policy_request()
      ->mutable_requests(0)
      ->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithMetaData) {
  RegisterClient();

  const int kPublicKeyVersion = 42;
  const base::Time kOldTimestamp(base::Time::UnixEpoch() + base::Days(20));

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->mutable_requests(0);
  policy_fetch_request->set_timestamp(
      kOldTimestamp.InMillisecondsSinceUnixEpoch());
  policy_fetch_request->set_public_key_version(kPublicKeyVersion);

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  client_->set_last_policy_timestamp(kOldTimestamp);
  client_->set_public_key_version(kPublicKeyVersion);

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidation) {
  RegisterClient();

  const int64_t kInvalidationVersion = 12345;
  const std::string kInvalidationPayload("12345");

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->mutable_requests(0);
  policy_fetch_request->set_invalidation_version(kInvalidationVersion);
  policy_fetch_request->set_invalidation_payload(kInvalidationPayload);

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(kInvalidationVersion, kInvalidationPayload);
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
  EXPECT_EQ(12345, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidationNoPayload) {
  RegisterClient();

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(-12345, std::string());
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  CheckPolicyResponse(policy_response);
  EXPECT_EQ(-12345, client_->fetched_invalidation_version());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(CloudPolicyClientMultipleThreadsTest,
       PolicyFetchWithBrowserDeviceIdentifier) {
  RegisterClient();

  // Add the policy type that contains browser device identifier.
  client_->AddPolicyTypeToFetch(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType, std::string());

  // Make a policy fetch.
  ExpectAndCaptureJob(GetPolicyResponse());
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  task_environment_->RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);

  // Collect the policy requests.
  ASSERT_TRUE(job_request_.has_policy_request());
  const em::DevicePolicyRequest& policy_request = job_request_.policy_request();
  std::map<std::pair<std::string, std::string>, em::PolicyFetchRequest>
      request_map;
  for (int i = 0; i < policy_request.requests_size(); ++i) {
    const em::PolicyFetchRequest& fetch_request = policy_request.requests(i);
    ASSERT_TRUE(fetch_request.has_policy_type());
    std::pair<std::string, std::string> key(fetch_request.policy_type(),
                                            fetch_request.settings_entity_id());
    EXPECT_EQ(0UL, request_map.count(key));
    request_map[key].CopyFrom(fetch_request);
  }

  std::map<std::pair<std::string, std::string>, em::PolicyFetchRequest>
      expected_requests;
  // Expected user policy fetch request.
  std::pair<std::string, std::string> user_policy_key(
      dm_protocol::kChromeUserPolicyType, std::string());
  expected_requests[user_policy_key] =
      GetPolicyRequest().policy_request().requests(0);
  // Expected user cloud policy fetch request.
  std::pair<std::string, std::string> user_cloud_policy_key(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType, std::string());
  em::PolicyFetchRequest policy_fetch_request =
      GetPolicyRequest().policy_request().requests(0);
  policy_fetch_request.set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  policy_fetch_request.set_allocated_browser_device_identifier(
      GetBrowserDeviceIdentifier().release());
  expected_requests[user_cloud_policy_key] = policy_fetch_request;

  EXPECT_EQ(request_map.size(), expected_requests.size());
  for (auto it = expected_requests.begin(); it != expected_requests.end();
       ++it) {
    EXPECT_EQ(1UL, request_map.count(it->first));
    EXPECT_EQ(request_map[it->first].SerializePartialAsString(),
              it->second.SerializePartialAsString());
  }
}
#endif

// Tests that previous OAuth token is no longer sent in policy fetch after its
// value was cleared.
TEST_F(CloudPolicyClientTest, PolicyFetchClearOAuthToken) {
  RegisterClient();

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->SetOAuthTokenAsAdditionalAuth("");
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, BadPolicyResponse) {
  RegisterClient();

  const em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::DeviceManagementResponse policy_response;

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->last_dm_status());

  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("fake-policy-data"));
  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("excess-fake-policy-data"));
  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyRequestFailure) {
  base::HistogramTester histogram_tester;

  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus.UserPolicy",
      DM_STATUS_REQUEST_FAILED, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DMServerCloudPolicyRequestStatus", DM_STATUS_REQUEST_FAILED,
      1);
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithExtensionPolicy) {
  RegisterClient();

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  // Set up the |expected_responses| and |policy_response|.
  static const char* kExtensions[] = {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "cccccccccccccccccccccccccccccccc",
  };
  typedef std::map<std::pair<std::string, std::string>, em::PolicyFetchResponse>
      ResponseMap;
  ResponseMap expected_responses;
  std::set<std::pair<std::string, std::string>> expected_namespaces;
  std::pair<std::string, std::string> key(dm_protocol::kChromeUserPolicyType,
                                          std::string());
  // Copy the user policy fetch request.
  expected_responses[key].CopyFrom(
      policy_response.policy_response().responses(0));
  expected_namespaces.insert(key);
  key.first = dm_protocol::kChromeExtensionPolicyType;
  expected_namespaces.insert(key);
  for (size_t i = 0; i < std::size(kExtensions); ++i) {
    key.second = kExtensions[i];
    em::PolicyData policy_data;
    policy_data.set_policy_type(key.first);
    policy_data.set_settings_entity_id(key.second);
    expected_responses[key].set_policy_data(policy_data.SerializeAsString());
    policy_response.mutable_policy_response()->add_responses()->CopyFrom(
        expected_responses[key]);
  }

  // Make a policy fetch.
  em::DeviceManagementRequest request;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.CaptureRequest(&request),
                      service_.SendJobOKAsync(policy_response)));
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeExtensionPolicyType,
                                std::string());
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);

  // Verify that the request includes the expected namespaces.
  ASSERT_TRUE(request.has_policy_request());
  const em::DevicePolicyRequest& policy_request = request.policy_request();
  ASSERT_EQ(2, policy_request.requests_size());
  for (int i = 0; i < policy_request.requests_size(); ++i) {
    const em::PolicyFetchRequest& fetch_request = policy_request.requests(i);
    ASSERT_TRUE(fetch_request.has_policy_type());
    EXPECT_FALSE(fetch_request.has_settings_entity_id());
    key = {fetch_request.policy_type(), std::string()};
    EXPECT_EQ(1u, expected_namespaces.erase(key));
  }
  EXPECT_TRUE(expected_namespaces.empty());

  // Verify that the client got all the responses mapped to their namespaces.
  for (auto it = expected_responses.begin(); it != expected_responses.end();
       ++it) {
    const em::PolicyFetchResponse* response =
        client_->GetPolicyFor(it->first.first, it->first.second);
    ASSERT_TRUE(response);
    EXPECT_EQ(it->second.SerializeAsString(), response->SerializeAsString());
  }
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseMachineCertificate) {
  RegisterClient();

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadMachineCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentCertificate) {
  RegisterClient();

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseEnrollmentCertificate(kEnrollmentCertificate,
                                                 std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadEnrollmentCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseMachineCertificateEmpty) {
  RegisterClient();

  em::DeviceManagementResponse upload_certificate_response =
      GetUploadCertificateResponse();
  upload_certificate_response.clear_cert_upload_response();
  ExpectAndCaptureJob(upload_certificate_response);
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(
                  CloudPolicyClient::Result(DM_STATUS_RESPONSE_DECODING_ERROR)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadMachineCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseMachineCertificateNotRegistered) {
  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentCertificateEmpty) {
  RegisterClient();

  em::DeviceManagementResponse upload_certificate_response =
      GetUploadCertificateResponse();
  upload_certificate_response.clear_cert_upload_response();
  ExpectAndCaptureJob(upload_certificate_response);
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(
                  CloudPolicyClient::Result(DM_STATUS_RESPONSE_DECODING_ERROR)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseEnrollmentCertificate(kEnrollmentCertificate,
                                                 std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadEnrollmentCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadCertificateFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(
      result_callback_observer_,
      OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_REQUEST_FAILED)))
      .Times(1);
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentId) {
  RegisterClient();

  em::DeviceManagementRequest upload_enrollment_id_request;
  upload_enrollment_id_request.mutable_cert_upload_request()->set_enrollment_id(
      kEnrollmentId);

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseEnrollmentId(kEnrollmentId, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            upload_enrollment_id_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadStatus) {
  RegisterClient();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadStatusNotRegistered) {
  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_F(CloudPolicyClientTest, UploadStatusWithOAuthToken) {
  RegisterClient();

  // Test that OAuth token is sent in status upload.
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  // Tests that previous OAuth token is no longer sent in status upload after
  // its value was cleared.
  client_->SetOAuthTokenAsAdditionalAuth("");

  callback = base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                            base::Unretained(&result_callback_observer_));
  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
#if !BUILDFLAG(IS_IOS)
  EXPECT_THAT(query_params_,
              Not(Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken))));
#endif
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadStatusWhilePolicyFetchActive) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.SendJobOKAsync(GetEmptyResponse())));
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type);

  // Now initiate a policy fetch - this should not cancel the upload job.
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  CheckPolicyResponse(policy_response);

  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadPolicyValidationReport) {
  RegisterClient();

  em::DeviceManagementRequest upload_policy_validation_report_request;
  {
    em::PolicyValidationReportRequest* policy_validation_report_request =
        upload_policy_validation_report_request
            .mutable_policy_validation_report_request();
    policy_validation_report_request->set_policy_type(policy_type_);
    policy_validation_report_request->set_policy_token(kPolicyToken);
    policy_validation_report_request->set_action(
        em::PolicyValidationReportRequest_Action_STORE);
    policy_validation_report_request->set_validation_result_type(
        em::PolicyValidationReportRequest::
            VALIDATION_RESULT_TYPE_VALUE_WARNING);
    em::PolicyValueValidationIssue* policy_value_validation_issue =
        policy_validation_report_request->add_policy_value_validation_issues();
    policy_value_validation_issue->set_policy_name(kPolicyName);
    policy_value_validation_issue->set_severity(
        em::PolicyValueValidationIssue::
            VALUE_VALIDATION_ISSUE_SEVERITY_WARNING);
    policy_value_validation_issue->set_debug_message(kValueValidationMessage);
  }

  ExpectAndCaptureJob(GetEmptyResponse());
  std::vector<ValueValidationIssue> issues;
  issues.push_back(
      {kPolicyName, ValueValidationIssue::kWarning, kValueValidationMessage});
  client_->UploadPolicyValidationReport(
      CloudPolicyValidatorBase::VALIDATION_VALUE_WARNING, issues, kStore,
      policy_type_, kPolicyToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_UPLOAD_POLICY_VALIDATION_REPORT,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            upload_policy_validation_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadChromeDesktopReport) {
  RegisterClient();

  em::DeviceManagementRequest chrome_desktop_report_request;
  chrome_desktop_report_request.mutable_chrome_desktop_report_request();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  std::unique_ptr<em::ChromeDesktopReportRequest> chrome_desktop_report =
      std::make_unique<em::ChromeDesktopReportRequest>();
  client_->UploadChromeDesktopReport(std::move(chrome_desktop_report),
                                     std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            chrome_desktop_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadChromeDesktopReportNotRegistered) {
  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  std::unique_ptr<em::ChromeDesktopReportRequest> chrome_desktop_report =
      std::make_unique<em::ChromeDesktopReportRequest>();
  client_->UploadChromeDesktopReport(std::move(chrome_desktop_report),
                                     result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_F(CloudPolicyClientTest, UploadChromeOsUserReport) {
  RegisterClient();

  em::DeviceManagementRequest chrome_os_user_report_request;
  chrome_os_user_report_request.mutable_chrome_os_user_report_request();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  std::unique_ptr<em::ChromeOsUserReportRequest> chrome_os_user_report =
      std::make_unique<em::ChromeOsUserReportRequest>();
  client_->UploadChromeOsUserReport(std::move(chrome_os_user_report),
                                    std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            chrome_os_user_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadChromeOsUserReportNotRegistered) {
  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  std::unique_ptr<em::ChromeOsUserReportRequest> chrome_os_user_report =
      std::make_unique<em::ChromeOsUserReportRequest>();
  client_->UploadChromeOsUserReport(std::move(chrome_os_user_report),
                                    result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_F(CloudPolicyClientTest, UploadChromeProfile) {
  RegisterClient();

  em::DeviceManagementRequest device_managment_request;
  device_managment_request.mutable_chrome_profile_report_request()
      ->mutable_os_report()
      ->set_name(kOsName);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  auto chrome_profile_report =
      std::make_unique<em::ChromeProfileReportRequest>();
  chrome_profile_report->mutable_os_report()->set_name(kOsName);
  client_->UploadChromeProfileReport(std::move(chrome_profile_report),
                                     std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_PROFILE_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            device_managment_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, UploadChromeProfileNotRegistered) {
  base::test::TestFuture<CloudPolicyClient::Result> result_future;
  auto chrome_profile_report =
      std::make_unique<em::ChromeProfileReportRequest>();
  client_->UploadChromeProfileReport(std::move(chrome_profile_report),
                                     result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

// A helper class to test all em::DeviceRegisterRequest::PsmExecutionResult enum
// values.
class CloudPolicyClientRegisterWithPsmParamsTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<PsmExecutionResult> {
 public:
  PsmExecutionResult GetPsmExecutionResult() const { return GetParam(); }
};

TEST_P(CloudPolicyClientRegisterWithPsmParamsTest,
       RegistrationWithCertificateAndPsmResult) {
  const int64_t kExpectedPsmDeterminationTimestamp = 2;

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();
  const PsmExecutionResult psm_execution_result = GetPsmExecutionResult();

  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  ExpectAndCaptureJob(GetRegistrationResponse());
  auto fake_signing_service = std::make_unique<FakeSigningService>();
  fake_signing_service->set_success(true);
  const std::string expected_job_request_string =
      GetCertBasedRegistrationRequest(fake_signing_service.get(),
                                      psm_execution_result,
                                      kExpectedPsmDeterminationTimestamp,
                                      /*demo_mode_dimensions=*/std::nullopt)
          .SerializePartialAsString();
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  device_attestation.psm_determination_timestamp =
      kExpectedPsmDeterminationTimestamp;
  device_attestation.psm_execution_result = psm_execution_result;
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, std::move(fake_signing_service));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_job_request_string);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyClientRegisterWithPsmParams,
    CloudPolicyClientRegisterWithPsmParamsTest,
    ::testing::Values(
        em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN,
        em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE,
        em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE,
        em::DeviceRegisterRequest::PSM_RESULT_ERROR));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)

class CloudPolicyClientUploadSecurityEventTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<bool> {
 public:
  bool include_device_info() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientUploadSecurityEventTest,
                         testing::Bool());

TEST_F(CloudPolicyClientTest, UploadSecurityEventReportNotRegistered) {
  ASSERT_FALSE(client_->is_registered());

  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  client_->UploadSecurityEventReport(/*include_device_info=*/false,
                                     MakeDefaultRealtimeReport(),
                                     result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_P(CloudPolicyClientUploadSecurityEventTest, Test) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  client_->UploadSecurityEventReport(
      include_device_info(), MakeDefaultRealtimeReport(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  std::optional<base::Value> payload = base::JSONReader::Read(job_payload_);
  ASSERT_TRUE(payload);
  const base::Value::Dict& payload_dict = payload->GetDict();

  ASSERT_FALSE(policy::GetDeviceName().empty());
  EXPECT_EQ(version_info::GetVersionNumber(),
            *payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetChromeVersionPath()));

  if (include_device_info()) {
    EXPECT_EQ(kDMToken, *payload_dict.FindStringByDottedPath(
                            ReportingJobConfigurationBase::
                                DeviceDictionaryBuilder::GetDMTokenPath()));
    EXPECT_EQ(client_id_, *payload_dict.FindStringByDottedPath(
                              ReportingJobConfigurationBase::
                                  DeviceDictionaryBuilder::GetClientIdPath()));
    EXPECT_EQ(policy::GetOSUsername(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                      GetMachineUserPath()));
    EXPECT_EQ(GetOSPlatform(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSPlatformPath()));
    EXPECT_EQ(GetOSVersion(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSVersionPath()));
    EXPECT_EQ(policy::GetDeviceName(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetNamePath()));
  } else {
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetDMTokenPath()));
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetClientIdPath()));
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::BrowserDictionaryBuilder::
            GetMachineUserPath()));
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetOSPlatformPath()));
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetOSVersionPath()));
    EXPECT_FALSE(payload_dict.FindStringByDottedPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::GetNamePath()));
  }

  const base::Value* events =
      payload_dict.Find(RealtimeReportingJobConfiguration::kEventListKey);
  EXPECT_EQ(base::Value::Type::LIST, events->type());
  EXPECT_EQ(1u, events->GetList().size());
}

TEST_F(CloudPolicyClientTest, RealtimeReportMerge) {
  auto config = std::make_unique<RealtimeReportingJobConfiguration>(
      client_.get(), service_.configuration()->GetRealtimeReportingServerUrl(),
      /*include_device_info*/ true,
      RealtimeReportingJobConfiguration::UploadCompleteCallback());

  // Add one report to the config.
  {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent");
    context.SetByDottedPath("profile.profileName", "Profile 1");
    context.SetByDottedPath("profile.profilePath", "C:\\User Data\\Profile 1");

    base::Value::Dict event;
    event.Set("time", "2019-09-10T20:01:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List events;
    events.Append(std::move(event));

    base::Value::Dict report;
    report.Set(RealtimeReportingJobConfiguration::kEventListKey,
               std::move(events));
    report.Set(RealtimeReportingJobConfiguration::kContextKey,
               std::move(context));

    ASSERT_TRUE(config->AddReport(std::move(report)));
  }

  // Add a second report to the config with a different context.
  {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name2@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent2");
    context.SetByDottedPath("browser.version", "1.0.0.0");

    base::Value::Dict event;
    event.Set("time", "2019-09-10T20:02:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List events;
    events.Append(std::move(event));

    base::Value::Dict report;
    report.Set(RealtimeReportingJobConfiguration::kEventListKey,
               std::move(events));
    report.Set(RealtimeReportingJobConfiguration::kContextKey,
               std::move(context));

    ASSERT_TRUE(config->AddReport(std::move(report)));
  }

  // The second config should trump the first.
  DeviceManagementService::JobConfiguration* job_config = config.get();
  std::optional<base::Value> payload =
      base::JSONReader::Read(job_config->GetPayload());
  ASSERT_TRUE(payload);
  const base::Value::Dict& payload_dict = payload->GetDict();

  ASSERT_EQ("name2@gmail.com",
            *payload_dict.FindStringByDottedPath("profile.gaiaEmail"));
  ASSERT_EQ("User-Agent2",
            *payload_dict.FindStringByDottedPath("browser.userAgent"));
  ASSERT_EQ("Profile 1",
            *payload_dict.FindStringByDottedPath("profile.profileName"));
  ASSERT_EQ("C:\\User Data\\Profile 1",
            *payload_dict.FindStringByDottedPath("profile.profilePath"));
  ASSERT_EQ("1.0.0.0", *payload_dict.FindStringByDottedPath("browser.version"));
  ASSERT_EQ(2u, payload_dict
                    .FindList(RealtimeReportingJobConfiguration::kEventListKey)
                    ->size());
}

TEST_F(CloudPolicyClientTest, UploadAppInstallReportNotRegistered) {
  ASSERT_FALSE(client_->is_registered());

  base::test::TestFuture<CloudPolicyClient::Result> result_future;

  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  result_future.GetCallback());

  const CloudPolicyClient::Result& result = result_future.Get();
  EXPECT_EQ(result,
            CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));
}

TEST_F(CloudPolicyClientTest, UploadAppInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, CancelUploadAppInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(0);

  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  em::AppInstallReportRequest app_install_report;
  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());

  // The job expected by the call to ExpectRealTimeReport() completes
  // when base::RunLoop().RunUntilIdle() is called.  To simulate a cancel
  // before the response for the request is processed, make sure to cancel it
  // before running a loop.
  client_->CancelAppInstallReportUpload();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
}

TEST_F(CloudPolicyClientTest, UploadAppInstallReportSupersedesPending) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(0);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));

  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  Mock::VerifyAndClearExpectations(&service_);
  Mock::VerifyAndClearExpectations(&status_callback_observer_);

  // Starting another app push-install report upload should cancel the pending
  // one.
  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  callback = base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                            base::Unretained(&result_callback_observer_));
  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

#endif

TEST_F(CloudPolicyClientTest, MultipleActiveRequests) {
  RegisterClient();

  // Set up pending upload status job.
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobOKAsync(GetEmptyResponse())));
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));

  // Set up pending upload certificate job.
  DeviceManagementService::JobConfiguration::JobType cert_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&cert_type),
                      service_.SendJobOKAsync(GetUploadCertificateResponse())));

  // Expect two calls on our upload observer, one for the status upload and
  // one for the certificate upload.
  CloudPolicyClient::ResultCallback callback2 =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback2));
  EXPECT_EQ(2, client_->GetActiveRequestCountForTest());

  // Now satisfy both active jobs.
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            cert_type);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, UploadStatusFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(
      result_callback_observer_,
      OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_REQUEST_FAILED)))
      .Times(1);
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, ShouldRejectUnsignedCommands) {
  const DeviceManagementStatus expected_error =
      DM_STATUS_RESPONSE_DECODING_ERROR;

  RegisterClient();

  em::DeviceManagementResponse remote_command_response;
  em::RemoteCommand* command =
      remote_command_response.mutable_remote_command_response()->add_commands();
  command->set_age_of_command(kAgeOfCommand);
  command->set_payload(kPayload);
  command->set_command_id(kLastCommandId + 1);
  command->set_type(em::RemoteCommand_Type_COMMAND_ECHO_TEST);

  ExpectAndCaptureJob(remote_command_response);

  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_error, _))
      .Times(1);
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      kRemoteCommandsFetchSignatureType,
      dm_protocol::kChromeUserRemoteCommandType, std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(CloudPolicyClientTest,
       ShouldIgnoreSignedCommandsIfUnsignedCommandsArePresent) {
  const DeviceManagementStatus expected_error =
      DM_STATUS_RESPONSE_DECODING_ERROR;

  RegisterClient();

  em::DeviceManagementResponse remote_command_response;
  auto* response = remote_command_response.mutable_remote_command_response();
  response->add_commands();
  response->add_secure_commands();

  ExpectAndCaptureJob(remote_command_response);

  std::vector<em::SignedData> received_commands;
  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_error, _))
      .WillOnce(SaveArg<1>(&received_commands));
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      kRemoteCommandsFetchSignatureType,
      dm_protocol::kChromeUserRemoteCommandType, std::move(callback));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(received_commands, ElementsAre());
}

TEST_F(CloudPolicyClientTest, ShouldNotFailIfRemoteCommandResponseIsEmpty) {
  const DeviceManagementStatus expected_result = DM_STATUS_SUCCESS;

  RegisterClient();

  em::DeviceManagementResponse empty_server_response;

  ExpectAndCaptureJob(empty_server_response);

  std::vector<em::SignedData> received_commands;
  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_result, _))
      .Times(1);
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      kRemoteCommandsFetchSignatureType,
      dm_protocol::kChromeUserRemoteCommandType, std::move(callback));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(received_commands, ElementsAre());
}

TEST_F(CloudPolicyClientTest, FetchSecureRemoteCommands) {
  RegisterClient();

  em::DeviceManagementRequest remote_command_request =
      GetRemoteCommandRequest(kRemoteCommandsFetchSignatureType);

  em::DeviceManagementResponse remote_command_response;
  em::SignedData* signed_command =
      remote_command_response.mutable_remote_command_response()
          ->add_secure_commands();
  signed_command->set_data("signed-data");
  signed_command->set_signature("signed-signature");

  ExpectAndCaptureJob(remote_command_response);

  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(
      remote_commands_observer,
      OnRemoteCommandsFetched(
          DM_STATUS_SUCCESS,
          ElementsAre(MatchProto(
              remote_command_response.remote_command_response().secure_commands(
                  0)))))
      .Times(1);

  base::RunLoop run_loop;
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindLambdaForTesting(
          [&](DeviceManagementStatus status,
              const std::vector<enterprise_management::SignedData>&
                  signed_commands) {
            remote_commands_observer.OnRemoteCommandsFetched(status,
                                                             signed_commands);
            run_loop.Quit();
          });
  const std::vector<em::RemoteCommandResult> command_results(
      1, remote_command_request.remote_command_request().command_results(0));
  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId),
      command_results, kRemoteCommandsFetchSignatureType,
      dm_protocol::kChromeUserRemoteCommandType, std::move(callback));
  run_loop.Run();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            remote_command_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest,
       RequestDeviceAttributeUpdatePermissionWithOAuthToken) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_permission_request;
  attribute_update_permission_request
      .mutable_device_attribute_update_permission_request();

  em::DeviceManagementResponse attribute_update_permission_response;
  attribute_update_permission_response
      .mutable_device_attribute_update_permission_response()
      ->set_result(
          em::DeviceAttributeUpdatePermissionResponse_ResultType_ATTRIBUTE_UPDATE_ALLOWED);

  ExpectAndCaptureJob(attribute_update_permission_response);
  EXPECT_CALL(status_callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&status_callback_observer_));
  client_->GetDeviceAttributeUpdatePermission(
      DMAuth::FromOAuthToken(kOAuthToken), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_ATTRIBUTE_UPDATE_PERMISSION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_permission_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest,
       RequestDeviceAttributeUpdatePermissionWithDMToken) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_permission_request;
  attribute_update_permission_request
      .mutable_device_attribute_update_permission_request();

  em::DeviceManagementResponse attribute_update_permission_response;
  attribute_update_permission_response
      .mutable_device_attribute_update_permission_response()
      ->set_result(
          em::DeviceAttributeUpdatePermissionResponse_ResultType_ATTRIBUTE_UPDATE_ALLOWED);

  ExpectAndCaptureJob(attribute_update_permission_response);
  EXPECT_CALL(status_callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&status_callback_observer_));
  client_->GetDeviceAttributeUpdatePermission(DMAuth::FromDMToken(kDMToken),
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_ATTRIBUTE_UPDATE_PERMISSION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_permission_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest,
       RequestDeviceAttributeUpdatePermissionMissingResponse) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_permission_request;
  attribute_update_permission_request
      .mutable_device_attribute_update_permission_request();

  em::DeviceManagementResponse attribute_update_permission_response;

  ExpectAndCaptureJob(attribute_update_permission_response);
  EXPECT_CALL(status_callback_observer_, OnCallbackComplete(false)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&status_callback_observer_));
  client_->GetDeviceAttributeUpdatePermission(
      DMAuth::FromOAuthToken(kOAuthToken), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_ATTRIBUTE_UPDATE_PERMISSION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_permission_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RequestDeviceAttributeUpdate) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_request;
  attribute_update_request.mutable_device_attribute_update_request()
      ->set_asset_id(kAssetId);
  attribute_update_request.mutable_device_attribute_update_request()
      ->set_location(kLocation);

  em::DeviceManagementResponse attribute_update_response;
  attribute_update_response.mutable_device_attribute_update_response()
      ->set_result(
          em::DeviceAttributeUpdateResponse_ResultType_ATTRIBUTE_UPDATE_SUCCESS);

  ExpectAndCaptureJob(attribute_update_response);
  EXPECT_CALL(status_callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&status_callback_observer_));
  client_->UpdateDeviceAttributes(DMAuth::FromOAuthToken(kOAuthToken), kAssetId,
                                  kLocation, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE,
            job_type_);
  VerifyQueryParameter();
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, RequestGcmIdUpdate) {
  RegisterClient();

  em::DeviceManagementRequest gcm_id_update_request;
  gcm_id_update_request.mutable_gcm_id_update_request()->set_gcm_id(kGcmID);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(status_callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&status_callback_observer_));
  client_->UpdateGcmId(kGcmID, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            gcm_id_update_request.SerializePartialAsString());
}

TEST_F(CloudPolicyClientTest, PolicyReregistration) {
  RegisterClient();

  // Handle 410 (unknown deviceID) on policy fetch.
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobResponseAsync(
                          net::OK, DeviceManagementService::kDeviceNotFound)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DM_STATUS_SERVICE_DEVICE_NOT_FOUND, client_->last_dm_status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());

  // Re-register.
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters user_recovery(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  client_->Register(user_recovery, client_id_, kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetReregistrationRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest, PolicyReregistrationFailsWithNonMatchingDMToken) {
  RegisterClient();

  // Handle 410 (unknown deviceID) on policy fetch.
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobResponseAsync(
                          net::OK, DeviceManagementService::kDeviceNotFound)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DM_STATUS_SERVICE_DEVICE_NOT_FOUND, client_->last_dm_status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());

  // Re-register (server sends wrong DMToken).
  ExpectAndCaptureJobReplyFailure(
      net::OK, DeviceManagementService::kInvalidAuthCookieOrDMToken);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters user_recovery(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  client_->Register(user_recovery, client_id_, kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetReregistrationRequest().SerializePartialAsString());
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
            client_->last_dm_status());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(CloudPolicyClientTest, PolicyReregistrationAfterDMTokenDeletion) {
  RegisterClient();
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());

  // Handle 410 (device needs reset) on policy fetch.
  DeviceManagementService::JobConfiguration::JobType upload_type;
  em::DeviceManagementResponse response;
  response.add_error_detail(em::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&upload_type),
          service_.SendJobResponseAsync(
              net::OK, DeviceManagementService::kDeviceNotFound, response)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DM_STATUS_SERVICE_DEVICE_NEEDS_RESET, client_->last_dm_status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());

  // Re-register.
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters user_recovery(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  client_->Register(user_recovery, client_id_, kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  VerifyQueryParameter();
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetReregistrationRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(CloudPolicyClientTest, RequestFetchRobotAuthCodes) {
  RegisterClient();

  em::DeviceManagementRequest robot_auth_code_fetch_request =
      GetRobotAuthCodeFetchRequest();
  em::DeviceManagementResponse robot_auth_code_fetch_response =
      GetRobotAuthCodeFetchResponse();

  ExpectAndCaptureJob(robot_auth_code_fetch_response);
  EXPECT_CALL(robot_auth_code_callback_observer_,
              OnRobotAuthCodeFetched(_, kRobotAuthCode));

  em::DeviceServiceApiAccessRequest::DeviceType device_type =
      em::DeviceServiceApiAccessRequest::CHROME_OS;
  std::set<std::string> oauth_scopes = {kApiAuthScope};
  client_->FetchRobotAuthCodes(
      DMAuth::FromDMToken(kDMToken), device_type, oauth_scopes,
      base::BindOnce(&MockRobotAuthCodeCallbackObserver::OnRobotAuthCodeFetched,
                     base::Unretained(&robot_auth_code_callback_observer_)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(robot_auth_code_fetch_request.SerializePartialAsString(),
            job_request_.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

TEST_F(CloudPolicyClientTest,
       RequestFetchRobotAuthCodesNotInterruptedByPolicyFetch) {
  RegisterClient();

  em::DeviceManagementResponse robot_auth_code_fetch_response =
      GetRobotAuthCodeFetchResponse();

  // Expect a robot auth code fetch request that never runs its callback to
  // simulate something happening while we wait for the request to return.
  DeviceManagementService::JobForTesting robot_job;
  DeviceManagementService::JobConfiguration::JobType robot_job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&robot_job_type),
                      SaveArg<0>(&robot_job)));

  EXPECT_CALL(robot_auth_code_callback_observer_,
              OnRobotAuthCodeFetched(_, kRobotAuthCode));

  em::DeviceServiceApiAccessRequest::DeviceType device_type =
      em::DeviceServiceApiAccessRequest::CHROME_OS;
  std::set<std::string> oauth_scopes = {kApiAuthScope};
  client_->FetchRobotAuthCodes(
      DMAuth::FromDMToken(kDMToken), device_type, oauth_scopes,
      base::BindOnce(&MockRobotAuthCodeCallbackObserver::OnRobotAuthCodeFetched,
                     base::Unretained(&robot_auth_code_callback_observer_)));
  base::RunLoop().RunUntilIdle();

  ExpectAndCaptureJob(GetPolicyResponse());
  EXPECT_CALL(observer_, OnPolicyFetched);

  client_->FetchPolicy(kReason);
  base::RunLoop().RunUntilIdle();

  // Try to manually finish the robot auth code fetch job.
  service_.SendJobResponseNow(&robot_job, net::OK,
                              DeviceManagementService::kSuccess,
                              robot_auth_code_fetch_response);

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
            robot_job_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
}

TEST_F(CloudPolicyClientTest, UploadFmRegistrationTokenRequest) {
  RegisterClient();

  em::FmRegistrationTokenUploadRequest request;
  request.set_token("fake token");
  request.set_protocol_version(101);
  request.set_token_type(em::FmRegistrationTokenUploadRequest::USER);
  request.set_expiration_timestamp_ms(
      (base::Time::Now() + base::Minutes(5)).InMillisecondsSinceUnixEpoch());

  em::DeviceManagementRequest expected_request;
  expected_request.mutable_fm_registration_token_upload_request()->CopyFrom(
      request);

  ExpectAndCaptureJob(GetFmRegistrationTokenUploadResponse());
  EXPECT_CALL(result_callback_observer_,
              OnCallbackComplete(CloudPolicyClient::Result(DM_STATUS_SUCCESS)))
      .Times(1);
  CloudPolicyClient::ResultCallback callback =
      base::BindOnce(&MockResultCallbackObserver::OnCallbackComplete,
                     base::Unretained(&result_callback_observer_));

  client_->UploadFmRegistrationToken(std::move(request), std::move(callback));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_UPLOAD_FM_REGISTRATION_TOKEN,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
}

struct MockClientCertProvisioningRequestCallbackObserver {
  MOCK_METHOD(
      void,
      Callback,
      (DeviceManagementStatus,
       const enterprise_management::ClientCertificateProvisioningResponse&
           response),
      (const));
};

// Tests for CloudPolicyClient::ClientCertProvisioningRequest. The test
// parameter is a device DMToken (which can be empty).
class CloudPolicyClientCertProvisioningRequestTest
    : public CloudPolicyClientTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  std::string GetDeviceDMToken() { return GetParam(); }

  void SetUp() override {
    CloudPolicyClientTest::SetUp();

    RegisterClient(/*device_dm_token=*/GetDeviceDMToken());
  }
};

// Tests that a ClientCertificateProvisioningRequest succeeds.
TEST_P(CloudPolicyClientCertProvisioningRequestTest, Success) {
  const std::string cert_scope = "fake_cert_scope_1";
  const std::string invalidation_topic = "fake_invalidation_topic_1";
  const std::string va_challenge = "fake_va_challenge_1";

  em::DeviceManagementRequest expected_request;
  {
    em::ClientCertificateProvisioningRequest* inner_request =
        expected_request.mutable_client_certificate_provisioning_request();
    inner_request->set_certificate_scope(cert_scope);
    inner_request->set_device_dm_token(GetDeviceDMToken());
  }

  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    em::StartCsrResponse* start_csr_response =
        inner_response->mutable_start_csr_response();
    start_csr_response->set_invalidation_topic(invalidation_topic);
    start_csr_response->set_va_challenge(va_challenge);
  }

  em::ClientCertificateProvisioningResponse received_response;

  MockClientCertProvisioningRequestCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer,
              Callback(DeviceManagementStatus::DM_STATUS_SUCCESS, _))
      .WillOnce(SaveArg<1>(&received_response));

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                      service_.CaptureRequest(&job_request_),
                      service_.SendJobOKAsync(fake_response)));

  client_->ClientCertProvisioningRequest(
      expected_request.client_certificate_provisioning_request(),
      base::BindOnce(
          &MockClientCertProvisioningRequestCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_request.SerializePartialAsString());
  EXPECT_EQ(fake_response.client_certificate_provisioning_response()
                .SerializePartialAsString(),
            received_response.SerializePartialAsString());
}

// Tests that a ClientCertificateProvisioningRequest fails because the response
// can't be decoded. Specifically, it doesn't contain a
// client_certificate_provisioning_response field.
TEST_P(CloudPolicyClientCertProvisioningRequestTest, FailureDecodingError) {
  const std::string cert_scope = "fake_cert_scope_1";

  em::ClientCertificateProvisioningRequest request;
  request.set_certificate_scope(cert_scope);
  request.set_device_dm_token(GetDeviceDMToken());

  // The response doesn't have a client_certificate_provisioning_response field
  // set, which should lead to DM_STATUS_DECODING_ERROR.
  em::DeviceManagementResponse fake_response;

  em::ClientCertificateProvisioningResponse received_response;

  MockClientCertProvisioningRequestCallbackObserver callback_observer;
  EXPECT_CALL(
      callback_observer,
      Callback(DeviceManagementStatus::DM_STATUS_RESPONSE_DECODING_ERROR, _))
      .WillOnce(SaveArg<1>(&received_response));

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(service_.SendJobOKAsync(fake_response));

  client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(
          &MockClientCertProvisioningRequestCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  const em::ClientCertificateProvisioningResponse empty_response;
  EXPECT_EQ(empty_response.SerializePartialAsString(),
            received_response.SerializePartialAsString());
}

// Tests that a ClientCertificateProvisioningRequest fails because the response
// DeviceManagementStatus is not DM_STATUS_SUCCESS.
TEST_P(CloudPolicyClientCertProvisioningRequestTest, NonSuccessStatus) {
  const std::string cert_scope = "fake_cert_scope_1";

  em::ClientCertificateProvisioningRequest request;
  request.set_certificate_scope(cert_scope);
  request.set_device_dm_token(GetDeviceDMToken());

  em::ClientCertificateProvisioningResponse received_response;

  MockClientCertProvisioningRequestCallbackObserver callback_observer;
  EXPECT_CALL(
      callback_observer,
      Callback(DeviceManagementStatus::DM_STATUS_SERVICE_ACTIVATION_PENDING, _))
      .WillOnce(SaveArg<1>(&received_response));

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(service_.SendJobResponseAsync(
          net::OK, DeviceManagementService::kPendingApproval));

  client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(
          &MockClientCertProvisioningRequestCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  const em::ClientCertificateProvisioningResponse empty_response;
  EXPECT_EQ(empty_response.SerializePartialAsString(),
            received_response.SerializePartialAsString());
}

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientCertProvisioningRequestTest,
                         ::testing::Values(std::string(), kDeviceDMToken));

}  // namespace policy
