// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/password_protection/password_protection_service.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/common/safe_browsing.mojom-forward.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/safe_browsing/password_protection/mock_password_protection_service.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/verdict_cache_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;

namespace {

const char kFormActionUrl[] = "https://form_action.com/";
const char kPasswordFrameUrl[] = "https://password_frame.com/";
const char kSavedDomain[] = "saved_domain.com";
const char kSavedDomain2[] = "saved_domain2.com";
const char kTargetUrl[] = "http://foo.com/";
const char kUserName[] = "username";

const unsigned int kMinute = 60;
const unsigned int kDay = 24 * 60 * kMinute;

}  // namespace

namespace safe_browsing {

using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD2(CheckCsdWhitelistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class TestPhishingDetector : public mojom::PhishingDetector {
 public:
  TestPhishingDetector() : should_timeout_(false) {}
  ~TestPhishingDetector() override {}

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<mojom::PhishingDetector>(std::move(handle)));
  }

  void StartPhishingDetection(
      const GURL& url,
      StartPhishingDetectionCallback callback) override {
    if (should_timeout_) {
      deferred_callbacks_.push_back(std::move(callback));
    } else {
      ReturnFeatures(url, std::move(callback));
    }
  }
  void ReturnFeatures(const GURL& url,
                      StartPhishingDetectionCallback callback) {
    ClientPhishingRequest verdict;
    verdict.set_is_phishing(false);
    verdict.set_client_score(0.1);
    std::move(callback).Run(mojom::PhishingDetectorResult::SUCCESS,
                            verdict.SerializeAsString());
  }

  void set_should_timeout(bool timeout) { should_timeout_ = timeout; }

 private:
  bool should_timeout_;
  std::vector<StartPhishingDetectionCallback> deferred_callbacks_;
  mojo::Receiver<mojom::PhishingDetector> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestPhishingDetector);
};

class TestPasswordProtectionService : public MockPasswordProtectionService {
 public:
  TestPasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<HostContentSettingsMap> content_setting_map)
      : MockPasswordProtectionService(database_manager,
                                      url_loader_factory,
                                      nullptr),
        cache_manager_(
            std::make_unique<VerdictCacheManager>(nullptr,
                                                  content_setting_map.get())) {}

  void RequestFinished(
      PasswordProtectionRequest* request,
      RequestOutcome outcome,
      std::unique_ptr<LoginReputationClientResponse> response) override {
    latest_request_ = request;
    latest_response_ = std::move(response);
    run_loop_.Quit();
  }

  LoginReputationClientResponse* latest_response() {
    return latest_response_.get();
  }

  void WaitForResponse() { run_loop_.Run(); }

  ~TestPasswordProtectionService() override {}

  size_t GetPendingRequestsCount() { return pending_requests_.size(); }

  const LoginReputationClientRequest* GetLatestRequestProto() {
    return latest_request_ ? latest_request_->request_proto() : nullptr;
  }

  void GetPhishingDetector(
      service_manager::InterfaceProvider* provider,
      mojo::Remote<mojom::PhishingDetector>* phishing_detector) override {
    service_manager::InterfaceProvider::TestApi test_api(provider);
    test_api.SetBinderForName(
        mojom::PhishingDetector::Name_,
        base::BindRepeating(&TestPhishingDetector::Bind,
                            base::Unretained(&test_phishing_detector_)));
    provider->GetInterface(phishing_detector->BindNewPipeAndPassReceiver());
    test_api.ClearBinderForName(mojom::PhishingDetector::Name_);
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    ReusedPasswordAccountType password_type,
                    const LoginReputationClientResponse& verdict,
                    const base::Time& receive_time) override {
    if (!CanGetReputationOfURL(url) || IsIncognito())
      return;

    cache_manager_->CachePhishGuardVerdict(url, trigger_type, password_type,
                                           verdict, receive_time);
  }

  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response) override {
    if (!url.is_valid() || !CanGetReputationOfURL(url))
      return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

    return cache_manager_->GetCachedPhishGuardVerdict(
        url, trigger_type, password_type, out_response);
  }

  int GetStoredVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type) override {
    return cache_manager_->GetStoredPhishGuardVerdictCount(trigger_type);
  }

  void SetDomFeatureCollectionTimeout(bool should_timeout) {
    test_phishing_detector_.set_should_timeout(should_timeout);
  }

 private:
  PasswordProtectionRequest* latest_request_;
  base::RunLoop run_loop_;
  std::unique_ptr<LoginReputationClientResponse> latest_response_;
  TestPhishingDetector test_phishing_detector_;

  // The TestPasswordProtectionService manages its own cache, rather than using
  // the global one.
  std::unique_ptr<VerdictCacheManager> cache_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordProtectionService);
};

