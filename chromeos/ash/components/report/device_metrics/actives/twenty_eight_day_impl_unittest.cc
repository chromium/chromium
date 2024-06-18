// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/actives/twenty_eight_day_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/report/device_metrics/use_case/stub_psm_client_manager.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/psm_utils.h"
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

namespace {

// PSM use case enum for 28-day-active use case.
constexpr psm_rlwe::RlweUseCase kPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_28DAY_ACTIVE;

// Enum classifies which responses should be returned for check membership
// responses.
// First phase queries 3 PSM ID's. [<day_0>, .., <day_26>, <day_27>]
// Second phase queries 1 PSM ID using binary search: [day_0, <day_x>, day_27].
enum class MembershipResponseTestCase {
  kUnknown = 0,  // Default value, typically this state should not be set.
  kFirstPhaseNegative = 1,                  // [F, F, F]
  kFirstPhasePositiveNegativeNegative = 2,  // [T, F, F]
  kFirstPhasePositivePositiveNegative = 3,  // [T, T, F]
  kFirstPhasePositive = 4,                  // [T, T, T]
  kSecondPhaseNegative = 5,                 // [F]
  kSecondPhasePositive = 6,                 // [T]
  kMaxValue = kSecondPhasePositive,
};

}  // namespace

class TwentyEightDayImplBase : public testing::Test {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  TwentyEightDayImplBase() = default;
  TwentyEightDayImplBase(const TwentyEightDayImplBase&) = delete;
  TwentyEightDayImplBase& operator=(const TwentyEightDayImplBase&) = delete;
  ~TwentyEightDayImplBase() override = default;

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

  base::test::ScopedFeatureList& GetScopedFeatureList() {
    return scoped_feature_list_;
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
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

class TwentyEightDayImplDirectCheckIn : public TwentyEightDayImplBase {
 public:
  void SetUp() override {
    GetScopedFeatureList().InitWithFeatures(
        {},
        /*disabled_features*/ {
            features::kDeviceActiveClient28DayActiveCheckMembership});

    TwentyEightDayImplBase::SetUp();

    // |psm_client_delegate| is owned by |psm_client_manager_|.
    // Stub successful request payloads when created by the PSM client.
    std::unique_ptr<StubPsmClientManagerDelegate> psm_client_delegate =
        std::make_unique<StubPsmClientManagerDelegate>();
    psm_client_manager_ =
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate));

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        psm_client_manager_.get());
    twenty_eight_day_impl_ =
        std::make_unique<TwentyEightDayImpl>(use_case_params_.get());
  }

  void TearDown() override {
    twenty_eight_day_impl_.reset();
    use_case_params_.reset();
    psm_client_manager_.reset();
  }

  TwentyEightDayImpl* GetTwentyEightDayImpl() {
    return twenty_eight_day_impl_.get();
  }

  base::Time GetLastPingTimestamp() {
    return twenty_eight_day_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    twenty_eight_day_impl_->SetLastPingTimestamp(ts);
  }

  std::optional<FresnelImportDataRequest>
  GenerateImportRequestBodyForTesting() {
    return twenty_eight_day_impl_->GenerateImportRequestBody();
  }

 private:
  std::unique_ptr<PsmClientManager> psm_client_manager_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<TwentyEightDayImpl> twenty_eight_day_impl_;
};

TEST_F(TwentyEightDayImplDirectCheckIn, QueryFeatureFlagDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      features::kDeviceActiveClient28DayActiveCheckMembership));
}

