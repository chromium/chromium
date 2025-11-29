// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/user_security_signals_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

using testing::_;
using testing::Mock;
using testing::Return;

namespace {

constexpr char kReportTriggerMetricName[] =
    "Enterprise.SecurityReport.User.Trigger";
constexpr char kFakePolicyName[] = "TestPolicy";

class MockUserSecuritySignalsServiceDelegate
    : public UserSecuritySignalsService::Delegate {
 public:
  MockUserSecuritySignalsServiceDelegate() = default;
  ~MockUserSecuritySignalsServiceDelegate() = default;

  MOCK_METHOD(void,
              OnReportEventTriggered,
              (SecurityReportTrigger),
              (override));
  MOCK_METHOD(network::mojom::CookieManager*, GetCookieManager, (), (override));
};

net::CanonicalCookie GetTestCookie(const GURL& url, const std::string& name) {
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, /*value=*/"cookie_value", /*domain=*/"." + url.GetHost(),
          /*path=*/"/", /*creation_time=*/base::Time(),
          /*expiration_time=*/base::Time(), /*last_access_time=*/base::Time(),
          /*secure=*/true, /*http_only=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
          /*partition_key=*/std::nullopt, /*status=*/nullptr);
  return *cookie;
}

}  // namespace

class UserSecuritySignalsServiceTest : public testing::Test {
 protected:
  UserSecuritySignalsServiceTest() {
    testing_prefs_.registry()->RegisterBooleanPref(
        kUserSecuritySignalsReporting, false);
    testing_prefs_.registry()->RegisterBooleanPref(
        kUserSecurityAuthenticatedReporting, false);
  }

  void SetUp() override {
    ON_CALL(delegate_, OnReportEventTriggered(_))
        .WillByDefault(
            [&](SecurityReportTrigger) { service_->OnReportUploaded(); });
    ON_CALL(delegate_, GetCookieManager())
        .WillByDefault(Return(&test_cookie_manager_));
  }

  void SetEnabledPolicy(bool enabled) {
    testing_prefs_.SetBoolean(kUserSecuritySignalsReporting, enabled);
  }

  void SetUseAuthPolicy(bool use_auth) {
    testing_prefs_.SetBoolean(kUserSecurityAuthenticatedReporting, use_auth);
  }

  void CreateAndRunSignalsService(bool expect_reporting_enabled = true,
                                  bool expect_using_cookie = false) {
    // Creation of the service with the pref value already enabled will trigger
    // an upload.
    EXPECT_CALL(delegate_, OnReportEventTriggered(_))
        .Times(expect_reporting_enabled ? 1 : 0);

    if (expect_using_cookie) {
      EXPECT_CALL(delegate_, GetCookieManager());
    }

    service_ = std::make_unique<UserSecuritySignalsService>(
        &testing_prefs_, &delegate_, &policy_service_);
    service_->Start();
    task_environment_.RunUntilIdle();

    ASSERT_EQ(service_->IsSecuritySignalsReportingEnabled(),
              expect_reporting_enabled);
    ASSERT_EQ(service_->ShouldUseCookies(), expect_using_cookie);
  }

  void FastForwardTimeToTrigger() {
    task_environment_.FastForwardBy(
        UserSecuritySignalsService::GetSecurityUploadCadence());
  }

  void FastForwardByHalfTimeToTrigger() {
    // Adding a second to remove rounding errors.
    task_environment_.FastForwardBy(
        UserSecuritySignalsService::GetSecurityUploadCadence() / 2 +
        base::Seconds(1));
  }

  void FastForwardCustomTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void TriggerValidCookieInsert() {
    test_cookie_manager_.DispatchCookieChange(net::CookieChangeInfo(
        GetTestCookie(GaiaUrls::GetInstance()->secure_google_url(),
                      GaiaConstants::kGaiaSigninCookieName),
        net::CookieAccessResult(), net::CookieChangeCause::INSERTED));
  }

  void FlushForTesting() {
    if (service_ && service_->cookie_listener_receiver_.is_bound()) {
      service_->cookie_listener_receiver_.FlushForTesting();
    }
  }

  // Note: Cadence unit is hours.
  void OverrideSecurityUploadCadence(base::TimeDelta new_cadence) {
    feature_list_.InitAndEnableFeatureWithParameters(
        enterprise_signals::features::kProfileSignalsReportingEnabled,
        {{enterprise_signals::features::kProfileSignalsReportingInterval.name,
          base::ToString(new_cadence.InHours()) + "h"}});
  }