class MockPasswordProtectionNavigationThrottle
    : public PasswordProtectionNavigationThrottle {
 public:
  MockPasswordProtectionNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      scoped_refptr<PasswordProtectionRequest> request,
      bool is_warning_showing)
      : PasswordProtectionNavigationThrottle(navigation_handle,
                                             request,
                                             is_warning_showing) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordProtectionNavigationThrottle);
};

class PasswordProtectionServiceTest : public ::testing::TestWithParam<bool> {
 public:
  PasswordProtectionServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  LoginReputationClientResponse CreateVerdictProto(
      LoginReputationClientResponse::VerdictType verdict,
      int cache_duration_sec,
      const std::string& cache_expression) {
    LoginReputationClientResponse verdict_proto;
    verdict_proto.set_verdict_type(verdict);
    verdict_proto.set_cache_duration_sec(cache_duration_sec);
    verdict_proto.set_cache_expression(cache_expression);
    return verdict_proto;
  }

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */,
        false /* migrate_requesting_and_top_level_origin_settings */);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    password_protection_service_ =
        std::make_unique<TestPasswordProtectionService>(
            database_manager_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            content_setting_map_);
    EXPECT_CALL(*password_protection_service_, IsExtendedReporting())
        .WillRepeatedly(Return(GetParam()));
    EXPECT_CALL(*password_protection_service_, IsIncognito())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*password_protection_service_,
                IsURLWhitelistedForPasswordEntry(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*password_protection_service_,
                GetPasswordProtectionWarningTriggerPref(_))
        .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
    url_ = PasswordProtectionService::GetPasswordProtectionRequestUrl();
  }

  void TearDown() override {
    password_protection_service_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  // Sets up |database_manager_| and |pending_requests_| as needed.
  void InitializeAndStartPasswordOnFocusRequest(
      bool match_whitelist,
      int timeout_in_ms,
      content::WebContents* web_contents) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_whitelist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));

    request_ = new PasswordProtectionRequest(
        web_contents, target_url, GURL(kFormActionUrl), GURL(kPasswordFrameUrl),
        kUserName, PasswordType::PASSWORD_TYPE_UNKNOWN, {},
        LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true,
        password_protection_service_.get(), timeout_in_ms);
    request_->Start();
  }

  void InitializeAndStartPasswordEntryRequest(
      PasswordType type,
      const std::vector<std::string>& matching_domains,
      bool match_whitelist,
      int timeout_in_ms,
      content::WebContents* web_contents) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_whitelist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));

    request_ = new PasswordProtectionRequest(
        web_contents, target_url, GURL(), GURL(), kUserName, type,
        matching_domains, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        true, password_protection_service_.get(), timeout_in_ms);
    request_->Start();
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger,
                    ReusedPasswordAccountType password_type,
                    LoginReputationClientResponse::VerdictType verdict,
                    int cache_duration_sec,
                    const std::string& cache_expression,
                    const base::Time& verdict_received_time) {
    ASSERT_FALSE(cache_expression.empty());
    LoginReputationClientResponse response(
        CreateVerdictProto(verdict, cache_duration_sec, cache_expression));
    password_protection_service_->CacheVerdict(url, trigger, password_type,
                                               response, verdict_received_time);
  }

  void CacheInvalidVerdict(ReusedPasswordAccountType password_type) {
    GURL invalid_hostname("http://invalid.com");
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_setting_map_->GetWebsiteSetting(
            invalid_hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
            std::string(), nullptr));

    if (!verdict_dictionary)
      verdict_dictionary = std::make_unique<base::DictionaryValue>();

    std::unique_ptr<base::DictionaryValue> invalid_verdict_entry =
        std::make_unique<base::DictionaryValue>();
    invalid_verdict_entry->SetString("invalid", "invalid_string");

    std::unique_ptr<base::DictionaryValue> invalid_cache_expression_entry =
        std::make_unique<base::DictionaryValue>();
    invalid_cache_expression_entry->SetWithoutPathExpansion(
        "invalid_cache_expression", std::move(invalid_verdict_entry));
    verdict_dictionary->SetWithoutPathExpansion(
        base::NumberToString(static_cast<std::underlying_type_t<PasswordType>>(
            password_protection_service_
                ->ConvertReusedPasswordAccountTypeToPasswordType(
                    password_type))),
        std::move(invalid_cache_expression_entry));
    content_setting_map_->SetWebsiteSettingDefaultScope(
        invalid_hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
        std::string(), std::move(verdict_dictionary));
  }

  size_t GetStoredVerdictCount(LoginReputationClientRequest::TriggerType type) {
    return password_protection_service_->GetStoredVerdictCount(type);
  }

  std::unique_ptr<content::WebContents> GetWebContents() {
    return base::WrapUnique(content::WebContentsTester::CreateTestWebContents(
        content::WebContents::CreateParams(&browser_context_)));
  }

  void VerifyContentAreaSizeCollection(
      const LoginReputationClientRequest& request) {
    bool should_report_content_size =
        password_protection_service_->IsExtendedReporting() &&
        !password_protection_service_->IsIncognito();
    EXPECT_EQ(should_report_content_size, request.has_content_area_height());
    EXPECT_EQ(should_report_content_size, request.has_content_area_width());
  }

  size_t GetNumberOfNavigationThrottles() {
    return request_ ? request_->throttles_.size() : 0u;
  }

 protected:
  // |task_environment_| is needed here because this test involves both UI and
  // IO threads.
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  GURL url_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestPasswordProtectionService> password_protection_service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  base::HistogramTester histograms_;
  content::TestBrowserContext browser_context_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

