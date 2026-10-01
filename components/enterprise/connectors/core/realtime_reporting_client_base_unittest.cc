// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

using Event = ::chrome::cros::reporting::proto::Event;
using ::testing::_;

constexpr char kDeviceDmToken[] = "device-dm-token";
constexpr char kRotatedDeviceDmToken[] = "rotated-device-dm-token";
constexpr char kBrowserClientId[] = "browser-client-id";
#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kProfileClientId[] = "profile-client-id";
#endif

// Minimal RealtimeReportingClientBase implementation that lets tests drive the
// cloud policy client management logic directly.
class TestRealtimeReportingClient : public RealtimeReportingClientBase {
 public:
  TestRealtimeReportingClient(
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : RealtimeReportingClientBase(device_management_service,
                                    url_loader_factory),
        device_management_service_for_testing_(device_management_service) {}

  ~TestRealtimeReportingClient() override = default;

  using RealtimeReportingClientBase::GetReportingClient;

  const std::vector<policy::CloudPolicyClient*>& client_errors() const {
    return client_errors_;
  }

  // policy::CloudPolicyClient::Observer:
  void OnClientError(policy::CloudPolicyClient* client) override {
    client_errors_.push_back(client);
  }

  base::WeakPtr<RealtimeReportingClientBase> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  std::optional<ReportingSettings> GetReportingSettings() override {
    return std::nullopt;
  }

  std::string GetProfileUserName() override { return "user@example.com"; }

  std::string GetProfileIdentifier() override { return "profile-identifier"; }

  std::string GetContentAreaAccountEmail(const GURL& url) override {
    return std::string();
  }

  bool ShouldIncludeDeviceInfo(bool per_profile) override {
    return !per_profile;
  }

 protected:
#if !BUILDFLAG(IS_CHROMEOS)
  std::pair<std::string, policy::CloudPolicyClient*> InitProfileReportingClient(
      const std::string& dm_token) override {
    SetOwnedReportingClient(
        /*per_profile=*/true,
        std::make_unique<policy::CloudPolicyClient>(
            device_management_service_for_testing_, nullptr,
            policy::CloudPolicyClient::DeviceDMTokenCallback()));
    policy::CloudPolicyClient* client =
        GetOwnedReportingClient(/*per_profile=*/true);
    client->SetupRegistration(dm_token, kProfileClientId,
                              /*user_affiliation_ids=*/{});

    return {GetProfilePolicyClientDescription(), client};
  }
#endif

  std::string GetBrowserClientId() override { return kBrowserClientId; }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void MaybeCollectDeviceSignalsAndReportEvent(
      Event event,
      policy::CloudPolicyClient* client,
      const ReportingSettings& settings) override {
    UploadSecurityEvent(std::move(event), client, settings);
  }
#endif

  void UploadCallback(
      EnterpriseReportingEventType event_type,
      base::TimeTicks upload_started_at,
      policy::CloudPolicyClient::Result upload_result) override {}

  ::chrome::cros::reporting::proto::UploadEventsRequest
  CreateUploadEventsRequest() override {
    return ::chrome::cros::reporting::proto::UploadEventsRequest();
  }

 private:
  raw_ptr<policy::DeviceManagementService>
      device_management_service_for_testing_;
  std::vector<policy::CloudPolicyClient*> client_errors_;
  base::WeakPtrFactory<TestRealtimeReportingClient> weak_factory_{this};
};

class RealtimeReportingClientBaseTest : public testing::Test {
 public:
  RealtimeReportingClientBaseTest()
      : device_management_service_(&job_creation_handler_) {}

  void SetUp() override {
    reporting_client_ = std::make_unique<TestRealtimeReportingClient>(
        &device_management_service_, /*url_loader_factory=*/nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService device_management_service_;
  std::unique_ptr<TestRealtimeReportingClient> reporting_client_;
};

// A scope's client is replaced when its DM token changes, e.g. on rotation.
// Replacing it used to destroy the client while `browser_client_` still pointed
// at it and while it still had this object registered as an observer, so the
// RemoveObserver() call that followed ran on freed memory. See
// crbug.com/552317858.
TEST_F(RealtimeReportingClientBaseTest, ReplacingAScopeClientIsSafe) {
  policy::CloudPolicyClient* client =
      reporting_client_->GetReportingClient(kDeviceDmToken,
                                            /*per_profile=*/false);
  ASSERT_TRUE(client);
  base::WeakPtr<policy::CloudPolicyClient> old_client = client->GetWeakPtr();

  policy::CloudPolicyClient* rotated_client =
      reporting_client_->GetReportingClient(kRotatedDeviceDmToken,
                                            /*per_profile=*/false);
  ASSERT_TRUE(rotated_client);

  // The old client is gone and nothing points at it any more.
  EXPECT_FALSE(old_client);
  EXPECT_EQ(rotated_client->dm_token(), kRotatedDeviceDmToken);
  EXPECT_TRUE(rotated_client->is_registered());
}

// The replacement client must be the one this object observes, otherwise a
// rejected DM token would never reach `rejected_dm_token_timers_`.
TEST_F(RealtimeReportingClientBaseTest, ReplacementClientIsObserved) {
  std::vector<policy::DeviceManagementService::JobForTesting> jobs;
  ON_CALL(job_creation_handler_, OnJobCreation(_))
      .WillByDefault(
          [&jobs](const policy::DeviceManagementService::JobForTesting& job) {
            jobs.push_back(job);
          });

  ASSERT_TRUE(reporting_client_->GetReportingClient(kDeviceDmToken,
                                                    /*per_profile=*/false));

  Event event;
  event.mutable_login_event();
  ReportingSettings settings(kRotatedDeviceDmToken, /*per_profile=*/false);
  reporting_client_->ReportEvent(std::move(event), settings);
  ASSERT_TRUE(base::test::RunUntil([&]() { return jobs.size() == 1u; }));

  policy::CloudPolicyClient* rotated_client =
      reporting_client_->GetReportingClient(kRotatedDeviceDmToken,
                                            /*per_profile=*/false);
  ASSERT_TRUE(rotated_client);

  device_management_service_.SendJobResponseNow(&jobs[0], /*net_error=*/0,
                                                /*response_code=*/403);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return reporting_client_->client_errors().size() == 1u; }));
  EXPECT_THAT(reporting_client_->client_errors(),
              testing::ElementsAre(rotated_client));
}

}  // namespace

}  // namespace enterprise_connectors
