// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include <cstddef>
#include <optional>
#include <set>
#include <vector>

#include "base/check_deref.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/features.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

namespace em = enterprise_management;

using testing::_;
using testing::StrictMock;

namespace policy {

constexpr char kAppPackage[] = "appPackage";
constexpr char kEventType[] = "eventType";
constexpr char kEventId[] = "eventId";
constexpr char kStatusCode[] = "status";

constexpr char kDummyToken[] = "dm_token";
constexpr char kPackage[] = "unitTestPackage";
constexpr char kExtensionId[] = "unitTestExtensionId";
constexpr char kExtensionName[] = "unitTestExtensionName";

class MockCallbackObserver {
 public:
  MockCallbackObserver() = default;

  MOCK_METHOD4(OnURLLoadComplete,
               void(DeviceManagementService::Job* job,
                    DeviceManagementStatus code,
                    int response_code,
                    std::optional<base::Value::Dict>));
};

class RealtimeReportingJobConfigurationTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  RealtimeReportingJobConfigurationTest()
#if BUILDFLAG(IS_CHROMEOS)
      : client_(&service_),
        fake_serial_number_(&fake_statistics_provider_)
#else
      : client_(&service_)
#endif
  {
  }

  void SetUp() override {
    client_.SetDMToken(kDummyToken);
    if (use_proto_format()) {
      feature_list_.InitWithFeatures(
          {kUploadRealtimeReportingEventsUsingProto,
           policy::features::kEnhancedSecurityEventFields},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {policy::features::kEnhancedSecurityEventFields},
          {kUploadRealtimeReportingEventsUsingProto});
    }

    configuration_ = std::make_unique<RealtimeReportingJobConfiguration>(
        &client_, service_.configuration()->GetRealtimeReportingServerUrl(),
        /*include_device_info=*/true,
        base::BindOnce(&MockCallbackObserver::OnURLLoadComplete,
                       base::Unretained(&callback_observer_)));
    if (use_proto_format()) {
      ::chrome::cros::reporting::proto::UploadEventsRequest request;
      request.mutable_browser()->set_user_agent("dummyAgent");
      for (size_t i = 0; i < kIds.size(); ++i) {
        request.add_events()->MergeFrom(CreateEventProto(kIds[i], i));
      }
      configuration_->AddRequest(std::move(request));
    } else {
      base::Value::Dict context;
      context.SetByDottedPath("browser.userAgent", "dummyAgent");
      base::Value::List events;
      for (size_t i = 0; i < kIds.size(); ++i) {
        base::Value::Dict event = CreateEvent(kIds[i], i);
        events.Append(std::move(event));
      }

      base::Value::Dict report = RealtimeReportingJobConfiguration::BuildReport(
          std::move(events), std::move(context));
      configuration_->AddReportDeprecated(std::move(report));
    }
  }

  bool use_proto_format() { return GetParam(); }

 protected:
  const std::vector<std::string> kIds = {"id1", "id2", "id3"};
  base::HistogramTester histogram_;
  static base::Value::Dict CreateEvent(const std::string& event_id, int type) {
    base::Value::Dict event;
    event.Set(kAppPackage, kPackage);
    event.Set(kEventType, type);
    base::Value::Dict wrapper;
    wrapper.Set(enterprise_connectors::kExtensionInstallEvent,
                std::move(event));
    wrapper.Set(kEventId, event_id);
    return wrapper;
  }

  static ::chrome::cros::reporting::proto::Event CreateEventProto(
      const std::string& event_id,
      int extension_action_type) {
    ::chrome::cros::reporting::proto::Event event;
    event.set_event_id(event_id);
    event.mutable_browser_extension_install_event()->set_name(kExtensionName);
    event.mutable_browser_extension_install_event()->set_id(kExtensionId);
    event.mutable_browser_extension_install_event()->set_extension_action_type(
        static_cast<::chrome::cros::reporting::proto::
                        BrowserExtensionInstallEvent::ExtensionAction>(
            extension_action_type));
    return event;
  }