TEST_P(PasswordProtectionServiceTest, TestCachePasswordReuseVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  LoginReputationClientResponse out_verdict;
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache a password reuse verdict with a different password type but same
  // origin and cache expression should add a new entry.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, stored verdict count for
  // PASSWORD_REUSE_EVENT should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_P(PasswordProtectionServiceTest, TestCachePasswordReuseVerdictsIncognito) {
  EXPECT_CALL(*password_protection_service_, IsIncognito())
      .WillRepeatedly(Return(true));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Try cache another verdict with the some origin and cache_expression.
  // Verdict count should not increase.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, verdict count should not
  // increase.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_P(PasswordProtectionServiceTest, TestCacheUnfamiliarLoginVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict, stored verdict count for
  // UNFAMILIAR_LOGIN_PAGE should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceTest,
       TestCacheUnfamiliarLoginVerdictsIncognito) {
  EXPECT_CALL(*password_protection_service_, IsIncognito())
      .WillRepeatedly(Return(true));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict. Verdict count should not
  // increase.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // Prepare 4 verdicts of the same origin with different cache expressions,
  // or password type, one is expired, one is not, one is of a different
  // trigger type, and the other is with a different password type.
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute, "test.com/",
               now);
  CacheVerdict(
      GURL("http://test.com/def/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING,
      10 * kMinute, "test.com/def/",
      base::Time::FromDoubleT(now.ToDoubleT() - kDay));  // Yesterday, expired.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  CacheVerdict(GURL("http://test.com/bar/login.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/bar/", now);
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute, "test.com/",
               now);

  ASSERT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  LoginReputationClientResponse actual_verdict;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return SAFE if look up for a URL that matches "test.com" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com/def, but the corresponding verdict is expired.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/def/ghi/index.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return PHISHING. Matches "test.com/bar/" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));

  // Now cache SAFE verdict for the full path.
  CacheVerdict(GURL("http://test.com/bar/foo.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/foo.jsp", now);

  // Return SAFE now. Matches the full cache expression.
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));
}

TEST_P(PasswordProtectionServiceTest, TestDoesNotCacheAboutBlank) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);

  // Should not actually cache, since about:blank is not valid for reputation
  // computing.
  CacheVerdict(
      GURL("about:blank"), LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_account_type, LoginReputationClientResponse::SAFE,
      10 * kMinute, "about:blank", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceTest, VerifyCanGetReputationOfURL) {
  // Invalid main frame URL.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(GURL()));

  // Main frame URL scheme is not HTTP or HTTPS.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("data:text/html, <p>hellow")));

  // Main frame URL is a local host.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://localhost:80")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://127.0.0.1")));

  // Main frame URL is a private IP address or anything in an IANA-reserved
  // range.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://192.168.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://10.0.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://[FEED::BEEF]")));

  // Main frame URL is a no-yet-assigned y ICANN gTLD.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://intranet")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://host.intranet.example")));

  // Main frame URL is a dotless domain.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://go/example")));

  // Main frame URL is anything else.
  EXPECT_TRUE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://www.chromium.org")));
}

