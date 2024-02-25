// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/actives/one_day_impl.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/report/device_metrics/use_case/stub_psm_client_manager.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

class OneDayImplBase : public testing::Test {
 public:
  OneDayImplBase() = default;
  OneDayImplBase(const OneDayImplBase&) = delete;
  OneDayImplBase& operator=(const OneDayImplBase&) = delete;
  ~OneDayImplBase() override = default;

  void SetUp() override {
    // Set the mock time to |kFakeTimeNow|.
    base::Time ts;
    ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
    task_environment_.AdvanceClock(ts - base::Time::Now());

    // Register all related local state prefs.
    ReportController::RegisterPrefs(local_state_.registry());

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
  }

 protected:
  base::Time GetFakeTimeNow() { return base::Time::Now(); }

  PrefService* GetLocalState() { return &local_state_; }

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return test_shared_loader_factory_;
  }

  // Generate a well-formed fake PSM network request and response bodies for
  // testing purposes.
  const std::string GetFresnelOprfResponse() {
    FresnelPsmRlweOprfResponse psm_oprf_response;
    *psm_oprf_response.mutable_rlwe_oprf_response() =
        psm_rlwe::PrivateMembershipRlweOprfResponse();
    return psm_oprf_response.SerializeAsString();
  }

  const std::string GetFresnelQueryResponse() {
    FresnelPsmRlweQueryResponse psm_query_response;
    *psm_query_response.mutable_rlwe_query_response() =
        psm_rlwe::PrivateMembershipRlweQueryResponse();
    return psm_query_response.SerializeAsString();
  }

  void SimulateOprfRequest(
      StubPsmClientManagerDelegate* delegate,
      const psm_rlwe::PrivateMembershipRlweOprfRequest& request) {
    delegate->set_oprf_request(request);
  }

  void SimulateQueryRequest(
      StubPsmClientManagerDelegate* delegate,
      const psm_rlwe::PrivateMembershipRlweQueryRequest& request) {
    delegate->set_query_request(request);
  }

  void SimulateMembershipResponses(
      StubPsmClientManagerDelegate* delegate,
      const private_membership::rlwe::RlweMembershipResponses&
          membership_responses) {
    delegate->set_membership_responses(membership_responses);
  }

  void SimulateOprfResponse(const std::string& serialized_response_body,
                            net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetOprfRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  // Generate a well-formed fake PSM network response body for testing purposes.
  void SimulateQueryResponse(const std::string& serialized_response_body,
                             net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetQueryRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  void SimulateImportResponse(const std::string& serialized_response_body,
                              net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetImportRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

class OneDayImplWithPsmQueryPositive : public OneDayImplBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    OneDayImplBase::SetUp();

    // |psm_client_delegate| is owned by |psm_client_manager_|.
    // Stub successful request payloads when created by the PSM client.
    std::unique_ptr<StubPsmClientManagerDelegate> psm_client_delegate =
        std::make_unique<StubPsmClientManagerDelegate>();
    SimulateOprfRequest(psm_client_delegate.get(),
                        psm_rlwe::PrivateMembershipRlweOprfRequest());
    SimulateQueryRequest(psm_client_delegate.get(),
                         psm_rlwe::PrivateMembershipRlweQueryRequest());
    SimulateMembershipResponses(psm_client_delegate.get(),
                                GetMembershipResponses());
    psm_client_manager_ =
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate));

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        psm_client_manager_.get());
    one_day_impl_ = std::make_unique<OneDayImpl>(use_case_params_.get());
  }

  void TearDown() override {
    one_day_impl_.reset();
    use_case_params_.reset();
    psm_client_manager_.reset();
  }

  OneDayImpl* GetOneDayImpl() { return one_day_impl_.get(); }

  // Returns a single positive membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(true);

    return membership_responses;
  }

  base::Time GetLastPingTimestamp() {
    return one_day_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    one_day_impl_->SetLastPingTimestamp(ts);
  }

 private:
  std::unique_ptr<PsmClientManager> psm_client_manager_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<OneDayImpl> one_day_impl_;
};

TEST_F(OneDayImplWithPsmQueryPositive, ValidateBrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetOneDayImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(OneDayImplWithPsmQueryPositive, ValidateLastPingedPreviousDate) {
  SetLastPingTimestamp(GetFakeTimeNow() - base::Days(1));

  GetOneDayImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Device should of imported for new day.
  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(OneDayImplWithPsmQueryPositive, ValidateLastPingedFutureDate) {
  SetLastPingTimestamp(GetFakeTimeNow() + base::Days(1));

  GetOneDayImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Last ping timestamp should not be updated since it's at a future date.
  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow() + base::Days(1));
}

TEST_F(OneDayImplWithPsmQueryPositive, GracefullyHandleOprfResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetOneDayImpl()->Run(base::DoNothing());

  // Return invalid response body.
  SimulateOprfResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

TEST_F(OneDayImplWithPsmQueryPositive, GracefullyHandleQueryResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetOneDayImpl()->Run(base::DoNothing());

  // Set valid oprf response but set invalid query response body.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

TEST_F(OneDayImplWithPsmQueryPositive, GracefullyHandleImportResponseFailure) {
  SetLastPingTimestamp(GetFakeTimeNow() - base::Days(1));

  GetOneDayImpl()->Run(base::DoNothing());

  // Set invalid import response body.
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow() - base::Days(1));
}

class OneDayImplWithPsmQueryNegative : public OneDayImplBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    OneDayImplBase::SetUp();

    // |psm_client_delegate| is owned by |psm_client_manager_|.
    // Stub successful request payloads when created by the PSM client.
    std::unique_ptr<StubPsmClientManagerDelegate> psm_client_delegate =
        std::make_unique<StubPsmClientManagerDelegate>();
    SimulateOprfRequest(psm_client_delegate.get(),
                        psm_rlwe::PrivateMembershipRlweOprfRequest());
    SimulateQueryRequest(psm_client_delegate.get(),
                         psm_rlwe::PrivateMembershipRlweQueryRequest());
    SimulateMembershipResponses(psm_client_delegate.get(),
                                GetMembershipResponses());
    psm_client_manager_ =
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate));

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        psm_client_manager_.get());
    one_day_impl_ = std::make_unique<OneDayImpl>(use_case_params_.get());
  }

  void TearDown() override {
    one_day_impl_.reset();
    use_case_params_.reset();
    psm_client_manager_.reset();
  }

  OneDayImpl* GetOneDayImpl() { return one_day_impl_.get(); }

  // Returns a single negative membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(false);

    return membership_responses;
  }

  base::Time GetLastPingTimestamp() {
    return one_day_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    one_day_impl_->SetLastPingTimestamp(ts);
  }

 private:
  std::unique_ptr<PsmClientManager> psm_client_manager_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<OneDayImpl> one_day_impl_;
};

TEST_F(OneDayImplWithPsmQueryNegative, ValidateBrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetOneDayImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(OneDayImplWithPsmQueryNegative, GracefullyHandleImportResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetOneDayImpl()->Run(base::DoNothing());

  // Set valid oprf and query responses but set invalid import response body.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

}  // namespace ash::report::device_metrics