TEST_F(TwentyEightDayImplDirectCheckIn, GracefullyHandleImportResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Set invalid import response body.
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneDay) {
  // Set last ping ts to be 1 day before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(1));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 1);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneWeek) {
  // Set last ping ts to be 7 days before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(7));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 7);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230122");
  EXPECT_EQ(request_body->import_data().at(6).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneMonth) {
  // Set last ping ts to be 1 month before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(31));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 28);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230101");
  EXPECT_EQ(request_body->import_data().at(27).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

class TwentyEightDayImplDirectCheckMembership : public TwentyEightDayImplBase {
 public:
  void SetUp() override {
    GetScopedFeatureList().InitWithFeatures(
        /*enabled_features=*/
        {features::kDeviceActiveClient28DayActiveCheckMembership},
        /*disabled_features*/ {});

    TwentyEightDayImplBase::SetUp();

    // |psm_client_delegate| is owned by |psm_client_manager_|.
    // Stub successful request payloads when created by the PSM client.
    std::unique_ptr<StubPsmClientManagerDelegate> psm_client_delegate =
        std::make_unique<StubPsmClientManagerDelegate>();
    psm_client_delegate_ = psm_client_delegate.get();
    SimulateOprfRequest(psm_client_delegate_,
                        psm_rlwe::PrivateMembershipRlweOprfRequest());
    SimulateQueryRequest(psm_client_delegate_,
                         psm_rlwe::PrivateMembershipRlweQueryRequest());
    psm_client_manager_ =
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate));

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        psm_client_manager_.get());
    twenty_eight_day_impl_ =
        std::make_unique<TwentyEightDayImpl>(use_case_params_.get());
  }

  void TearDown() override {
    twenty_eight_day_impl_.reset();

    // Dangling pointer must be set to null before destructing use case params.
    psm_client_delegate_ = nullptr;

    use_case_params_.reset();
    psm_client_manager_.reset();
  }

  TwentyEightDayImpl* GetTwentyEightDayImpl() {
    return twenty_eight_day_impl_.get();
  }

  base::Value::Dict* GetActivesCache() {
    return &twenty_eight_day_impl_->actives_cache_;
  }

  psm_rlwe::RlweMembershipResponses::MembershipResponseEntry
  GetStubMembershipResponseEntry(bool is_positive, const std::string& date) {
    // Create a stub entry used for stubbing data.
    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry entry;

    private_membership::MembershipResponse* response =
        entry.mutable_membership_response();
    response->set_is_member(is_positive);

    psm_rlwe::RlwePlaintextId stub_plaintext_id =
        utils::GeneratePsmIdentifier(utils::kFakeHighEntropySeed,
                                     psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                     date)
            .value();
    stub_plaintext_id.set_non_sensitive_id(date);

    *entry.mutable_plaintext_id() = stub_plaintext_id;
    return entry;
  }

  // Return membership response based on the |MembershipResponseTestCase|.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses(
      MembershipResponseTestCase test_case,
      const std::vector<psm_rlwe::RlwePlaintextId>& query_ids) {
    psm_rlwe::RlweMembershipResponses membership_responses;

    switch (test_case) {
      case MembershipResponseTestCase::kUnknown:
        ADD_FAILURE() << "Invalid Membership Response test case.";
        break;
      case MembershipResponseTestCase::kFirstPhaseNegative:  // [F, F, F]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(0).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(1).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(2).non_sensitive_id()));
        break;
      case MembershipResponseTestCase::
          kFirstPhasePositiveNegativeNegative:  // [T, F, F]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(0).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(1).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(2).non_sensitive_id()));
        break;
      case MembershipResponseTestCase::
          kFirstPhasePositivePositiveNegative:  // [T, T, F]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(0).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(1).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(2).non_sensitive_id()));
        break;
      case MembershipResponseTestCase::kFirstPhasePositive:  // [T, T, T]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(0).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(1).non_sensitive_id()));
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(2).non_sensitive_id()));
        break;
      case MembershipResponseTestCase::kSecondPhaseNegative:  // [F]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(false,
                                           query_ids.at(0).non_sensitive_id()));
        break;
      case MembershipResponseTestCase::kSecondPhasePositive:  // [T]
        membership_responses.mutable_membership_responses()->Add(
            GetStubMembershipResponseEntry(true,
                                           query_ids.at(0).non_sensitive_id()));
        break;
    }

    return membership_responses;
  }

  base::Time GetLastPingTimestamp() const {
    return twenty_eight_day_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    twenty_eight_day_impl_->SetLastPingTimestamp(ts);
  }

  std::optional<FresnelImportDataRequest> GenerateImportRequestBodyForTesting()
      const {
    return twenty_eight_day_impl_->GenerateImportRequestBody();
  }

  std::vector<psm_rlwe::RlwePlaintextId> GetPsmIdentifiersToQueryPhaseOne()
      const {
    std::vector<psm_rlwe::RlwePlaintextId> query_ids =
        twenty_eight_day_impl_->GetPsmIdentifiersToQueryPhaseOne();
    DCHECK_EQ(query_ids.size(), (size_t)3)
        << "First Phase Query should look for 3 ids.";
    return query_ids;
  }

  std::vector<psm_rlwe::RlwePlaintextId> GetPsmIdentifiersToQueryPhaseTwo()
      const {
    std::vector<psm_rlwe::RlwePlaintextId> query_ids =
        twenty_eight_day_impl_->GetPsmIdentifiersToQueryPhaseTwo();
    DCHECK_EQ(query_ids.size(), (size_t)1)
        << "First Phase Query should look for 1 id.";
    return query_ids;
  }

  bool IsFirstPhaseComplete() const {
    return twenty_eight_day_impl_->IsFirstPhaseComplete();
  }

  bool IsSecondPhaseComplete() const {
    return twenty_eight_day_impl_->IsSecondPhaseComplete();
  }

  base::Time FindLeftMostKnownMembership() const {
    return twenty_eight_day_impl_->FindLeftMostKnownMembership();
  }

  base::Time FindRightMostKnownNonMembership() const {
    return twenty_eight_day_impl_->FindRightMostKnownNonMembership();
  }

  StubPsmClientManagerDelegate* GetPsmClientDelegate() {
    return psm_client_delegate_;
  }

  // Cleans the stored local state prefs back to default settings.
  void SimulatePowerwash() {
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveLastKnown1DayActivePingTimestamp);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveLastKnown28DayActivePingTimestamp);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp);
    GetLocalState()->ClearPref(prefs::kDeviceActiveLastKnownChurnActiveStatus);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1);
    GetLocalState()->ClearPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2);
    GetLocalState()->ClearPref(prefs::kDeviceActive28DayActivePingCache);

    // Update the actives cache object with the newly cleared local state prefs.
    twenty_eight_day_impl_->LoadActivesCachePref();
  }

 private:
  // Maintain pointer to delegate_ in order to stub fake check membership
  // responses. Object lifetime is maintained by |use_case_params_|.
  raw_ptr<StubPsmClientManagerDelegate> psm_client_delegate_;
  std::unique_ptr<PsmClientManager> psm_client_manager_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<TwentyEightDayImpl> twenty_eight_day_impl_;
};