TEST_P(PasswordProtectionServiceTest, TestNoRequestSentForWhitelistedURL) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://safe.com/"));
  InitializeAndStartPasswordOnFocusRequest(true /* match whitelist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(4 /* MATCHED_WHITELIST */, 1)));
}

// crbug.com/1010007: crashes on win
#if defined(OS_WIN)
#define MAYBE_TestNoRequestSentIfVerdictAlreadyCached \
  DISABLED_TestNoRequestSentIfVerdictAlreadyCached
#else
#define MAYBE_TestNoRequestSentIfVerdictAlreadyCached \
  TestNoRequestSentIfVerdictAlreadyCached
#endif
TEST_P(PasswordProtectionServiceTest,
       MAYBE_TestNoRequestSentIfVerdictAlreadyCached) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  CacheVerdict(GURL(kTargetUrl),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::LOW_REPUTATION, 10 * kMinute,
               GURL(kTargetUrl).host().append("/"), base::Time::Now());
  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(5 /* RESPONSE_ALREADY_CACHED */, 1)));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->latest_response()->verdict_type());
}

TEST_P(PasswordProtectionServiceTest, TestResponseFetchFailed) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up failed response.
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  test_url_loader_factory_.AddResponse(
      url_, network::mojom::URLResponseHead::New(), std::string(), status);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(9 /* FETCH_FAILED */, 1)));
}

TEST_P(PasswordProtectionServiceTest, TestMalformedResponse) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up malformed response.
  test_url_loader_factory_.AddResponse(url_.spec(), "invalid response");
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(10 /* RESPONSE_MALFORMED */, 1)));
}

TEST_P(PasswordProtectionServiceTest, TestRequestTimedout) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/0,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(3 /* TIMEDOUT */, 1)));
}

TEST_P(PasswordProtectionServiceTest,
       TestPasswordOnFocusRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kPasswordOnFocusVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  LoginReputationClientResponse* actual_response =
      password_protection_service_->latest_response();
  EXPECT_EQ(expected_response.verdict_type(), actual_response->verdict_type());
  EXPECT_EQ(expected_response.cache_expression(),
            actual_response->cache_expression());
  EXPECT_EQ(expected_response.cache_duration_sec(),
            actual_response->cache_duration_sec());
}