  static base::Value::Dict CreateResponse(
      const std::set<std::string>& success_ids,
      const std::set<std::string>& failed_ids,
      const std::set<std::string>& permanent_failed_ids) {
    base::Value::Dict response;
    if (!success_ids.empty()) {
      base::Value::List& list =
          response
              .Set(RealtimeReportingJobConfiguration::kUploadedEventsKey,
                   base::Value(base::Value::Type::LIST))
              ->GetList();
      for (const auto& id : success_ids) {
        list.Append(id);
      }
    }
    if (!failed_ids.empty()) {
      base::Value::List& list =
          response
              .Set(RealtimeReportingJobConfiguration::kFailedUploadsKey,
                   base::Value(base::Value::Type::LIST))
              ->GetList();
      for (const auto& id : failed_ids) {
        base::Value::Dict failure;
        failure.Set(kEventId, id);
        failure.Set(kStatusCode, 8 /* RESOURCE_EXHAUSTED */);
        list.Append(std::move(failure));
      }
    }
    if (!permanent_failed_ids.empty()) {
      base::Value::List& list =
          response
              .Set(
                  RealtimeReportingJobConfiguration::kPermanentFailedUploadsKey,
                  base::Value(base::Value::Type::LIST))
              ->GetList();
      for (const auto& id : permanent_failed_ids) {
        base::Value::Dict failure;
        failure.Set(kEventId, id);
        failure.Set(kStatusCode, 9 /* FAILED_PRECONDITION */);
        list.Append(std::move(failure));
      }
    }

    return response;
  }

  static std::string CreateResponseString(const base::Value::Dict& response) {
    std::string response_string;
    base::JSONWriter::Write(response, &response_string);
    return response_string;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService service_{&job_creation_handler_};
  MockCloudPolicyClient client_;
  StrictMock<MockCallbackObserver> callback_observer_;
  DeviceManagementService::Job job_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  class ScopedFakeSerialNumber {
   public:
    explicit ScopedFakeSerialNumber(
        ash::system::ScopedFakeStatisticsProvider* fake_statistics_provider) {
      // The fake serial number must be set before |configuration_| is
      // constructed below.
      fake_statistics_provider->SetMachineStatistic(
          ash::system::kSerialNumberKey, "fake_serial_number");
    }
  };
  ScopedFakeSerialNumber fake_serial_number_;
#endif
  std::unique_ptr<RealtimeReportingJobConfiguration> configuration_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(RealtimeReportingJobConfigurationTest, ValidatePayload) {
  if (use_proto_format()) {
    // If using the proto format, validate the request.
    ::chrome::cros::reporting::proto::UploadEventsRequest request;
    request.ParseFromString(configuration_->GetPayload());
    EXPECT_EQ(kDummyToken, request.device().dm_token());
    EXPECT_EQ(client_.client_id(), request.device().client_id());
    EXPECT_EQ(GetOSUsername(), request.browser().machine_user());
    EXPECT_EQ(version_info::GetVersionNumber(),
              request.browser().chrome_version());
    EXPECT_EQ(GetOSPlatform(), request.device().os_platform());
    EXPECT_EQ(GetOSVersion(), request.device().os_version());
    EXPECT_FALSE(GetDeviceName().empty());
    EXPECT_EQ(GetDeviceName(), request.device().name());
    EXPECT_FALSE(GetDeviceFqdn().empty());
    EXPECT_EQ(GetDeviceFqdn(), request.device().device_fqdn());
    EXPECT_EQ(GetNetworkName(), request.device().network_name());

    EXPECT_EQ(kIds.size(), base::checked_cast<size_t>(request.events_size()));
    int i = -1;
    for (const auto& event : request.events()) {
      EXPECT_EQ(kIds[++i], event.event_id());
      auto extension_event = event.browser_extension_install_event();
      EXPECT_EQ(kExtensionName, extension_event.name());
      EXPECT_EQ(kExtensionId, extension_event.id());
      EXPECT_EQ(
          static_cast<::chrome::cros::reporting::proto::
                          BrowserExtensionInstallEvent::ExtensionAction>(i),
          extension_event.extension_action_type());
    }
  } else {
    std::optional<base::Value> payload =
        base::JSONReader::Read(configuration_->GetPayload());
    EXPECT_TRUE(payload.has_value());
    const base::Value::Dict& payload_dict = payload->GetDict();
    EXPECT_EQ(kDummyToken, *payload_dict.FindStringByDottedPath(
                               ReportingJobConfigurationBase::
                                   DeviceDictionaryBuilder::GetDMTokenPath()));
    EXPECT_EQ(client_.client_id(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetClientIdPath()));
    EXPECT_EQ(GetOSUsername(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                      GetMachineUserPath()));
    EXPECT_EQ(version_info::GetVersionNumber(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                      GetChromeVersionPath()));
    EXPECT_EQ(GetOSPlatform(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSPlatformPath()));
    EXPECT_EQ(GetOSVersion(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSVersionPath()));
    EXPECT_FALSE(GetDeviceName().empty());
    EXPECT_EQ(GetDeviceName(), *payload_dict.FindStringByDottedPath(
                                   ReportingJobConfigurationBase::
                                       DeviceDictionaryBuilder::GetNamePath()));
    EXPECT_FALSE(GetDeviceFqdn().empty());
    EXPECT_EQ(GetDeviceFqdn(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetDeviceFqdnPath()));
    EXPECT_EQ(GetNetworkName(),
              *payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetNetworkNamePath()));

    base::Value::List* events = payload->GetDict().FindList(
        RealtimeReportingJobConfiguration::kEventListKey);
    EXPECT_EQ(kIds.size(), events->size());
    int i = -1;
    for (const base::Value& event_val : *events) {
      const base::Value::Dict& event = event_val.GetDict();
      const std::string& id = CHECK_DEREF(event.FindString(kEventId));
      EXPECT_EQ(kIds[++i], id);
      const std::optional<int> type =
          event.FindDict(enterprise_connectors::kExtensionInstallEvent)
              ->FindInt(kEventType);
      ASSERT_TRUE(type.has_value());
      EXPECT_EQ(i, *type);
    }
  }
}

TEST_P(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_Success) {
  base::Value::Dict response =
      CreateResponse(/*success_ids=*/{kIds[0], kIds[1], kIds[2]},
                     /*failed_ids=*/{}, /*permanent_failed_ids=*/{});
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_SUCCESS,
                                DeviceManagementService::kSuccess,
                                testing::Eq(testing::ByRef(response))));
  configuration_->OnURLLoadComplete(&job_, net::OK,
                                    DeviceManagementService::kSuccess,
                                    CreateResponseString(response));
}