  void InitializePolicyUpdateReportTrigger(bool enabled) {
    feature_list_.InitWithFeatureState(
        enterprise_signals::features::kPolicyDataCollectionEnabled, enabled);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple testing_prefs_;
  std::unique_ptr<UserSecuritySignalsService> service_ = nullptr;
  testing::StrictMock<MockUserSecuritySignalsServiceDelegate> delegate_;
  network::TestCookieManager test_cookie_manager_;
  policy::MockPolicyService policy_service_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UserSecuritySignalsServiceTest, NotStarted) {
  service_ = std::make_unique<UserSecuritySignalsService>(
      &testing_prefs_, &delegate_, &policy_service_);

  EXPECT_FALSE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_FALSE(service_->ShouldUseCookies());

  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  // Pref values should be available even if the service was not started.
  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_TRUE(service_->ShouldUseCookies());

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
  histogram_tester_.ExpectTotalCount(kReportTriggerMetricName, 0);
}

TEST_F(UserSecuritySignalsServiceTest, PolicyDefault) {
  EXPECT_CALL(delegate_, OnReportEventTriggered(_)).Times(0);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/false);

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
  histogram_tester_.ExpectTotalCount(kReportTriggerMetricName, 0);
}

TEST_F(UserSecuritySignalsServiceTest, PolicyEnabledWithoutCookies) {
  SetEnabledPolicy(true);

  CreateAndRunSignalsService();

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 1);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyCollectionEnabled_PolicyChangeTriggersReport) {
  SetEnabledPolicy(true);
  InitializePolicyUpdateReportTrigger(true);
  policy::PolicyNamespace ns(policy::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyMap previous;
  policy::PolicyMap current;
  current.Set(kFakePolicyName, policy::POLICY_LEVEL_MANDATORY,
              policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
              base::Value(true), nullptr);
  EXPECT_CALL(policy_service_, AddObserver).Times(1);

  CreateAndRunSignalsService();

  base::RunLoop run_loop;
  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kPolicyChange))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  service_->OnPolicyUpdated(ns, previous, current);
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kPolicyChange, 1);
}

TEST_F(
    UserSecuritySignalsServiceTest,
    PolicyCollectionEnabled_PolicyUpdateWithNoPolicyChange_DoesNotTriggersReport) {
  SetEnabledPolicy(true);
  InitializePolicyUpdateReportTrigger(true);
  policy::PolicyNamespace ns(policy::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyMap previous;
  previous.Set(kFakePolicyName, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(true), nullptr);
  policy::PolicyMap current;
  current.Set(kFakePolicyName, policy::POLICY_LEVEL_MANDATORY,
              policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
              base::Value(true), nullptr);
  EXPECT_CALL(policy_service_, AddObserver).Times(1);

  CreateAndRunSignalsService();

  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kPolicyChange))
      .Times(0);

  service_->OnPolicyUpdated(ns, previous, current);

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
}

TEST_F(
    UserSecuritySignalsServiceTest,
    PolicyCollectionEnabled_TogglingSignalsCollectionPolicy_UpdatesPolicyObservation) {
  // Initial start with policy disabled.
  SetEnabledPolicy(false);
  InitializePolicyUpdateReportTrigger(true);
  EXPECT_CALL(policy_service_, AddObserver).Times(0);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/false,
                             /*expect_using_cookie=*/false);

  testing::Mock::VerifyAndClearExpectations(&policy_service_);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Enabling the policy should add an observer.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate_, OnReportEventTriggered(_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(policy_service_, AddObserver).Times(1);
  SetEnabledPolicy(true);

  service_->Start();
  run_loop.Run();
}