TEST_P(PasswordProtectionServiceTest,
       TestProtectedPasswordEntryRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kAnyPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initiate a saved password entry request (w/ no sync password).
  AccountInfo account_info;
  account_info.account_id = CoreAccountId("account_id");
  account_info.email = "email";
  account_info.gaia = "gaia";
  EXPECT_CALL(*password_protection_service_, GetSignedInNonSyncAccount(_))
      .WillRepeatedly(Return(account_info));

  InitializeAndStartPasswordEntryRequest(
      PasswordType::OTHER_GAIA_PASSWORD, {"gmail.com"},
      /*match_whitelist=*/false,
      /*timeout_in_ms=*/10000, web_contents.get());
  password_protection_service_->WaitForResponse();

  // UMA: request outcomes
  EXPECT_THAT(
      histograms_.GetAllSamples(kAnyPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  EXPECT_THAT(
      histograms_.GetAllSamples(kNonSyncPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kNonSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));

  // UMA: verdicts
  EXPECT_THAT(histograms_.GetAllSamples(kAnyPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  histograms_.ExpectTotalCount(kSyncPasswordEntryVerdictHistogram, 0);
  EXPECT_THAT(histograms_.GetAllSamples(kNonSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
}

TEST_P(PasswordProtectionServiceTest,
       TestSyncPasswordEntryRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kAnyPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSyncing())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  // Initiate a sync password entry request (w/ no saved password).
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {},
      /*match_whitelist=*/false,
      /*timeout_in_ms=*/10000, web_contents.get());
  password_protection_service_->WaitForResponse();

  // UMA: request outcomes
  EXPECT_THAT(
      histograms_.GetAllSamples(kAnyPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(
      histograms_.GetAllSamples(kSyncPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);

  // UMA: verdicts
  EXPECT_THAT(histograms_.GetAllSamples(kAnyPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryVerdictHistogram, 0);
}

TEST_P(PasswordProtectionServiceTest, TestTearDownWithPendingRequests) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  GURL target_url(kTargetUrl);
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url, _))
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), target_url, GURL("http://foo.com/submit"),
      GURL("http://foo.com/frame"), "username", PasswordType::SAVED_PASSWORD,
      {}, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);

  // Destroy password_protection_service_ while there is one request pending.
  password_protection_service_.reset();
  base::RunLoop().RunUntilIdle();

  // We should not log on TearDown, since that can dispatch calls to pure
  // virtual methods.
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      IsEmpty());
}

TEST_P(PasswordProtectionServiceTest, VerifyPasswordOnFocusRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
            actual_request->trigger_type());
  ASSERT_EQ(2, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_EQ(kPasswordFrameUrl, actual_request->frames(1).url());
  EXPECT_EQ(true, actual_request->frames(1).has_password_field());
  ASSERT_EQ(1, actual_request->frames(1).forms_size());
  EXPECT_EQ(kFormActionUrl, actual_request->frames(1).forms(0).action_url());
  VerifyContentAreaSizeCollection(*actual_request);
}

TEST_P(PasswordProtectionServiceTest,
       VerifyPasswordOnFocusRequestProtoForAllowlistMatch) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  EXPECT_CALL(*password_protection_service_, CanSendSamplePing())
      .WillRepeatedly(Return(true));
  InitializeAndStartPasswordOnFocusRequest(/*match_whitelist=*/true,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  ASSERT_EQ(1, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
}

TEST_P(PasswordProtectionServiceTest,
       VerifySyncPasswordProtectionRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initialize request triggered by chrome sync password reuse.
  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {}, false /* match whitelist */,
      100000 /* timeout in ms*/, web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            actual_request->trigger_type());
  EXPECT_EQ(1, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_TRUE(actual_request->frames(0).has_password_field());
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  const auto& reuse_event = actual_request->password_reuse_event();
  EXPECT_TRUE(reuse_event.is_chrome_signin_password());
  EXPECT_EQ(0, reuse_event.domains_matching_password_size());
  VerifyContentAreaSizeCollection(*actual_request);
}

TEST_P(PasswordProtectionServiceTest,
       VerifyNonSyncPasswordProtectionRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initialize request triggered by saved password reuse.
  InitializeAndStartPasswordEntryRequest(
      PasswordType::SAVED_PASSWORD, {kSavedDomain, kSavedDomain2},
      false /* match whitelist */, 100000 /* timeout in ms*/,
      web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  const auto& reuse_event = actual_request->password_reuse_event();
  EXPECT_FALSE(reuse_event.is_chrome_signin_password());

  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    ASSERT_EQ(2, reuse_event.domains_matching_password_size());
    EXPECT_EQ(kSavedDomain, reuse_event.domains_matching_password(0));
    EXPECT_EQ(kSavedDomain2, reuse_event.domains_matching_password(1));
  } else {
    EXPECT_EQ(0, reuse_event.domains_matching_password_size());
  }
  VerifyContentAreaSizeCollection(*actual_request);
}

TEST_P(PasswordProtectionServiceTest, VerifyShouldShowModalWarning) {
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  AccountInfo account_info;
  account_info.account_id = CoreAccountId("account_id");
  account_info.email = "email";
  account_info.gaia = "gaia";
  EXPECT_CALL(*password_protection_service_, GetSignedInNonSyncAccount(_))
      .WillRepeatedly(Return(account_info));

  // Don't show modal warning if it is not a password reuse ping.
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // Don't show modal warning if it is a saved password reuse and the experiment
  // isn't on.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      safe_browsing::kPasswordProtectionForSignedInUsers);
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSavedPasswords);
    EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type, LoginReputationClientResponse::PHISHING));
    EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type,
        LoginReputationClientResponse::LOW_REPUTATION));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);

    // Don't show modal warning if non-sync gaia account experiment is not
    // on.
    reused_password_account_type.set_account_type(
        ReusedPasswordAccountType::GMAIL);
    reused_password_account_type.set_is_account_syncing(false);
    EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type, LoginReputationClientResponse::PHISHING));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    // Show modal warning if non-sync gaia account experiment is on.
    reused_password_account_type.set_account_type(
        ReusedPasswordAccountType::GMAIL);
    reused_password_account_type.set_is_account_syncing(false);
    EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type, LoginReputationClientResponse::PHISHING));
  }

  // Don't show modal warning if reused password type unknown.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GMAIL);
  reused_password_account_type.set_is_account_syncing(true);
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // For a GSUITE account, don't show warning if password protection is set to
  // off by enterprise policy.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // For a GSUITE account, show warning if password protection is set to
  // PHISHING_REUSE.
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
  EXPECT_EQ(
      PHISHING_REUSE,
      password_protection_service_->GetPasswordProtectionWarningTriggerPref(
          reused_password_account_type));
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // Modal dialog warning is also shown on LOW_REPUTATION verdict.
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type,
      LoginReputationClientResponse::LOW_REPUTATION));

  // Modal dialog warning should not be shown for enterprise password reuse
  // if it is turned off by policy.
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // Show modal warning for enterprise password reuse if the trigger is
  // configured to PHISHING_REUSE.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));
}