TEST_P(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_NetError) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_REQUEST_FAILED, _,
                                testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(&job_, net::ERR_CONNECTION_RESET,
                                    0 /* ignored */, "");
}

TEST_P(RealtimeReportingJobConfigurationTest,
       OnURLLoadComplete_InvalidRequest) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_REQUEST_INVALID,
                                DeviceManagementService::kInvalidArgument,
                                testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kInvalidArgument, "");
}

TEST_P(RealtimeReportingJobConfigurationTest,
       OnURLLoadComplete_InvalidDMToken) {
  EXPECT_CALL(
      callback_observer_,
      OnURLLoadComplete(&job_, DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
                        DeviceManagementService::kInvalidAuthCookieOrDMToken,
                        testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kInvalidAuthCookieOrDMToken, "");
}

TEST_P(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_NotSupported) {
  EXPECT_CALL(
      callback_observer_,
      OnURLLoadComplete(&job_, DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
                        DeviceManagementService::kDeviceManagementNotAllowed,
                        testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kDeviceManagementNotAllowed, "");
}

TEST_P(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_TempError) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_TEMPORARY_UNAVAILABLE,
                                DeviceManagementService::kServiceUnavailable,
                                testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kServiceUnavailable, "");
}

TEST_P(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_UnknownError) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_HTTP_STATUS_ERROR,
                                DeviceManagementService::kInvalidURL,
                                testing::Eq(std::nullopt)));
  configuration_->OnURLLoadComplete(&job_, net::OK,
                                    DeviceManagementService::kInvalidURL, "");
}