TEST_F(TwentyEightDayImplDirectCheckMembership, QueryFeatureFlagEnabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      features::kDeviceActiveClient28DayActiveCheckMembership));
}

TEST_F(TwentyEightDayImplDirectCheckMembership, BrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Stub the First phase of Check Membership.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateMembershipResponses(
      GetPsmClientDelegate(),
      GetMembershipResponses(
          MembershipResponseTestCase::kFirstPhasePositiveNegativeNegative,
          GetPsmIdentifiersToQueryPhaseOne()));
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  EXPECT_TRUE(IsFirstPhaseComplete());

  // Stub the Second Phase of Check Membership.
  for (int i = 0; i < 4; i++) {
    SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
    SimulateMembershipResponses(
        GetPsmClientDelegate(),
        GetMembershipResponses(MembershipResponseTestCase::kSecondPhaseNegative,
                               GetPsmIdentifiersToQueryPhaseTwo()));
    SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  }
  EXPECT_TRUE(IsSecondPhaseComplete());

  // Stub import data for Check-In.
  SimulateImportResponse(std::string(), net::HTTP_OK);
  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(TwentyEightDayImplDirectCheckMembership, MaxCheckMembershipSent) {
  // TODO(hirthanan): Validate max check membership requests that can occur.
}

TEST_F(TwentyEightDayImplDirectCheckMembership, FirstPhaseNegative) {
  // TODO(hirthanan): If first phase of check membership returns negative,
  // we can assume the device has not pinged for any of the 28 day period.
}

TEST_F(TwentyEightDayImplDirectCheckMembership,
       FirstPhasePositiveNegativeNegative) {
  // TODO(hirthanan): Scenario requires the device to enter the second phase of
  // check membership.
}

TEST_F(TwentyEightDayImplDirectCheckMembership,
       FirstPhasePositivePositiveNegative) {
  // TODO(hirthanan): Scenario will not require second phase of check membership
  // because we know the last ping date.
}

TEST_F(TwentyEightDayImplDirectCheckMembership, FirstPhasePositive) {
  // TODO(hirthanan): Scenario will not require second phase of check membership
  // because we know the last ping date.
}

}  // namespace ash::report::device_metrics