TEST_P(PasswordProtectionServiceTest, VerifyContentTypeIsPopulated) {
  LoginReputationClientResponse response =
      CreateVerdictProto(LoginReputationClientResponse::SAFE, 10 * kMinute,
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       response.SerializeAsString());

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  content::WebContentsTester::For(web_contents.get())
      ->SetMainFrameMimeType("application/pdf");

  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());

  password_protection_service_->WaitForResponse();

  EXPECT_EQ(
      "application/pdf",
      password_protection_service_->GetLatestRequestProto()->content_type());
}

TEST_P(PasswordProtectionServiceTest, VerifyIsSupportedPasswordTypeForPinging) {
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  AccountInfo account_info;
  account_info.account_id = CoreAccountId("account_id");
  account_info.email = "email";
  account_info.gaia = "gaia";
  EXPECT_CALL(*password_protection_service_, GetSignedInNonSyncAccount(_))
      .WillRepeatedly(Return(account_info));

  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::SAVED_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  EXPECT_FALSE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::OTHER_GAIA_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::ENTERPRISE_PASSWORD));

  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::SAVED_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::ENTERPRISE_PASSWORD));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    // Only ping for signed in, non-syncing users if the experiment is on.
    EXPECT_FALSE(
        password_protection_service_->IsSupportedPasswordTypeForPinging(
            PasswordType::OTHER_GAIA_PASSWORD));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
        PasswordType::OTHER_GAIA_PASSWORD));
  }
}

TEST_P(PasswordProtectionServiceTest, TestPingsForAboutBlank) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), "username",
      PasswordType::SAVED_PASSWORD, {"example.com"},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 1);
}

TEST_P(PasswordProtectionServiceTest,
       TestVisualFeaturesPopulatedInOnFocusPing) {
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {"example.com"},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();

  bool is_sber = GetParam();
  if (is_sber) {
    password_protection_service_->WaitForResponse();
    ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
    EXPECT_TRUE(password_protection_service_->GetLatestRequestProto()
                    ->has_visual_features());
  }
}

TEST_P(PasswordProtectionServiceTest, TestDomFeaturesPopulated) {
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {"example.com"},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();

  password_protection_service_->WaitForResponse();
  ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
  EXPECT_TRUE(password_protection_service_->GetLatestRequestProto()
                  ->has_dom_features());
}

TEST_P(PasswordProtectionServiceTest, TestDomFeaturesTimeout) {
  password_protection_service_->SetDomFeatureCollectionTimeout(true);
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 10 * kMinute,
                         GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {"example.com"},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  task_environment_.FastForwardUntilNoTasksRemain();

  password_protection_service_->WaitForResponse();
  ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
  EXPECT_FALSE(password_protection_service_->GetLatestRequestProto()
                   ->has_dom_features());
}

TEST_P(PasswordProtectionServiceTest, TestRequestCancelOnTimeout) {
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(true /* match whitelist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  auto throttle = std::make_unique<MockPasswordProtectionNavigationThrottle>(
      nullptr, request_, false);
  EXPECT_EQ(1U, GetNumberOfNavigationThrottles());
  request_->Cancel(true /* timeout */);
  EXPECT_EQ(1U, GetNumberOfNavigationThrottles());
}

TEST_P(PasswordProtectionServiceTest, TestRequestCancelNotOnTimeout) {
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(true /* match whitelist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  auto throttle = std::make_unique<MockPasswordProtectionNavigationThrottle>(
      nullptr, request_, false);
  EXPECT_EQ(1U, GetNumberOfNavigationThrottles());
  request_->Cancel(false /* timeout */);
  EXPECT_EQ(0U, GetNumberOfNavigationThrottles());
}

TEST_P(PasswordProtectionServiceTest, TestWebContentsDestroyed) {
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  web_contents.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

INSTANTIATE_TEST_SUITE_P(Regular,
                         PasswordProtectionServiceTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SBER,
                         PasswordProtectionServiceTest,
                         ::testing::Values(true));

}  // namespace safe_browsing