TEST_P(RealtimeReportingJobConfigurationTest, ShouldRetry_Success) {
  auto response_string =
      CreateResponseString(CreateResponse({kIds[0], kIds[1], kIds[2]}, {}, {}));
  auto should_retry = configuration_->ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::NO_RETRY, should_retry);
}

TEST_P(RealtimeReportingJobConfigurationTest, ShouldRetry_PartialFalure) {
  // Batch failures are retried
  auto response_string =
      CreateResponseString(CreateResponse({kIds[0], kIds[1]}, {kIds[2]}, {}));
  auto should_retry = configuration_->ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::RETRY_WITH_DELAY, should_retry);
}

TEST_P(RealtimeReportingJobConfigurationTest, ShouldRetry_PermanentFailure) {
  // Permanent failures are not retried.
  auto response_string =
      CreateResponseString(CreateResponse({kIds[0], kIds[1]}, {}, {kIds[2]}));
  auto should_retry = configuration_->ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::NO_RETRY, should_retry);
}

TEST_P(RealtimeReportingJobConfigurationTest, ShouldRetry_InvalidResponse) {
  auto should_retry = configuration_->ShouldRetry(
      DeviceManagementService::kSuccess, "some error");
  EXPECT_EQ(DeviceManagementService::Job::NO_RETRY, should_retry);
}

TEST_P(RealtimeReportingJobConfigurationTest, OnBeforeRetry_HttpFailure) {
  // No change should be made to the payload in this case.
  auto original_payload = configuration_->GetPayload();
  configuration_->OnBeforeRetry(DeviceManagementService::kServiceUnavailable,
                                "");
  EXPECT_EQ(original_payload, configuration_->GetPayload());
}

TEST_P(RealtimeReportingJobConfigurationTest, GetPayloadRecordsUmaMetrics) {
  // GetPayload should record the payload size as an UMA metric.
  std::string payload = configuration_->GetPayload();
  histogram_.ExpectUniqueSample(
      enterprise_connectors::kAllUploadSizeUmaMetricName, payload.size(), 1);

  histogram_.ExpectUniqueSample(
      enterprise_connectors::kExtensionInstallUploadSizeUmaMetricName,
      payload.size(), 1);
}

TEST_P(RealtimeReportingJobConfigurationTest, OnBeforeRetry_PartialBatch) {
  // Only those events whose ids are in failed_uploads should be in the payload
  // after the OnBeforeRetry call.
  auto response_string =
      CreateResponseString(CreateResponse(/*success_ids=*/{kIds[0]},
                                          /*failed_ids=*/{kIds[1]},
                                          /*permanent_failed_ids=*/{kIds[2]}));
  configuration_->OnBeforeRetry(DeviceManagementService::kSuccess,
                                response_string);
  if (GetParam()) {
    // If using the proto format, validate the request.
    ::chrome::cros::reporting::proto::UploadEventsRequest request;
    request.ParseFromString(configuration_->GetPayload());
    EXPECT_EQ(1, request.events_size());
    EXPECT_EQ(kIds[1], request.events(0).event_id());
  } else {
    // If using the JSON format, validate the request.
    std::optional<base::Value> payload =
        base::JSONReader::Read(configuration_->GetPayload());
    base::Value::List* events = payload->GetDict().FindList(
        RealtimeReportingJobConfiguration::kEventListKey);
    EXPECT_EQ(1u, events->size());
    auto& event = (*events)[0];
    EXPECT_EQ(kIds[1], *event.GetDict().FindString(kEventId));
  }
}

TEST_P(RealtimeReportingJobConfigurationTest, OnBeforeRetry_InvalidResponse) {
  // No change should be made to the payload in this case.
  auto original_payload = configuration_->GetPayload();
  configuration_->OnBeforeRetry(DeviceManagementService::kSuccess, "error");
  EXPECT_EQ(original_payload, configuration_->GetPayload());
}

INSTANTIATE_TEST_SUITE_P(,
                         RealtimeReportingJobConfigurationTest,
                         ::testing::Bool());
}  // namespace policy