TEST_F(
    UserSecuritySignalsServiceTest,
    PolicyCollectionDisabled_TogglingSignalsCollectionPolicy_DoesNotUpdatePolicyObservation) {
  // Initial start with policy disabled.
  SetEnabledPolicy(false);
  InitializePolicyUpdateReportTrigger(false);
  EXPECT_CALL(policy_service_, AddObserver).Times(0);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/false,
                             /*expect_using_cookie=*/false);

  testing::Mock::VerifyAndClearExpectations(&policy_service_);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Enabling the policy should not add an observer.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate_, OnReportEventTriggered(_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(policy_service_, AddObserver).Times(0);
  SetEnabledPolicy(true);

  service_->Start();
  run_loop.Run();

  testing::Mock::VerifyAndClearExpectations(&policy_service_);
  testing::Mock::VerifyAndClearExpectations(&delegate_);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyCollectionDisabled_PolicyUpdate_DoesNotTriggersReport) {
  SetEnabledPolicy(true);
  InitializePolicyUpdateReportTrigger(false);
  policy::PolicyNamespace ns(policy::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyMap previous;
  policy::PolicyMap current;
  current.Set(kFakePolicyName, policy::POLICY_LEVEL_MANDATORY,
              policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
              base::Value(true), nullptr);
  EXPECT_CALL(policy_service_, AddObserver).Times(0);

  CreateAndRunSignalsService();

  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kPolicyChange))
      .Times(0);

  service_->OnPolicyUpdated(ns, previous, current);

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
}

TEST_F(UserSecuritySignalsServiceTest, PolicyEnabledWithCookies_FastForwards) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/true,
                             /*expect_using_cookie=*/true);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forwarding should trigger another upload.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardTimeToTrigger();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forwarding again should trigger another upload.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardTimeToTrigger();

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 3);
}

// Test case to simulate when a security signals report is uploaded by a
// different service. For example, Chrome profile reporting can send a full
// report with security signals on its own schedule.
TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_ExternalTriggerDelays) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/true,
                             /*expect_using_cookie=*/true);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Signaling that a report was uploaded after waiting a halftime means waiting
  // another halftime should not result in a second report being triggered.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(0);
  FastForwardByHalfTimeToTrigger();
  service_->OnReportUploaded();
  FastForwardByHalfTimeToTrigger();

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 1);
}

TEST_F(UserSecuritySignalsServiceTest, PolicyBecomesEnabledWithoutCookies) {
  CreateAndRunSignalsService(/*expect_reporting_enabled=*/false);
  EXPECT_FALSE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(0);

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
  Mock::VerifyAndClearExpectations(&delegate_);

  // A report should be triggered when the policy becomes enabled.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  SetEnabledPolicy(true);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 1);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_NullCookieManager) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  // A upload should occur first on service creation.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);

  // Having a broken cookie manager should not cause crashes.
  EXPECT_CALL(delegate_, GetCookieManager()).WillOnce(Return(nullptr));

  service_ = std::make_unique<UserSecuritySignalsService>(
      &testing_prefs_, &delegate_, &policy_service_);
  service_->Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_TRUE(service_->ShouldUseCookies());

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 1);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_ValidCookieAdded) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/true,
                             /*expect_using_cookie=*/true);

  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kCookieChange))
      .Times(1);

  // Fake that the first-party authentication cookie was inserted for the first
  // time.
  TriggerValidCookieInsert();
  FlushForTesting();

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kCookieChange, 1);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_ValidCookieUpdated) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/true,
                             /*expect_using_cookie=*/true);

  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kCookieChange))
      .Times(1);

  // Fake that the first-party authentication cookie was updated. When there
  // already is an existing cookie, this triggers two events:
  // - An overwrite with the old cookie,
  // - A insert with the new cookie.
  test_cookie_manager_.DispatchCookieChange(net::CookieChangeInfo(
      GetTestCookie(GaiaUrls::GetInstance()->secure_google_url(),
                    GaiaConstants::kGaiaSigninCookieName),
      net::CookieAccessResult(), net::CookieChangeCause::OVERWRITE));

  TriggerValidCookieInsert();
  FlushForTesting();

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kCookieChange, 1);
}

TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_DifferentCookieEvents) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  CreateAndRunSignalsService(/*expect_reporting_enabled=*/true,
                             /*expect_using_cookie=*/true);

  // Fake that various cookie change events were triggered, none with "INSERT" /
  // "INSERTED_NO_VALUE_CHANGE_OVERWRITE". This means no additional report
  // should be triggered.
  const auto kCases = std::to_array<net::CookieChangeCause>({
      net::CookieChangeCause::EXPLICIT,
      net::CookieChangeCause::UNKNOWN_DELETION,
      net::CookieChangeCause::OVERWRITE,
      net::CookieChangeCause::EXPIRED,
      net::CookieChangeCause::EVICTED,
      net::CookieChangeCause::EXPIRED_OVERWRITE,
      net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE,
  });
  for (const auto& cause : kCases) {
    test_cookie_manager_.DispatchCookieChange(net::CookieChangeInfo(
        GetTestCookie(GaiaUrls::GetInstance()->secure_google_url(),
                      GaiaConstants::kGaiaSigninCookieName),
        net::CookieAccessResult(), cause));
  }

  // Fake an insert event for unrelated cookies.
  test_cookie_manager_.DispatchCookieChange(net::CookieChangeInfo(
      GetTestCookie(GaiaUrls::GetInstance()->secure_google_url(),
                    "OTHER_1P_COOKIE"),
      net::CookieAccessResult(), net::CookieChangeCause::INSERTED));
  test_cookie_manager_.DispatchCookieChange(net::CookieChangeInfo(
      GetTestCookie(GURL("https://example.com"),
                    GaiaConstants::kGaiaSigninCookieName),
      net::CookieAccessResult(), net::CookieChangeCause::INSERTED));

  FlushForTesting();

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kCookieChange, 2);
}

TEST_F(UserSecuritySignalsServiceTest,
       DelayedAuthPolicy_AffectsCookieWatching) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(false);

  CreateAndRunSignalsService();

  // Auth policy is not enabled, so this should not trigger any report.
  TriggerValidCookieInsert();
  FlushForTesting();

  // Turning the policy on, which will trigger a report when the cookie changes.
  EXPECT_CALL(delegate_, GetCookieManager());
  SetUseAuthPolicy(true);
  EXPECT_CALL(delegate_,
              OnReportEventTriggered(SecurityReportTrigger::kCookieChange))
      .Times(1);
  TriggerValidCookieInsert();
  FlushForTesting();

  // Turn it back off, should again not trigger an update.
  SetUseAuthPolicy(false);
  TriggerValidCookieInsert();
  FlushForTesting();

  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kTimer, 1);
  histogram_tester_.ExpectBucketCount(kReportTriggerMetricName,
                                      SecurityReportTrigger::kCookieChange, 1);
}

// Test that verifies if the feature flag param overrides the upload cadence
// correctly.
TEST_F(UserSecuritySignalsServiceTest, FlagOverrideCadence) {
  auto default_security_upload_cadence =
      UserSecuritySignalsService::GetSecurityUploadCadence();

  // Overwrite the feature flag parameter to double the upload interval.
  OverrideSecurityUploadCadence(default_security_upload_cadence * 2);

  SetEnabledPolicy(true);

  CreateAndRunSignalsService();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forwarding should not trigger another upload, since the internval is
  // overwritten.
  EXPECT_CALL(delegate_, OnReportEventTriggered(_)).Times(0);
  FastForwardCustomTime(default_security_upload_cadence);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forwarding again should trigger another upload.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardCustomTime(default_security_upload_cadence);

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 2);
}

// Test that verifies if the feature flag param overrides the upload cadence
// correctly, even when the timer has already started. Note that this affects
// the timer on next upload while the currently running timer is unaffected.
TEST_F(UserSecuritySignalsServiceTest, FlagOverrideCadenceWhileRunning) {
  auto default_security_upload_cadence =
      UserSecuritySignalsService::GetSecurityUploadCadence();

  SetEnabledPolicy(true);

  CreateAndRunSignalsService();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forwarding should trigger another upload, since the interval is still
  // at default value.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardCustomTime(default_security_upload_cadence);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Overwrite the feature flag parameter to double the upload interval.
  OverrideSecurityUploadCadence(default_security_upload_cadence * 2);

  // Fast forwarding should trigger another upload because the timer started
  // before we overwrite the interval, so it would be using the old value (i.e
  // not doubled).
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardCustomTime(default_security_upload_cadence);
  Mock::VerifyAndClearExpectations(&delegate_);

  // No upload trigger on first fast forward because interval is already
  // doubled now.
  EXPECT_CALL(delegate_, OnReportEventTriggered(_)).Times(0);
  FastForwardCustomTime(default_security_upload_cadence);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Fast forward again should trigger an upload since we reached the new,
  // doubled upload deadline.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardCustomTime(default_security_upload_cadence);

  histogram_tester_.ExpectUniqueSample(kReportTriggerMetricName,
                                       SecurityReportTrigger::kTimer, 4);
}

}  // namespace enterprise_reporting
